#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PORT 8084
#define BUFFER_SIZE 1024
#define BUFFER_RESP (BUFFER_SIZE + 7)

int main() {
    int sockfd, client_sockfd;
    struct sockaddr_in addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];

    // Crear socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("Error al crear socket");
        return 1;
    }

    // Configurar dirección del servidor
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    // Asociar socket a la dirección
    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("Error al asociar socket");
        close(sockfd);
        return 1;
    }

    // Escuchar conexiones entrantes
    if (listen(sockfd, 5) == -1) {
        perror("Error al escuchar en socket");
        close(sockfd);
        return 1;
    }

    printf("Esperando conexiones en el puerto %d...\n", PORT);

    // Aceptar conexión entrante
    client_sockfd = accept(sockfd, (struct sockaddr *)&client_addr, &client_len);
    if (client_sockfd == -1) {
        perror("Error al aceptar conexión");
        close(sockfd);
        return 1;
    }

    printf("Cliente conectado\n");

    // Comunicación con el cliente
    while (1) {
        ssize_t bytes_received = recv(client_sockfd, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received == -1) {
            perror("Error al recibir datos");
            break;
        }
        buffer[bytes_received] = '\0';
        printf("Cliente: %s\n", buffer);

        if (strcmp(buffer, "bye") == 0) {
            printf("Cliente cerró la conexión\n");
            break;
        }

        // Crear mensaje con prefijo "echo"
        char response[BUFFER_RESP];
        snprintf(response, sizeof(response), "echo %s", buffer);

        // Enviar respuesta al cliente
        ssize_t bytes_sent = send(client_sockfd, response, strlen(response), 0);
        if (bytes_sent == -1) {
            perror("Error al enviar datos");
            break;
        }
    }

    // Cerrar sockets
    close(client_sockfd);
    close(sockfd);
    return 0;
}
