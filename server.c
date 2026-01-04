/* server.c app */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <signal.h>


#define MAX_CLIENTS 100 /* 100 client */
#define BUFFER_SIZE 2048 /* msg size */
#define PORT 8080 /* server port */
		 
typedef struct {
	int socket;
	struct sockaddr_in address;
	char nickname[32];
	int active;
} Client;

Client clients[MAX_CLIENTS];
int server_socket;
fd_set read_fds;
int max_sd;

/* function prototype */
void initialize_server();
void handle_new_connection();
void handle_client_message(int client_index);
void broadcast_message(const char *message, int sender_socket);

void cleanup();

int main() {
	printf("=== Multi-Connection Chat Server ===\n");
	printf("Maximum clients :  %d\n", MAX_CLIENTS);
	printf("Server starting on port : %d...\n", PORT);

	/* initialize server */
	initialize_server();

	/* Main server loop */
	while (1) {
		/* Clear the socket set */
		FD_ZERO(&read_fds);

		/* add server socket set */
		FD_SET(server_socket, &read_fds);

		max_sd = server_socket;

		/* add client socket to set */
		for (int i=0; i < MAX_CLIENTS; i++) {
			if (clients[i].active) {
				int sd = clients[i].socket;
				FD_SET(sd, &read_fds);
				if (sd > max_sd) {
					max_sd = sd;
				}
			}
		}



	exit(EXIT_SUCCESS);
}

