#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define PORT 5567
#define BUFFER_SIZE 1024
#define USERNAME_SIZE 50

static void get_timestamp(char *buffer, size_t size)
{
    time_t now = time(NULL);
    struct tm local_time;
    localtime_r(&now, &local_time);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", &local_time);
}

int main(int argc, char *argv[])
{
    char username[USERNAME_SIZE];
    char buffer[BUFFER_SIZE];
    char received_at[32];

    if (argc >= 2) {
        snprintf(username, sizeof(username), "%s", argv[1]);
    } else {
        printf("Enter username: ");
        if (fgets(username, sizeof(username), stdin) == NULL) {
            return EXIT_FAILURE;
        }
        username[strcspn(username, "\r\n")] = '\0';
    }

    int client_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (client_socket < 0) {
        perror("UDP socket creation failed");
        return EXIT_FAILURE;
    }

    struct timeval timeout = {.tv_sec = 30, .tv_usec = 0};
    setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO,
               &timeout, sizeof(timeout));

    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &server_address.sin_addr);

    snprintf(buffer, sizeof(buffer), "REGISTER %s\n", username);
    if (sendto(client_socket, buffer, strlen(buffer), 0,
               (struct sockaddr *)&server_address,
               sizeof(server_address)) < 0) {
        perror("Registration request failed");
        close(client_socket);
        return EXIT_FAILURE;
    }

    printf("UDP client %s registered. Waiting for live scores...\n", username);

    while (1) {
        ssize_t bytes = recvfrom(client_socket, buffer, sizeof(buffer) - 1,
                                 0, NULL, NULL);
        if (bytes < 0) {
            perror("No score update received within timeout");
            close(client_socket);
            return EXIT_FAILURE;
        }

        buffer[bytes] = '\0';
        get_timestamp(received_at, sizeof(received_at));
        printf("Received at %s: %s", received_at, buffer);

        if (strstr(buffer, "Status: Finished") != NULL) {
            printf("Final match status received. Client exiting gracefully.\n");
            break;
        }
    }

    close(client_socket);
    return EXIT_SUCCESS;
}
