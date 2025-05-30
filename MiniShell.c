#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <sys/wait.h>
#include <signal.h>
#include <arpa/inet.h>

#define MAX_INPUT 1024
#define MAX_CMDS 4
#define MAX_ARGS 64
#define MAX_HISTORY 100
#define PUERTO 1666

char history[MAX_HISTORY][MAX_INPUT];
int history_count = 0;
int sockfd;

// ======================== COMUNICACIÓN CON SERVIDOR ========================

void enviar_a_servidor(const char *mensaje) {
    send(sockfd, mensaje, strlen(mensaje), 0);
    char respuesta[128] = "";
    recv(sockfd, respuesta, sizeof(respuesta) - 1, 0);
    respuesta[127] = '\0';

    if (strcmp(respuesta, "cerrar") == 0) {
        printf("Servidor pidió cerrar por seguridad.\n");
        close(sockfd);
        exit(0);
    } else if (strstr(respuesta, "no es posible") != NULL) {
        printf("Servidor: %s\n", respuesta);
    }
}

// ======================== MANEJADOR DE SEÑALES ========================

void handle_sigint(int sig) {
    (void)sig;
    enviar_a_servidor("supercaligragilisticoespilaridoso");
}

// ======================== UTILIDADES ========================

void enable_raw_mode(struct termios *orig) {
    struct termios raw;
    tcgetattr(STDIN_FILENO, orig);
    raw = *orig;
    raw.c_lflag &= ~(ICANON | ECHO);
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

int read_input(char *buf, const char *prompt) {
    int len = 0, pos = 0, c, hist_index = history_count;
    struct termios orig;
    enable_raw_mode(&orig);

    printf("%s", prompt);
    fflush(stdout);

    while (1) {
        c = getchar();
        if (c == 127 || c == 8) {
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
        } else if (c == 27) {
            if (getchar() == '[') {
                switch (getchar()) {
                    case 'A':
                        if (hist_index > 0) {
                            hist_index--;
                            strcpy(buf, history[hist_index]);
                            len = pos = strlen(buf);
                            redraw_line(prompt, buf, pos);
                        }
                        break;
                    case 'B':
                        if (hist_index < history_count - 1) {
                            hist_index++;
                            strcpy(buf, history[hist_index]);
                            len = pos = strlen(buf);
                        } else {
                            hist_index = history_count;
                            buf[0] = '\0';
                            len = pos = 0;
                        }
                        redraw_line(prompt, buf, pos);
                        break;
                    case 'C':
                        if (pos < len) pos++;
                        redraw_line(prompt, buf, pos);
                        break;
                    case 'D':
                        if (pos > 0) pos--;
                        redraw_line(prompt, buf, pos);
                        break;
                }
            }
        } else if (c >= 32 && c <= 126) {
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

void parse_args(char *cmd, char **args) {
    int i = 0;
    args[i] = strtok(cmd, " ");
    while (args[i] != NULL && i < MAX_ARGS - 1) {
        args[++i] = strtok(NULL, " ");
    }
    args[i] = NULL;
}

// ======================== FUNCIÓN PRINCIPAL ========================

int main() {
    struct sockaddr_in server_addr;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PUERTO);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);
    connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));

    signal(SIGINT, handle_sigint);

    char input[MAX_INPUT];
    printf("Mini Shell con servidor en puerto %d\n", PUERTO);
    printf("-------------------------------------------\n");

    while (1) {
        int len = read_input(input, "$ ");
        if (len == 0) continue;
        if (strcmp(input, "exit") == 0) break;

        enviar_a_servidor(input);

        if (history_count < MAX_HISTORY) {
            strcpy(history[history_count++], input);
        }

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
                exit(1);
            }
        }

        for (int i = 0; i < num_cmds; i++) wait(NULL);
    }

    close(sockfd);
    return 0;
}
