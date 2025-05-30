#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>

int sockfd;

void manejador(int sig) {
    const char *msg = "supercalifragilisticoespialidoso";
    send(sockfd, msg, strlen(msg), 0);
    printf("\nSe ha enviado '%s' al servidor\n", msg);
    fflush(stdout);
}

int main() {
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("Error creando socket");
        return 1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(1666);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Error conectando al servidor");
        close(sockfd);
        return 1;
    }

    signal(SIGINT, manejador);  // Capturar Ctrl+C

    printf("Conectado al servidor\n");

    char buffer[1024];
    while (1) {
        printf("> ");
        fflush(stdout);

        if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
            break;
        }

        ssize_t bytes_sent = send(sockfd, buffer, strlen(buffer), 0);
        if (bytes_sent == -1) {
            perror("Error enviando datos");
            break;
        }

        ssize_t bytes_received = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received <= 0) {
            printf("El servidor cerró la conexión.\n");
            break;
        }

        buffer[bytes_received] = '\0';
        printf("Servidor: %s\n", buffer);
    }

    close(sockfd);
    return 0;
}