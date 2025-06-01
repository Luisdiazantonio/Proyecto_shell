#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <termios.h>

#define MAX_INPUT 1024
#define MAX_CMDS 4
#define MAX_ARGS 64
#define MAX_HISTORY 100

char history[MAX_HISTORY][MAX_INPUT];
int history_count = 0;

//-----------------------------------------------------PROTOTIPOS--------------------------------------------------------------------
void enable_raw_mode(struct termios *orig);
// configuracion estandar de terminal
void disable_raw_mode(struct termios *orig);
//escribir el comando almacenado segun lo tecleado
void redraw_line(const char *prompt, const char *buf, int pos);
int read_input(char *buf, const char *prompt);
// Función para dividir un comando en tokens (argumentos)
void parse_args(char *cmd, char **args);
//Conexion al Server
void conexion();

//-----------------------------------------------------FUNCION PRINCIPAL--------------------------------------------------------------------
int main() {
    char input[MAX_INPUT];
    printf("Mini Shell (Ctrl+C para salir)\n");
    printf("-------------------------------------------\n");

    while (1) {
        int len = read_input(input, "$ ");
        if (len == 0) continue;
        if (strcmp(input, "exit") == 0) break;
        if (history_count < MAX_HISTORY) {
            strcpy(history[history_count++], input);
        }

        // Separar comandos por '|'
        char *commands[MAX_CMDS];
        int num_cmds = 0;
        commands[num_cmds] = strtok(input, "|");
        while (commands[num_cmds] != NULL && num_cmds < MAX_CMDS - 1) {
            commands[++num_cmds] = strtok(NULL, "|");
        }

        int prev_fd = -1; // Para leer de la tubería anterior
        int pipefd[2];

        for (int i = 0; i < num_cmds; i++) {
            // Quitar espacios iniciales
            while (*commands[i] == ' ') commands[i]++;
            
            // Crear tubería si no es el último comando
            if (i < num_cmds - 1) {
                if (pipe(pipefd) < 0) {
                    perror("pipe");
                    exit(1);
                }
            }

            pid_t pid = fork();
            if (pid == 0) {
                // Proceso hijo

                // Si hay un anterior, redirige entrada
                if (prev_fd != -1) {
                    dup2(prev_fd, 0);
                    close(prev_fd);
                }

                // Si hay un siguiente, redirige salida
                if (i < num_cmds - 1) {
                    close(pipefd[0]);          // Cierra lectura
                    dup2(pipefd[1], 1);        // Redirige salida
                    close(pipefd[1]);          // Cierra escritura original
                }

                // Parsear y ejecutar el comando
                char *args[MAX_ARGS];
                parse_args(commands[i], args);
                execvp(args[0], args);
                perror("execvp");
                exit(1);
            } else if (pid > 0) {
                // Proceso padre

                // Cerrar extremos que ya no se usan
                if (prev_fd != -1) close(prev_fd);
                if (i < num_cmds - 1) {
                    close(pipefd[1]);   // El padre no escribe
                    prev_fd = pipefd[0];
                }
            } else {
                perror("fork");
                exit(1);
            }
        }

        // Esperar a todos los procesos
        for (int i = 0; i < num_cmds; i++) {
            wait(NULL);
        }
    }

    return 0;
}

//-----------------------------------------------------FUNCIONES--------------------------------------------------------------------

void enable_raw_mode(struct termios *orig) {
    struct termios raw;
    tcgetattr(STDIN_FILENO, orig);
    raw = *orig;
    raw.c_lflag &= ~(ICANON | ECHO); // desactiva canonico y echo, procesa linea por linea o caracter por caracter
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

// configuracion estandar de terminal
void disable_raw_mode(struct termios *orig) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, orig);
}

//escribir el comando almacenado segun lo tecleado
void redraw_line(const char *prompt, const char *buf, int pos) {
    printf("\33[2K\r%s%s", prompt, buf);  // Borra y escribe
    printf("\r%s", prompt);
    for (int i = 0; i < pos; i++) printf("%c", buf[i]);  // Mueve cursor
    fflush(stdout);
}

int read_input(char *buf, const char *prompt) {
    int len = 0, pos = 0, c, hist_index = history_count;
    struct termios orig;
    enable_raw_mode(&orig);

    printf("%s", prompt);
    fflush(stdout);

    while (1) {
        c = getchar();
        if (c == 127 || c == 8) {  // Backspace
            if (pos > 0) {
                memmove(&buf[pos - 1], &buf[pos], len - pos);
                len--;
                pos--;
                buf[len] = '\0';
                redraw_line(prompt, buf, pos);
            }
        } else if (c == '\n') {
            putchar('\n');
            buf[len] = '\0';
            break;
        } else if (c == 27) { // Escape sequence
            if (getchar() == '[') {
                switch (getchar()) {
                    case 'A': //  buscar en los comandos almacenados
                        if (hist_index > 0) {
                            hist_index--;
                            strcpy(buf, history[hist_index]);
                            len = pos = strlen(buf);
                            redraw_line(prompt, buf, pos);
                        }
                        break;
                    case 'B': //  buscar los comandos almacenados
                        if (hist_index < history_count - 1) {
                            hist_index++;
                            strcpy(buf, history[hist_index]);
                            len = pos = strlen(buf);
                        } else {
                            //vaciar el buffer, dejar en cero
                            hist_index = history_count;
                            buf[0] = '\0';
                            len = pos = 0;
                        }
                        redraw_line(prompt, buf, pos);
                        break;
                    case 'C': // 
                        if (pos < len) pos++;
                        redraw_line(prompt, buf, pos); //recorer en la linea
                        break;
                    case 'D': //
                        if (pos > 0) pos--;
                        redraw_line(prompt, buf, pos); //recorre en la linea
                        break;
                }
            }
        } else if (c >= 32 && c <= 126) { // Caracter imprimible
            if (len < MAX_INPUT - 1) {
                memmove(&buf[pos + 1], &buf[pos], len - pos);
                buf[pos++] = c;
                len++;
                buf[len] = '\0';
                redraw_line(prompt, buf, pos);
            }
        }
    }

    disable_raw_mode(&orig);
    return len;
}

// Función para dividir un comando en tokens (argumentos)
void parse_args(char *cmd, char **args) {
    int i = 0;
    args[i] = strtok(cmd, " ");
    while (args[i] != NULL && i < MAX_ARGS - 1) {
        args[++i] = strtok(NULL, " ");
    }
    args[i] = NULL;
}

//Este codigo es para la conexion del Shell con el servidor
void conexion() {
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1) {
		perror("Error creating socket");
		return 1;
	}

	struct sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(8080);
	inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

	if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
		perror("Error connecting to server");
		close(sockfd);
		return 1;
	}

	char buffer[1024];
	while (1) {
		printf("> ");
		fflush(stdout);

		if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
			break;
		}

		// Enviar mensaje
		ssize_t bytes_sent = send(sockfd, buffer, strlen(buffer), 0);
		if (bytes_sent == -1) {
			perror("Error sending data");
			break;
		}

		// Recibir respuesta
		ssize_t bytes_received = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
		if (bytes_received <= 0) {
			printf("Server closed the connection.\n");
			break;
		}
		buffer[bytes_received] = '\0';

		printf("Server: %s\n", buffer);
	}

	close(sockfd);
	return 0;
}


