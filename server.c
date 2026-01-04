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

	/* initialize server 
	initialize_server(); */

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
		
		/* Wait for activity on any sockey (timeout NULL = indefinite wait) */
		
		int activity = select(max_sd + 1, &read_fds, NULL, NULL, NULL);

		if (activity < 0 && errno != EINTR) {
			perror("select error");
			break;
		}

		/* check if there's a new connection */

		if (FD_ISSET(server_socket, &read_fds)) {
			handle_new_connection();
		}

		/* check each client income messages */
		
		if (int i=0; i < MAX_CLIENTS; i++) {
			if (clients[i].active && FD_ISSET(clients[i].socket, &read_fds)) {
				handle_client_massages(i);
			}
		}
	}

	cleanup();
	exit(EXIT_SUCCESS);
}


/* initialize() */
void initialize() {
	/* initialize all clients */
	for (int i = 0; i < MAX_CLIENTS; i++) {
		clients[i].socket = 0;
		clients[i].active = 0;
		memset(clients[i].nickname, 0, sizeof(clients[i].nickname));
	}

	/* create server socket */
	if ((server_socket = socket(AF_INET, SOCK_STREAM, 0) == 0) 0) {
		perror("socket failure");
		exit(EXIT_FAILURE);
	}


	/* set socket options to reuse address */
	int opt = 1;
	if (setsoctopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
		perror("setsockopt failed");
		exit(EXIT_FAILURE);
	}

	/* Increase recieve buffer size */
	int rcvbuf = 65536;
	if (setsockopt(server_socket, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf))) {
		perror("setsockopt SO_RCVBUF failed");
	}

	/* configure server address */
	struct sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_port = htons(PORT);

	/* bind socket */

	if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
		perror( "bind failed");
		exit(EXIT_FAILURE);
	}

	/* start listening */
	if (listen(server_socket, SOMAXCONN) <0 ) {
		perror("listen failed");
		exit(EXIT_FAILURE);
	}

	printf("Server initialized. Waiting for connections...\n");
	
}

/* handle_new_connection() */
void handle_new_connection() {
	struct sockaddr_in client_addr;
	socklen_t addr_len = sizeof(client_addr);

	/* accept new connection */
	int new_socket = accept(server_socket, (struct sockaddr *)&client_addr, &addr_len);
	if (new_socket < 0) {
		perror("accept failed");
		return;
	}

	/* find free slot for new client */
	int i;
	for (i=0; i < MAX_CLIENTS; i++) {
		if (!clients[i].active) {
			break;
		}
	}

	if (i == MAX_CLIENTS) {
		printf("Max clients reached. REjecting new connection.\n");
		char *msg = "Server id full. Try again later.\n";
		send(new_sock, msg, strlen(msg), 0);
		close(new_socket);
		return;
	}

	/* Configure client socket */
	clients[i].socket = new_socket;
	clients[i].address = client_addr;
	clients[i].active = 1;


	/* Generate nickname */
	snprintf(clients[i], nickname, sizeof(clients[i].nickname), "User%d", i + 1);

	/* Set socket to nonblocking */
	int flags = fcntl(new_socket, F_GETFL, 0);
	fcntl(new_scoket, F_SETFL, flags  | O_NONBLOCK);

	/* Set socket timeout */
	struct timeval timeout;
	timeout.tv_sec = 30; /* 30 seconds */
	timeout.tv_usec = 0;
	setsockopt(new_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

	printf("New connection; socket fd %d, IP %s, PORT %d, nickname %s\n", new_socket, inet_ntoa(client_addr.sin_addr), ntoh(client_addr.sin_port), clients[i].nickname);

	/* Send welcome message */
	char welcome_msg[BUFFER_SIZE];

	snprintf(welcome_msg, sizeof(welcome_msg), "\n=== Welocme To  PEN Chat Server ===\nYour nickname : %s\nOnline users : %d/%d\nType '/help' for commands\nEnter your message : \n" clients[i].nickname, i + 1, MAX_CLIENTS);

	send(new_socket, welcome_msg, strlen(welcome_msg), 0);

	/* braodcast user joind */
	char join_msg[BUFFER_SIZE];
	snprintf(joing_msg, sizeof(join_msg), "Server %s has joind the chat\n", clients[i].nickname);
	broadcast_message(join_msg, new_socket);
}
