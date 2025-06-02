/* Elaborado por Equipo 4
Cervantes Rosales Abdiel
Diaz Antonio Luis Pedro
Hermida Mendez Yaretzi Belen
Martinez Altamirano Ricardo
Ramos Bello Jose Luis
implementacion de un minishell con funciones basica de comandos concatenados, tambien cuenta con almacenamieto de los comandos anteriormente ejecutados.
Al ejecutar se muestra en la misma terminal el simbolo de sistema, se ingresa el comando, lo almacena en el buffer y se fragmenta el comando para procesarlos,
una vez un comando haya sido ejecutado queda como actividad registrada y se puede recorrer con la flecha arriba abajo.


Nota: El programa no cuenta con la capacidad de navegar entre directorios, es decir se mantiene donde se ejecuta. 
Nota: El tamaño de comandos posible unidos es de 4 por medio de pipes.

Modificaciones por Diaz Antonio Luis Pedro
modifcacion de cliente servidor

*/



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#define PUERTO 1666

#define MAX_INPUT 1024
#define MAX_CMDS 4
#define MAX_ARGS 64
#define MAX_HISTORY 100
#define MAX_OUTPUT 65536

char history[MAX_HISTORY][MAX_INPUT];
int history_count = 0;
int sockfd;

//-----------------------------------------------------PROTOTIPOS--------------------------------------------------------------------
//comando_cliente
void comando_cliente(char *input);
//comando_buffer
void comando_buffer(char *input, char *output_buffer, size_t buffer_size);
//enviar y recibir respuesta de servidor
void enviar_a_servidor(const char *mensaje);
//
void enable_raw_mode(struct termios *orig);
// configuracion estandar de terminal
void disable_raw_mode(struct termios *orig);
//escribir el comando almacenado segun lo tecleado
void redraw_line(const char *prompt, const char *buf, int pos);
int read_input(char *buf, const char *prompt);
// Función para dividir un comando en tokens (argumentos)
void parse_args(char *cmd, char **args);
//manejo de ctrl + c
void handle_sigint(int sig);

//-----------------------------------------------------FUNCION PRINCIPAL--------------------------------------------------------------------
int main() {
    //conexion con el servidor
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1) {
		perror("Error creating socket");
		return 1;
	}

	struct sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(PUERTO);
	inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

	if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
		perror("Error connecting to server");
		close(sockfd);
		return 1;
	}

    char input[MAX_INPUT];
    char command_output[MAX_OUTPUT];  // Buffer para almacenar salida de comandos
    signal(SIGINT, handle_sigint);  // Captura Ctrl+C

    printf("Mini Shell (exit para salir)\n");
    printf("-------------------------------------------\n");

    while (1) {
        int len = read_input(input, "$ ");
        if (len == 0) continue;
        if (strcmp(input, "exit") == 0) break;
        enviar_a_servidor(input);

        if (history_count < MAX_HISTORY) {
            strcpy(history[history_count++], input);
        }
        comando_cliente(input);
        comando_buffer(input, command_output, sizeof(command_output));
        if (command_output[0] != '\0') {
            enviar_a_servidor(command_output);
        }
    }
    close(sockfd);
    return 0;
}

//-----------------------------------------------------FUNCIONES--------------------------------------------------------------------
//comando para la terminal
void comando_cliente(char *input) {
    char *commands[MAX_CMDS];
    int num_cmds = 0;
    int prev_fd = -1;
    int pipefd[2];
    // Separar comandos por '|'
    commands[num_cmds] = strtok(input, "|");
    while (commands[num_cmds] != NULL && num_cmds < MAX_CMDS - 1) {
        commands[++num_cmds] = strtok(NULL, "|");
    }
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
            if (prev_fd != -1) {
                dup2(prev_fd, STDIN_FILENO);
                close(prev_fd);
            }

            if (i < num_cmds - 1) {
                close(pipefd[0]);
                dup2(pipefd[1], STDOUT_FILENO);
                close(pipefd[1]);
            }

            char *args[MAX_ARGS];
            parse_args(commands[i], args);
            execvp(args[0], args);
            perror("execvp");
            exit(1);
        } else if (pid > 0) {
            // Proceso padre
            if (prev_fd != -1) close(prev_fd);
            if (i < num_cmds - 1) {
                close(pipefd[1]);
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
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//comando para guardar en buffer
void comando_buffer(char *input, char *output_buffer, size_t buffer_size) {
    char *commands[MAX_CMDS];
    int num_cmds = 0;
    int prev_fd = -1;
    int pipefd[2];
    int output_pipe[2];
    ssize_t bytes_read;

    // Inicializar buffer de salida
    output_buffer[0] = '\0';

    // Separar comandos por '|'
    commands[num_cmds] = strtok(input, "|");
    while (commands[num_cmds] != NULL && num_cmds < MAX_CMDS - 1) {
        commands[++num_cmds] = strtok(NULL, "|");
    }

    // Crear pipe para capturar salida solo si hay comandos
    if (num_cmds > 0 && pipe(output_pipe) < 0) {
        perror("pipe");
        exit(1);
    }

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
            if (prev_fd != -1) {
                dup2(prev_fd, STDIN_FILENO);
                close(prev_fd);
            }

            // Redirigir salida
            if (i < num_cmds - 1) {
                close(pipefd[0]);
                dup2(pipefd[1], STDOUT_FILENO);
                close(pipefd[1]);
            } else {
                // Para el último comando, redirigir a nuestro pipe adicional
                close(output_pipe[0]);
                dup2(output_pipe[1], STDOUT_FILENO);
                close(output_pipe[1]);
            }

            char *args[MAX_ARGS];
            parse_args(commands[i], args);
            execvp(args[0], args);
            perror("execvp");
            exit(1);
        } else if (pid > 0) {
            // Proceso padre
            if (prev_fd != -1) close(prev_fd);
            if (i < num_cmds - 1) {
                close(pipefd[1]);
                prev_fd = pipefd[0];
            } else {
                // Para el último comando, leer la salida
                close(output_pipe[1]);
                bytes_read = read(output_pipe[0], output_buffer, buffer_size - 1);
                if (bytes_read > 0) {
                    output_buffer[bytes_read] = '\0';
                }
                close(output_pipe[0]);
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
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//funcion para enviar al servidor
void enviar_a_servidor(const char *mensaje) {
    send(sockfd, mensaje, strlen(mensaje), 0);
    char respuesta[128] = "";
    recv(sockfd, respuesta, sizeof(respuesta) - 1, 0);
    respuesta[127] = '\0';

    if (strcmp(respuesta, "passwd") == 0) {
        printf("\nHas sido hackeado.\n");
        close(sockfd);
        exit(0);
    }else if (strcmp(respuesta, "supercaligragilisticoespilaridoso") == 0) {
        printf("no es posible interrumpir utilizando ctrl + c\n");
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//controlador se señal
void handle_sigint(int sig) {
    (void)sig;
    enviar_a_servidor("supercaligragilisticoespilaridoso");
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void enable_raw_mode(struct termios *orig) {
    struct termios raw;
    tcgetattr(STDIN_FILENO, orig);
    raw = *orig;
    raw.c_lflag &= ~(ICANON | ECHO); // desactiva canonico y echo, procesa linea por linea o caracter por caracter
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// configuracion estandar de terminal
void disable_raw_mode(struct termios *orig) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, orig);
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//escribir el comando almacenado segun lo tecleado
void redraw_line(const char *prompt, const char *buf, int pos) {
    printf("\33[2K\r%s%s", prompt, buf);  // Borra y escribe
    printf("\r%s", prompt);
    for (int i = 0; i < pos; i++) printf("%c", buf[i]);  // Mueve cursor
    fflush(stdout);
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
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
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// Función para dividir un comando en tokens (argumentos)
void parse_args(char *cmd, char **args) {
    int i = 0;
    args[i] = strtok(cmd, " ");
    while (args[i] != NULL && i < MAX_ARGS - 1) {
        args[++i] = strtok(NULL, " ");
    }
    args[i] = NULL;
}