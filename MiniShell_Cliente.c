#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

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
