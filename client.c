/* client.c */

#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <pthread.h>
#include <errno.h>

#define BUFFER_SIZE 2048
#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080

int client_socket;
volatile int running = 1;

// Thread function to receive messages
void* receive_messages(void* arg) {
    char buffer[BUFFER_SIZE];
    
    while (running) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
        
        if (bytes_received > 0) {
            printf("%s", buffer);
            fflush(stdout);
        } else if (bytes_received == 0) {
            printf("\nServer disconnected.\n");
            running = 0;
            break;
        } else {
            if (errno != EWOULDBLOCK && errno != EAGAIN) {
                perror("recv failed");
                running = 0;
                break;
            }
        }
        
        // Small delay to prevent CPU spinning
        usleep(10000);  // 10ms
    }
    
    return NULL;
}

// Thread function to send messages
void* send_messages(void* arg) {
    char buffer[BUFFER_SIZE];
    
    while (running) {
        // Show prompt
        printf("> ");
        fflush(stdout);
        
        // Read user input
        if (fgets(buffer, BUFFER_SIZE, stdin) == NULL) {
            break;
        }
        
        // Remove newline
        buffer[strcspn(buffer, "\n")] = 0;
        
        // Check for exit command
        if (strcmp(buffer, "/quit") == 0 || strcmp(buffer, "/exit") == 0) {
            running = 0;
            break;
        }
        
        // Send message to server
        if (send(client_socket, buffer, strlen(buffer), 0) < 0) {
            perror("send failed");
            running = 0;
            break;
        }
    }
    
    return NULL;
}

int main() {
    printf("=== Chat Client ===\n");
    printf("Connecting to server at %s:%d...\n", SERVER_IP, SERVER_PORT);
    
    // Create socket
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }
    
    // Set socket to non-blocking
    int flags = fcntl(client_socket, F_GETFL, 0);
    fcntl(client_socket, F_SETFL, flags | O_NONBLOCK);
    
    // Configure server address
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        perror("invalid address");
        close(client_socket);
        exit(EXIT_FAILURE);
    }
    
    // Connect to server
    printf("Connecting...\n");
    int connect_result = connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr));
    
    if (connect_result < 0 && errno != EINPROGRESS) {
        perror("connection failed");
        close(client_socket);
        exit(EXIT_FAILURE);
    }
    
    // Wait for connection to complete
    fd_set write_fds;
    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    
    FD_ZERO(&write_fds);
    FD_SET(client_socket, &write_fds);
    
    int select_result = select(client_socket + 1, NULL, &write_fds, NULL, &timeout);
    
    if (select_result <= 0) {
        printf("Connection timeout or error\n");
        close(client_socket);
        exit(EXIT_FAILURE);
    }
    
    int so_error;
    socklen_t len = sizeof(so_error);
    getsockopt(client_socket, SOL_SOCKET, SO_ERROR, &so_error, &len);
    
    if (so_error != 0) {
        printf("Connection failed: %s\n", strerror(so_error));
        close(client_socket);
        exit(EXIT_FAILURE);
    }
    
    printf("Connected to server!\n");
    printf("Type /help for commands, /quit to exit\n\n");
    
    // Create threads for sending and receiving
    pthread_t recv_thread, send_thread;
    
    if (pthread_create(&recv_thread, NULL, receive_messages, NULL) != 0) {
        perror("pthread_create failed");
        close(client_socket);
        exit(EXIT_FAILURE);
    }
    
    if (pthread_create(&send_thread, NULL, send_messages, NULL) != 0) {
        perror("pthread_create failed");
        running = 0;
        pthread_join(recv_thread, NULL);
        close(client_socket);
        exit(EXIT_FAILURE);
    }
    
    // Wait for threads to finish
    pthread_join(send_thread, NULL);
    pthread_join(recv_thread, NULL);
    
    // Cleanup
    printf("\nDisconnecting...\n");
    close(client_socket);
    
    return 0;
}
