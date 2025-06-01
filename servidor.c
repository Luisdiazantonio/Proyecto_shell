#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <arpa/inet.h>

#define SERVER_PORT 1666      // Puerto del servidor
#define BUFFER_SIZE 1024      // Tamaño del buffer para recibir datos
#define MAX_CLIENTS 2         // Número máximo de clientes en espera

volatile sig_atomic_t server_running = 1;  // Variable global para controlar la ejecución del servidor

/**
 * Manejador de señales SIGINT y SIGTERM.
 * Cuando se recibe Ctrl+C, se marca la variable `server_running` como 0 para detener el servidor.
 */
void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        server_running = 0;
        printf("\nDeteniendo servidor...\n");
    }
}

/**
 * Función para manejar la conexión con un cliente.
 * - Recibe mensajes del cliente.
 * - Si el cliente envía "supercalifragilisticoespialidoso", el servidor responde con un mensaje especial.
 * - Si el cliente envía "passwd", el servidor se detiene por seguridad.
 * - Se hace "echo" de cualquier otro mensaje recibido.
 */
void handle_client(int client_sockfd) {
    char buffer[BUFFER_SIZE];

    while (server_running) {
        // Recibir datos del cliente
        ssize_t bytes_received = recv(client_sockfd, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received <= 0) {
            if (bytes_received == 0) {
                printf("Cliente desconectado.\n");
            } else {
                perror("Error recibiendo datos");
            }
            break;
        }

        buffer[bytes_received] = '\0';  // Convertir los datos recibidos a cadena de texto
        printf("Recibido: %s\n", buffer);

        // Si el cliente envía "supercalifragilisticoespialidoso", responder con el mensaje especial
        if (strcmp(buffer, "supercalifragilisticoespialidoso") == 0) {
            const char *response = "No es posible interrumpir utilizando Ctrl+C";
            send(client_sockfd, response, strlen(response), 0);
            continue;
        }

        // Si se detecta el comando "passwd", cerrar el servidor
        if (strstr(buffer, "passwd") != NULL) {
            printf("Comando 'passwd' detectado. Cerrando servidor.\n");
            send(client_sockfd, "SERVIDOR: Comando 'passwd' detectado. Cerrando conexión.", 53, 0);
            server_running = 0;
            break;
        }

        // Enviar un eco del mensaje recibido de vuelta al cliente
        send(client_sockfd, buffer, strlen(buffer), 0);
    }

    close(client_sockfd);  // Cerrar la conexión con el cliente al salir del bucle
}

int main() {
    int sockfd, client_sockfd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    // Configurar manejo de señales para permitir la terminación controlada del servidor
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Crear el socket del servidor
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("Error creando socket");
        return 1;
    }

    // Permitir reutilización del puerto
    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Configurar dirección del servidor
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;        // IPv4
    server_addr.sin_port = htons(SERVER_PORT); // Puerto asignado
    server_addr.sin_addr.s_addr = INADDR_ANY; // Aceptar conexiones desde cualquier IP

    // Asociar el socket con la dirección y el puerto configurados
    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Error en bind");
        close(sockfd);
        return 1;
    }

    // Configurar el socket en modo escucha para aceptar conexiones entrantes
    if (listen(sockfd, MAX_CLIENTS) == -1) {
        perror("Error en listen");
        close(sockfd);
        return 1;
    }

    printf("Servidor Mini Shell en puerto %d\n", SERVER_PORT);

    // Bucle principal: acepta conexiones de clientes y los maneja
    while (server_running) {
        client_sockfd = accept(sockfd, (struct sockaddr *)&client_addr, &client_len);
        if (client_sockfd == -1) {
            if (errno == EINTR) continue;  // Ignorar interrupciones por señales y continuar
            perror("Error en accept");
            continue;
        }

        handle_client(client_sockfd);  // Manejar la conexión con el cliente
    }

    close(sockfd);  // Cerrar el socket principal del servidor al terminar
    printf("Servidor detenido.\n");
    return 0;
}

/*se compilan tanto servidor como cliente, se ejecuta primero el servidor deberia mostrar un mensaje que diga que se esta esperando conexiones, 
se ejecuta cliente y se verifica la conexion */