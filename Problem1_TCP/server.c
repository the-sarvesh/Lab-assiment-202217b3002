#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define PORT 5566
#define MAX_CLIENTS 20
#define BUFFER_SIZE 1024
#define USERNAME_SIZE 50

typedef struct {
    int socket;
    char username[USERNAME_SIZE];
    int active;
} Client;

static Client clients[MAX_CLIENTS];
static pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

static void get_timestamp(char *buffer, size_t size)
{
    time_t now = time(NULL);
    struct tm local_time;
    localtime_r(&now, &local_time);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", &local_time);
}

static int register_client(int socket, const char *username)
{
    int free_slot = -1;

    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i].active && strcmp(clients[i].username, username) == 0) {
            pthread_mutex_unlock(&clients_mutex);
            return -1;
        }
        if (!clients[i].active && free_slot == -1) {
            free_slot = i;
        }
    }

    if (free_slot >= 0) {
        clients[free_slot].socket = socket;
        snprintf(clients[free_slot].username,
                 sizeof(clients[free_slot].username), "%s", username);
        clients[free_slot].active = 1;
    }
    pthread_mutex_unlock(&clients_mutex);
    return free_slot >= 0 ? 0 : -2;
}

static void remove_client(int socket)
{
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i].active && clients[i].socket == socket) {
            memset(&clients[i], 0, sizeof(clients[i]));
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

static void broadcast_message(const char *message, int excluded_socket)
{
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i].active && clients[i].socket != excluded_socket) {
            send(clients[i].socket, message, strlen(message), MSG_NOSIGNAL);
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

static void *handle_client(void *argument)
{
    int client_socket = *(int *)argument;
    free(argument);

    char buffer[BUFFER_SIZE];
    char username[USERNAME_SIZE] = "";
    char timestamp[32];
    char outgoing[BUFFER_SIZE + 200];

    ssize_t bytes = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    if (bytes <= 0) {
        close(client_socket);
        return NULL;
    }
    buffer[bytes] = '\0';

    if (strncmp(buffer, "REGISTER ", 9) != 0) {
        const char *error = "ERROR: Invalid registration format.\n";
        send(client_socket, error, strlen(error), MSG_NOSIGNAL);
        close(client_socket);
        return NULL;
    }

    snprintf(username, sizeof(username), "%s", buffer + 9);
    username[strcspn(username, "\r\n")] = '\0';
    if (username[0] == '\0') {
        const char *error = "ERROR: Username cannot be empty.\n";
        send(client_socket, error, strlen(error), MSG_NOSIGNAL);
        close(client_socket);
        return NULL;
    }

    int result = register_client(client_socket, username);
    if (result != 0) {
        const char *error = result == -1
                                ? "ERROR: Username already exists.\n"
                                : "ERROR: Server client limit reached.\n";
        send(client_socket, error, strlen(error), MSG_NOSIGNAL);
        close(client_socket);
        return NULL;
    }

    printf("Client joined: %s\n", username);
    snprintf(outgoing, sizeof(outgoing),
             "SYSTEM: %s joined the campus announcement system.\n", username);
    broadcast_message(outgoing, client_socket);

    snprintf(outgoing, sizeof(outgoing),
             "Registration successful. Welcome %s!\n", username);
    send(client_socket, outgoing, strlen(outgoing), MSG_NOSIGNAL);

    while ((bytes = recv(client_socket, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes] = '\0';
        buffer[strcspn(buffer, "\r\n")] = '\0';

        if (strcmp(buffer, "exit") == 0) {
            break;
        }

        if (strcmp(username, "FACULTY") != 0) {
            const char *denied = "You are not authorized to send announcements.\n";
            send(client_socket, denied, strlen(denied), MSG_NOSIGNAL);
            continue;
        }

        if (strncmp(buffer, "ANNOUNCE ", 9) != 0 || buffer[9] == '\0') {
            const char *help = "Use: ANNOUNCE <announcement message>\n";
            send(client_socket, help, strlen(help), MSG_NOSIGNAL);
            continue;
        }

        get_timestamp(timestamp, sizeof(timestamp));
        snprintf(outgoing, sizeof(outgoing),
                 "[%s] ANNOUNCEMENT from %s:\n%s\n",
                 timestamp, username, buffer + 9);
        printf("%s", outgoing);
        broadcast_message(outgoing, -1);
    }

    printf("Client left: %s\n", username);
    remove_client(client_socket);
    snprintf(outgoing, sizeof(outgoing),
             "SYSTEM: %s left the campus announcement system.\n", username);
    broadcast_message(outgoing, client_socket);
    close(client_socket);
    return NULL;
}

int main(void)
{
    signal(SIGPIPE, SIG_IGN);

    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Socket creation failed");
        return EXIT_FAILURE;
    }

    int option = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));

    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);
    server_address.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (bind(server_socket, (struct sockaddr *)&server_address,
             sizeof(server_address)) < 0) {
        perror("Bind failed");
        close(server_socket);
        return EXIT_FAILURE;
    }

    if (listen(server_socket, 10) < 0) {
        perror("Listen failed");
        close(server_socket);
        return EXIT_FAILURE;
    }

    printf("============================================\n");
    printf("     CAMPUS ANNOUNCEMENT TCP SERVER\n");
    printf("============================================\n");
    printf("Server running on 127.0.0.1:%d\n", PORT);
    printf("Waiting for clients...\n\n");

    while (1) {
        int *client_socket = malloc(sizeof(*client_socket));
        if (client_socket == NULL) {
            perror("Memory allocation failed");
            continue;
        }

        *client_socket = accept(server_socket, NULL, NULL);
        if (*client_socket < 0) {
            perror("Accept failed");
            free(client_socket);
            continue;
        }

        pthread_t thread;
        if (pthread_create(&thread, NULL, handle_client, client_socket) != 0) {
            perror("Thread creation failed");
            close(*client_socket);
            free(client_socket);
            continue;
        }
        pthread_detach(thread);
    }
}
