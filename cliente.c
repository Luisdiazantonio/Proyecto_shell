#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>

int sockfd;  // Variable global para el socket del cliente

/**
 * Manejador de la señal SIGINT (Ctrl+C).
 * En lugar de cerrar el cliente, envía un mensaje especial al servidor.
 */
void manejador(int sig) {
    const char *msg = "supercalifragilisticoespialidoso";
    send(sockfd, msg, strlen(msg), 0);
    printf("\nSe ha enviado '%s' al servidor\n", msg);
    fflush(stdout);
}

int main() {
    struct sockaddr_in server_addr;

    // Crear socket del cliente
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("Error creando socket");
        return 1;
    }

    // Configurar dirección del servidor
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;  // IPv4
    server_addr.sin_port = htons(1666);  // Puerto 1666
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);  // Dirección local (localhost)

    // Intentar conectarse al servidor
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Error conectando al servidor");
        close(sockfd);
        return 1;
    }

    // Registrar el manejador de señales para Ctrl+C
    signal(SIGINT, manejador);

    printf("Cliente conectado al servidor\n");

    char buffer[1024];

    // Bucle para enviar y recibir datos del servidor
    while (1) {
        printf("> ");
        fflush(stdout);

        // Leer entrada del usuario
        if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
            break;  // Salir si hay error en la entrada
        }

        buffer[strcspn(buffer, "\n")] = '\0';  // Eliminar salto de línea

        // Enviar datos al servidor
        ssize_t bytes_sent = send(sockfd, buffer, strlen(buffer), 0);
        if (bytes_sent == -1) {
            perror("Error enviando datos");
            break;
        }

        // Recibir respuesta del servidor
        ssize_t bytes_received = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received <= 0) {
            printf("El servidor cerró la conexión.\n");
            break;
        }

        buffer[bytes_received] = '\0';  // Convertir datos recibidos en cadena
        printf("Servidor: %s\n", buffer);
    }

    // Cerrar el socket al salir del bucle
    close(sockfd);
    return 0;
}