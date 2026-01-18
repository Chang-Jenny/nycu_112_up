#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <time.h>

#define SERVER_ADDRESS "up.zoolab.org"
#define SERVER_PORT 10931
#define BUFFER_SIZE 1024

int main() {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }

    struct hostent *server = gethostbyname(SERVER_ADDRESS);
    if (server == NULL) {
        perror("Error getting server address");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    memcpy(&server_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    server_addr.sin_port = htons(SERVER_PORT);

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error connecting to server");
        exit(EXIT_FAILURE);
    }

    srand(time(NULL));

    char buffer[BUFFER_SIZE];
    for (int i = 0; i < 100; i++) {
        // printf("Sending R\n");
        write(sockfd, "R", 1);
        if (rand() % 10 == 0) {
            write(sockfd, "flag\n", 5);
            printf("Sent flag\n");
            // Read server response
            ssize_t bytes_received = read(sockfd, buffer, BUFFER_SIZE - 1);
            if (bytes_received > 0) {
                buffer[bytes_received] = '\0';
                // Check if response contains "flag" and not "ERROR> this file is protected from being read by a user."
                if (strstr(buffer, "flag") != NULL && strstr(buffer, "ERROR> this file is protected from being read by a user.") == NULL) {
                    printf("Received flag: %s\n", buffer);
                }
            }
        }
    }

    // 關閉socket
    close(sockfd);

    return 0;
}
