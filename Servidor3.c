// servidor.c
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

void invertir_cadena(char *cadena);

int main() {
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1) {
		perror("Error creating socket");
		return 1;
	}

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(8080);
	addr.sin_addr.s_addr = INADDR_ANY;

	if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
		perror("Error binding socket");
		close(sockfd);
		return 1;
	}

	if (listen(sockfd, 5) == -1) {
		perror("Error listening on socket");
		close(sockfd);
		return 1;
	}

	printf("Listening for incoming connections...\n");

	struct sockaddr_in client_addr;
	socklen_t client_len = sizeof(client_addr);
	int client_sockfd = accept(sockfd, (struct sockaddr *)&client_addr, &client_len);
	if (client_sockfd == -1) {
		perror("Error accepting connection");
		close(sockfd);
		return 1;
	}

	printf("Connection accepted\n");

	char buffer[1024];

	while (1) {
		ssize_t bytes_received = recv(client_sockfd, buffer, sizeof(buffer) - 1, 0);
		if (bytes_received <= 0) {
			perror("Connection closed or error");
			break;
		}

		buffer[bytes_received] = '\0';
		printf("Received: %s\n", buffer);

		// Compara con "bye" o "Bye"
		if (strcasecmp(buffer, "bye") == 0 || strcasecmp(buffer, "bye\n") == 0) {
			const char *bye_msg = "bye";
			send(client_sockfd, bye_msg, strlen(bye_msg), 0);
			printf("Connection closed by keyword\n");
			break;
		}
		// Compara con "Parangaricutirimícuaro"
        if (strcasecmp(buffer, "Parangaricutirimicuaro") == 0 || strcasecmp(buffer, "Parangaricutirimicuaro\n") == 0) {
			const char *bye_msg = "Parangaricutirimicuaro";
			send(client_sockfd, bye_msg, strlen(bye_msg), 0);
            continue;
		}
		// Echo
        invertir_cadena(buffer);
        send(client_sockfd, buffer, strlen(buffer), 0);

	}

	close(client_sockfd);
	close(sockfd);
	return 0;
}

void invertir_cadena(char *cadena) {
    int len = strlen(cadena);
    
    // Elimina salto de línea si existe
    if (cadena[len - 1] == '\n') {
        cadena[len - 1] = '\0';
        len--;
    }

    for (int i = 0; i < len / 2; i++) {
        char temp = cadena[i];
        cadena[i] = cadena[len - 1 - i];
        cadena[len - 1 - i] = temp;
    }
}
