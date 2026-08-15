#include <arpa/inet.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define PORT 5567
#define MAX_CLIENTS 20
#define BUFFER_SIZE 1024
#define USERNAME_SIZE 50

typedef struct {
    struct sockaddr_in address;
    char username[USERNAME_SIZE];
    int active;
} Client;

typedef struct {
    const char *score;
    const char *overs;
    const char *status;
} ScoreUpdate;

static int server_socket;
static Client clients[MAX_CLIENTS];
static pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

static void get_timestamp(char *buffer, size_t size)
{
    time_t now = time(NULL);
    struct tm local_time;
    localtime_r(&now, &local_time);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", &local_time);
}

static int same_client(const struct sockaddr_in *first,
                       const struct sockaddr_in *second)
{
    return first->sin_addr.s_addr == second->sin_addr.s_addr &&
           first->sin_port == second->sin_port;
}

static void register_client(const struct sockaddr_in *address,
                            const char *username)
{
    int slot = -1;

    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i].active && same_client(&clients[i].address, address)) {
            slot = i;
            break;
        }
        if (!clients[i].active && slot == -1) {
            slot = i;
        }
    }

    if (slot >= 0) {
        clients[slot].address = *address;
        snprintf(clients[slot].username,
                 sizeof(clients[slot].username), "%s", username);
        clients[slot].active = 1;
    }
    pthread_mutex_unlock(&clients_mutex);

    if (slot >= 0) {
        char confirmation[BUFFER_SIZE];
        snprintf(confirmation, sizeof(confirmation),
                 "REGISTERED: Welcome %s to live score updates.\n", username);
        sendto(server_socket, confirmation, strlen(confirmation), 0,
               (const struct sockaddr *)address, sizeof(*address));
        printf("Client registered: %s (%s:%d)\n", username,
               inet_ntoa(address->sin_addr), ntohs(address->sin_port));
    }
}

static void *registration_listener(void *argument)
{
    (void)argument;
    char buffer[BUFFER_SIZE];

    while (1) {
        struct sockaddr_in client_address;
        socklen_t address_size = sizeof(client_address);
        ssize_t bytes = recvfrom(server_socket, buffer, sizeof(buffer) - 1, 0,
                                 (struct sockaddr *)&client_address,
                                 &address_size);
        if (bytes <= 0) {
            continue;
        }
        buffer[bytes] = '\0';
        if (strncmp(buffer, "REGISTER ", 9) == 0) {
            char *username = buffer + 9;
            username[strcspn(username, "\r\n")] = '\0';
            register_client(&client_address, username);
        }
    }
    return NULL;
}

static int connected_clients(void)
{
    int count = 0;
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        count += clients[i].active ? 1 : 0;
    }
    pthread_mutex_unlock(&clients_mutex);
    return count;
}

static void broadcast_update(const char *message)
{
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i].active) {
            sendto(server_socket, message, strlen(message), 0,
                   (struct sockaddr *)&clients[i].address,
                   sizeof(clients[i].address));
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

int main(void)
{
    server_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (server_socket < 0) {
        perror("UDP socket creation failed");
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

    printf("============================================\n");
    printf("       UDP LIVE SPORTS SCORE SERVER\n");
    printf("============================================\n");
    printf("Server running on 127.0.0.1:%d\n", PORT);
    printf("Start at least two clients, then press ENTER here.\n");

    pthread_t listener_thread;
    if (pthread_create(&listener_thread, NULL, registration_listener, NULL) != 0) {
        perror("Registration thread creation failed");
        close(server_socket);
        return EXIT_FAILURE;
    }
    pthread_detach(listener_thread);

    getchar();
    if (connected_clients() == 0) {
        printf("No clients registered. Start clients and press ENTER again.\n");
        getchar();
    }

    const char *match_name = "Campus Cricket Final: Falcons vs Tigers";
    const ScoreUpdate updates[] = {
        {"0/0", "0.0 overs", "Live"},
        {"82/2", "10.0 overs", "Live"},
        {"165/6", "20.0 overs", "Innings Break"},
        {"44/1", "5.0 overs", "Live"},
        {"121/4", "15.0 overs", "Live"},
        {"166/5", "19.4 overs", "Finished"}
    };

    char timestamp[32];
    char message[BUFFER_SIZE];
    size_t update_count = sizeof(updates) / sizeof(updates[0]);

    for (size_t i = 0; i < update_count; ++i) {
        get_timestamp(timestamp, sizeof(timestamp));
        snprintf(message, sizeof(message),
                 "[%s] Match: %s | Current Score: %s | Overs/Time: %s | Status: %s\n",
                 timestamp, match_name, updates[i].score,
                 updates[i].overs, updates[i].status);
        printf("Broadcasting: %s", message);
        broadcast_update(message);
        sleep(3);
    }

    printf("Match completed. Server is shutting down gracefully.\n");
    close(server_socket);
    return EXIT_SUCCESS;
}
