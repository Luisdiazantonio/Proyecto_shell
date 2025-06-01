#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <sys/wait.h>
#include <signal.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/types.h>
#include <netdb.h>

#define MAX_INPUT 1024
#define MAX_CMDS 4
#define MAX_ARGS 64
#define MAX_HISTORY 100
#define SERVER_PORT 1666
#define RECONNECT_DELAY 2

// Estructura para el estado global
typedef struct {
    int sockfd;
    volatile sig_atomic_t connected;
    char history[MAX_HISTORY][MAX_INPUT];
    int history_count;
    struct sockaddr_in server_addr;
} AppState;

// ======================== MANEJADOR DE SEÑALES ========================

void handle_sigint(int sig) {
    (void)sig;
    printf("\n");
}

// ======================== COMUNICACIÓN CON SERVIDOR ========================

int send_to_server(AppState *state, const char *message) {
    if (!state->connected) return -1;
    
    if (send(state->sockfd, message, strlen(message), 0) == -1) {
        perror("Error al enviar al servidor");
        state->connected = 0;
        return -1;
    }
    
    char response[MAX_INPUT] = "";
    ssize_t bytes_recv = recv(state->sockfd, response, sizeof(response) - 1, 0);
    
    if (bytes_recv <= 0) {
        if (bytes_recv == 0) {
            printf("Servidor cerró la conexión\n");
        } else {
            perror("Error al recibir del servidor");
        }
        state->connected = 0;
        return -1;
    }
    
    response[bytes_recv] = '\0';

    if (strcmp(response, "cerrar") == 0) {
        printf("Servidor pidió cerrar por seguridad.\n");
        state->connected = 0;
        close(state->sockfd);
        exit(0);
    } else if (strstr(response, "no es posible") != NULL) {
        printf("Servidor: %s\n", response);
    } else {
        printf("Servidor: %s\n", response);
    }
    
    return 0;
}

// ======================== UTILIDADES DE TERMINAL ========================

void enable_raw_mode(struct termios *orig) {
    struct termios raw;
    tcgetattr(STDIN_FILENO, orig);
    raw = *orig;
    raw.c_lflag &= ~(ICANON | ECHO | ECHOE | ECHOK | ECHONL | ICRNL);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void disable_raw_mode(struct termios *orig) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, orig);
}

void redraw_line(const char *prompt, const char *buf, int pos) {
    printf("\33[2K\r%s%s", prompt, buf);
    printf("\r%s", prompt);
    for (int i = 0; i < pos; i++) printf("%c", buf[i]);
    fflush(stdout);
}

int read_input(AppState *state, char *buf, const char *prompt) {
    int len = 0, pos = 0, c, hist_index = state->history_count;
    struct termios orig;
    enable_raw_mode(&orig);

    printf("%s", prompt);
    fflush(stdout);

    while (1) {
        c = getchar();
        
        // Manejo de teclas especiales
        if (c == 127 || c == 8) { // Backspace
            if (pos > 0) {
                memmove(&buf[pos - 1], &buf[pos], len - pos);
                len--;
                pos--;
                buf[len] = '\0';
                redraw_line(prompt, buf, pos);
            }
        } else if (c == '\n') { // Enter
            putchar('\n');
            buf[len] = '\0';
            break;
        } else if (c == 27) { // Escape sequence (flechas)
            if (getchar() == '[') {
                switch (getchar()) {
                    case 'A': // Flecha arriba - historial
                        if (hist_index > 0) {
                            hist_index--;
                            strcpy(buf, state->history[hist_index]);
                            len = pos = strlen(buf);
                            redraw_line(prompt, buf, pos);
                        }
                        break;
                    case 'B': // Flecha abajo - historial
                        if (hist_index < state->history_count - 1) {
                            hist_index++;
                            strcpy(buf, state->history[hist_index]);
                            len = pos = strlen(buf);
                        } else {
                            hist_index = state->history_count;
                            buf[0] = '\0';
                            len = pos = 0;
                        }
                        redraw_line(prompt, buf, pos);
                        break;
                    case 'C': // Flecha derecha
                        if (pos < len) pos++;
                        redraw_line(prompt, buf, pos);
                        break;
                    case 'D': // Flecha izquierda
                        if (pos > 0) pos--;
                        redraw_line(prompt, buf, pos);
                        break;
                }
            }
        } else if (c >= 32 && c <= 126) { // Caracteres imprimibles
            if (len < MAX_INPUT - 1) {
                memmove(&buf[pos + 1], &buf[pos], len - pos);
                buf[pos++] = c;
                len++;
                buf[len] = '\0';
                redraw_line(prompt, buf, pos);
            }
        } else if (c == 3) { // Ctrl+C
            if (state->connected) {
                send_to_server(state, "supercalifragilisticoespialidoso");
            }
            printf("\n%s", prompt);
            fflush(stdout);
        }
    }

    disable_raw_mode(&orig);
    return len;
}

void parse_args(char *cmd, char **args) {
    int i = 0;
    args[i] = strtok(cmd, " ");
    while (args[i] != NULL && i < MAX_ARGS - 1) {
        args[++i] = strtok(NULL, " ");
    }
    args[i] = NULL;
}

// ======================== MANEJO DE CONEXIÓN ========================

int connect_to_server(AppState *state) {
    state->sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (state->sockfd == -1) {
        perror("Error al crear socket");
        return -1;
    }
    
    if (connect(state->sockfd, (struct sockaddr *)&state->server_addr, sizeof(state->server_addr)) == -1) {
        close(state->sockfd);
        return -1;
    }
    
    state->connected = 1;
    return 0;
}

void init_server_connection(AppState *state) {
    memset(&state->server_addr, 0, sizeof(state->server_addr));
    state->server_addr.sin_family = AF_INET;
    state->server_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, "127.0.0.1", &state->server_addr.sin_addr);
    
    state->connected = 0;
    state->history_count = 0;
}

int reconnect_to_server(AppState *state) {
    if (state->connected) return 0;
    
    printf("Intentando reconectar con el servidor...\n");
    
    close(state->sockfd);
    if (connect_to_server(state) == 0) {
        printf("Conexión restablecida\n");
        return 0;
    }
    
    return -1;
}

// ======================== EJECUCIÓN DE COMANDOS ========================

int execute_local_command(char *input) {
    char *commands[MAX_CMDS];
    int num_cmds = 0;
    
    commands[num_cmds] = strtok(input, "|");
    while (commands[num_cmds] != NULL && num_cmds < MAX_CMDS - 1) {
        commands[++num_cmds] = strtok(NULL, "|");
    }

    int prev_fd = -1;
    int pipefd[2];

    for (int i = 0; i < num_cmds; i++) {
        while (*commands[i] == ' ') commands[i]++;
        if (i < num_cmds - 1) pipe(pipefd);

        pid_t pid = fork();
        if (pid == 0) {
            if (prev_fd != -1) {
                dup2(prev_fd, 0);
                close(prev_fd);
            }
            if (i < num_cmds - 1) {
                close(pipefd[0]);
                dup2(pipefd[1], 1);
                close(pipefd[1]);
            }
            char *args[MAX_ARGS];
            parse_args(commands[i], args);
            execvp(args[0], args);
            perror("execvp");
            exit(1);
        } else if (pid > 0) {
            if (prev_fd != -1) close(prev_fd);
            if (i < num_cmds - 1) {
                close(pipefd[1]);
                prev_fd = pipefd[0];
            }
        } else {
            perror("fork");
            return -1;
        }
    }

    for (int i = 0; i < num_cmds; i++) wait(NULL);
    return 0;
}

// ======================== FUNCIÓN PRINCIPAL ========================

int main() {
    AppState state;
    char input[MAX_INPUT];
    
    // Configuración inicial
    signal(SIGINT, handle_sigint);
    init_server_connection(&state);
    
    if (connect_to_server(&state) == -1) {
        fprintf(stderr, "No se pudo conectar al servidor al inicio\n");
    }

    printf("MiniShell - Conectado al servidor en puerto %d\n", SERVER_PORT);

    while (1) {
        if (!state.connected) {
            sleep(RECONNECT_DELAY);
            reconnect_to_server(&state);
        }

        int len = read_input(&state, input, state.connected ? "$ " : "(desconectado) $ ");
        if (len == 0) continue;
        
        // Comandos internos
        if (strcmp(input, "exit") == 0) break;
        if (strcmp(input, "reconnect") == 0) {
            reconnect_to_server(&state);
            continue;
        }

        // Añadir al historial
        if (state.history_count < MAX_HISTORY) {
            strcpy(state.history[state.history_count++], input);
        } else {
            // Rotar historial si está lleno
            for (int i = 0; i < MAX_HISTORY - 1; i++) {
                strcpy(state.history[i], state.history[i + 1]);
            }
            strcpy(state.history[MAX_HISTORY - 1], input);
        }

        // Ejecución local o envío al servidor
        if (input[0] == '!') {
            // Ejecución local (prefijo !)
            if (execute_local_command(input + 1) == -1) {
                printf("Error al ejecutar comando local\n");
            }
        } else if (state.connected) {
            // Envío al servidor
            send_to_server(&state, input);
        } else {
            printf("No conectado al servidor. Comando no enviado.\n");
        }
    }

    if (state.connected) {
        close(state.sockfd);
    }
    return 0;
}