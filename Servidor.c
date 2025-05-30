#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <signal.h>

#define PUERTO 1666

int contiene_passwd(const char *mensaje) {
    return strstr(mensaje, "passwd") != NULL;
}

int main() {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("Error al crear socket");
        return 1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PUERTO);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("Error al hacer bind");
        close(sockfd);
        return 1;
    }

    if (listen(sockfd, 5) == -1) {
        perror("Error al escuchar");
        close(sockfd);
        return 1;
    }

    printf("Servidor escuchando en puerto %d...\n", PUERTO);

    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_sockfd = accept(sockfd, (struct sockaddr *)&client_addr, &client_len);
    if (client_sockfd == -1) {
        perror("Error al aceptar conexión");
        close(sockfd);
        return 1;
    }

    char buffer[1024];
    while (1) {
        ssize_t bytes_received = recv(client_sockfd, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received <= 0) {
            perror("Conexión cerrada o error");
            break;
        }

        buffer[bytes_received] = '\0';
        printf("[MiniShell]: %s\n", buffer);

        if (strcmp(buffer, "supercaligragilisticoespilaridoso") == 0) {
            const char *msg = "no es posible interrumpir utilizando ctrl + c";
            send(client_sockfd, msg, strlen(msg), 0);
            continue;
        }

        if (contiene_passwd(buffer)) {
            const char *msg = "cerrar";
            send(client_sockfd, msg, strlen(msg), 0);
            break;
        }

        const char *ok = "ok";
        send(client_sockfd, ok, strlen(ok), 0);
    }

    close(client_sockfd);
    close(sockfd);
    return 0;
}
