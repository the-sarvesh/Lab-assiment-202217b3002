#include <arpa/inet.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PORT 5566
#define BUFFER_SIZE 1024
#define USERNAME_SIZE 50

static int client_socket;

static void *receive_messages(void *argument)
{
    (void)argument;
    char buffer[BUFFER_SIZE];

    while (1) {
        ssize_t bytes = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        if (bytes <= 0) {
            printf("\nDisconnected from server.\n");
            break;
        }
        buffer[bytes] = '\0';
        printf("\n%s", buffer);
        fflush(stdout);
    }
    return NULL;
}

int main(int argc, char *argv[])
{
    char username[USERNAME_SIZE];
    char message[BUFFER_SIZE];

    if (argc >= 2) {
        snprintf(username, sizeof(username), "%s", argv[1]);
    } else {
        printf("Enter username: ");
        if (fgets(username, sizeof(username), stdin) == NULL) {
            return EXIT_FAILURE;
        }
        username[strcspn(username, "\r\n")] = '\0';
    }

    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0) {
        perror("Socket creation failed");
        return EXIT_FAILURE;
    }

    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &server_address.sin_addr);

    if (connect(client_socket, (struct sockaddr *)&server_address,
                sizeof(server_address)) < 0) {
        perror("Connection failed");
        close(client_socket);
        return EXIT_FAILURE;
    }

    snprintf(message, sizeof(message), "REGISTER %s\n", username);
    send(client_socket, message, strlen(message), 0);

    ssize_t bytes = recv(client_socket, message, sizeof(message) - 1, 0);
    if (bytes <= 0) {
        printf("Registration failed: server disconnected.\n");
        close(client_socket);
        return EXIT_FAILURE;
    }
    message[bytes] = '\0';
    printf("%s", message);
    if (strncmp(message, "ERROR", 5) == 0) {
        close(client_socket);
        return EXIT_FAILURE;
    }

    if (strcmp(username, "FACULTY") == 0) {
        printf("Enter announcements as: ANNOUNCE <message>\n");
    } else {
        printf("Waiting for announcements. Type exit to disconnect.\n");
    }

    pthread_t receiver_thread;
    if (pthread_create(&receiver_thread, NULL, receive_messages, NULL) != 0) {
        perror("Receiver thread creation failed");
        close(client_socket);
        return EXIT_FAILURE;
    }

    while (fgets(message, sizeof(message), stdin) != NULL) {
        if (send(client_socket, message, strlen(message), 0) < 0) {
            perror("Send failed");
            break;
        }
        if (strncmp(message, "exit", 4) == 0) {
            break;
        }
    }

    shutdown(client_socket, SHUT_RDWR);
    pthread_join(receiver_thread, NULL);
    close(client_socket);
    return EXIT_SUCCESS;
}
