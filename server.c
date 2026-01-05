/* server.c */

#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <errno.h>
#include <signal.h>

#define MAX_CLIENTS 100      // Supports up to 100 concurrent clients
#define BUFFER_SIZE 2048     // Message buffer size
#define PORT 8080            // Server port

// Client structure to store connection info
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

// Function prototypes
void initialize_server();
void handle_new_connection();
void handle_client_message(int client_index);
void broadcast_message(const char *message, int sender_socket);
void remove_client(int client_index);
void cleanup();

int main() {
    printf("=== Multi-Connection PEN Chat Server ===\n");
    printf("Maximum clients: %d\n", MAX_CLIENTS);
    printf("Server starting on port %d...\n", PORT);
    
    // Initialize server
    initialize_server();
    
    // Main server loop
    while (1) {
        // Clear the socket set
        FD_ZERO(&read_fds);
        
        // Add server socket to set
        FD_SET(server_socket, &read_fds);
        max_sd = server_socket;
        
        // Add client sockets to set
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].active) {
                int sd = clients[i].socket;
                FD_SET(sd, &read_fds);
                if (sd > max_sd) {
                    max_sd = sd;
                }
            }
        }
        
        // Wait for activity on any socket (timeout NULL = indefinite wait)
        int activity = select(max_sd + 1, &read_fds, NULL, NULL, NULL);
        
        if (activity < 0 && errno != EINTR) {
            perror("select error");
            break;
        }
        
        // Check if there's a new connection
        if (FD_ISSET(server_socket, &read_fds)) {
            handle_new_connection();
        }
        
        // Check each client for incoming messages
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].active && FD_ISSET(clients[i].socket, &read_fds)) {
                handle_client_message(i);
            }
        }
    }
    
    cleanup();
    return 0;
}

void initialize_server() {
    // Initialize all clients
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].socket = 0;
        clients[i].active = 0;
        memset(clients[i].nickname, 0, sizeof(clients[i].nickname));
    }
    
    // Create server socket
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    
    // Set socket options to reuse address
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }
    
    // Increase receive buffer size
    int rcvbuf = 65536;
    if (setsockopt(server_socket, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf))) {
        perror("setsockopt SO_RCVBUF failed");
    }
    
    // Configure server address
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    
    // Bind socket
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    
    // Start listening
    if (listen(server_socket, SOMAXCONN) < 0) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }
    
    printf("Server initialized. Waiting for connections...\n");
}

void handle_new_connection() {
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    
    // Accept new connection
    int new_socket = accept(server_socket, (struct sockaddr *)&client_addr, &addr_len);
    if (new_socket < 0) {
        perror("accept failed");
        return;
    }
    
    // Find free slot for new client
    int i;
    for (i = 0; i < MAX_CLIENTS; i++) {
        if (!clients[i].active) {
            break;
        }
    }
    
    if (i == MAX_CLIENTS) {
        printf("Max clients reached. Rejecting new connection.\n");
        char *msg = "Server is full. Try again later.\n";
        send(new_socket, msg, strlen(msg), 0);
        close(new_socket);
        return;
    }
    
    // Configure client socket
    clients[i].socket = new_socket;
    clients[i].address = client_addr;
    clients[i].active = 1;
    
    // Generate nickname
    snprintf(clients[i].nickname, sizeof(clients[i].nickname), 
             "User%d", i + 1);
    
    // Set socket to non-blocking
    int flags = fcntl(new_socket, F_GETFL, 0);
    fcntl(new_socket, F_SETFL, flags | O_NONBLOCK);
    
    // Set socket timeout
    struct timeval timeout;
    timeout.tv_sec = 30;  // 30 seconds
    timeout.tv_usec = 0;
    setsockopt(new_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    
    printf("New connection: socket fd %d, IP %s, port %d, nickname %s\n",
           new_socket, 
           inet_ntoa(client_addr.sin_addr), 
           ntohs(client_addr.sin_port),
           clients[i].nickname);
    
    // Send welcome message
    char welcome_msg[BUFFER_SIZE];
    snprintf(welcome_msg, sizeof(welcome_msg),
             "\n=== Welcome PEN to Chat Server ===\n"
             "Your nickname: %s\n"
             "Online users: %d/%d\n"
             "Type '/help' for commands\n"
             "Enter your message:\n",
             clients[i].nickname,
             i + 1, MAX_CLIENTS);
    send(new_socket, welcome_msg, strlen(welcome_msg), 0);
    
    // Broadcast user joined
    char join_msg[BUFFER_SIZE];
    snprintf(join_msg, sizeof(join_msg),
             "[SERVER] %s has joined the chat\n",
             clients[i].nickname);
    broadcast_message(join_msg, new_socket);
}

void handle_client_message(int client_index) {
    char buffer[BUFFER_SIZE];
    int client_socket = clients[client_index].socket;
    
    // Clear buffer
    memset(buffer, 0, BUFFER_SIZE);
    
    // Read message
    int bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
    
    if (bytes_received <= 0) {
        // Connection closed or error
        if (bytes_received == 0) {
            printf("Client %s disconnected\n", clients[client_index].nickname);
        } else {
            perror("recv failed");
        }
        remove_client(client_index);
        return;
    }
    
    // Remove newline character
    buffer[strcspn(buffer, "\n")] = 0;
    buffer[strcspn(buffer, "\r")] = 0;
    
    printf("Message from %s: %s\n", clients[client_index].nickname, buffer);
    
    // Handle commands
    if (buffer[0] == '/') {
        if (strncmp(buffer, "/nick ", 6) == 0) {
            // Change nickname
            char old_nick[32];
            strcpy(old_nick, clients[client_index].nickname);
            strncpy(clients[client_index].nickname, buffer + 6, 31);
            clients[client_index].nickname[31] = '\0';
            
            char msg[BUFFER_SIZE];
            snprintf(msg, sizeof(msg),
                     "[SERVER] %s changed nickname to %s\n",
                     old_nick, clients[client_index].nickname);
            broadcast_message(msg, -1);
        }
        else if (strcmp(buffer, "/list") == 0) {
            // List online users
            char list_msg[BUFFER_SIZE] = "\n=== Online Users ===\n";
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i].active) {
                    char temp[64];
                    snprintf(temp, sizeof(temp), 
                             "%d. %s (Socket: %d)\n",
                             i + 1, clients[i].nickname, clients[i].socket);
                    strcat(list_msg, temp);
                }
            }
            strcat(list_msg, "===================\n");
            send(client_socket, list_msg, strlen(list_msg), 0);
        }
        else if (strcmp(buffer, "/help") == 0) {
            // Show help
            char help_msg[] = "\n=== Available Commands ===\n"
                             "/nick <name> - Change nickname\n"
                             "/list - List online users\n"
                             "/help - Show this help\n"
                             "/quit - Exit chat\n"
                             "========================\n";
            send(client_socket, help_msg, strlen(help_msg), 0);
        }
        else if (strcmp(buffer, "/quit") == 0) {
            // Quit
            char quit_msg[BUFFER_SIZE];
            snprintf(quit_msg, sizeof(quit_msg),
                     "[SERVER] %s has left the chat\n",
                     clients[client_index].nickname);
            broadcast_message(quit_msg, client_socket);
            remove_client(client_index);
        }
        else {
            char error_msg[] = "[SERVER] Unknown command. Type /help for list.\n";
            send(client_socket, error_msg, strlen(error_msg), 0);
        }
        return;
    }
    
    // Normal message - broadcast to all clients
    if (strlen(buffer) > 0) {
        char formatted_msg[BUFFER_SIZE + 50];
        snprintf(formatted_msg, sizeof(formatted_msg),
                 "%s: %s\n", clients[client_index].nickname, buffer);
        broadcast_message(formatted_msg, client_socket);
    }
}

void broadcast_message(const char *message, int sender_socket) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active && clients[i].socket != sender_socket) {
            if (send(clients[i].socket, message, strlen(message), 0) < 0) {
                perror("send failed");
                // Don't remove client here - let select detect the error
            }
        }
    }
}

void remove_client(int client_index) {
    printf("Removing client: %s (socket %d)\n", 
           clients[client_index].nickname, 
           clients[client_index].socket);
    
    // Close socket
    close(clients[client_index].socket);
    
    // Mark as inactive
    clients[client_index].socket = 0;
    clients[client_index].active = 0;
    memset(clients[client_index].nickname, 0, sizeof(clients[client_index].nickname));
}

void cleanup() {
    printf("\nShutting down server...\n");
    
    // Close all client sockets
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active) {
            close(clients[i].socket);
        }
    }
    
    // Close server socket
    if (server_socket > 0) {
        close(server_socket);
    }
    
    printf("Server shutdown complete.\n");
}
