#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <time.h>

#define MAX_CLIENTS 50
#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080

typedef struct {
    int id;
    int socket;
} ClientInfo;

void* client_thread(void* arg) {
    ClientInfo* info = (ClientInfo*)arg;
    int client_id = info->id;
    int sock = info->socket;
    
    char buffer[256];
    
    // Read welcome message
    recv(sock, buffer, sizeof(buffer) - 1, 0);
    
    // Send some messages
    for (int i = 0; i < 5; i++) {
        snprintf(buffer, sizeof(buffer), "Hello from Client %d, Message %d", client_id, i + 1);
        send(sock, buffer, strlen(buffer), 0);
        sleep(rand() % 3 + 1);  // Random delay 1-3 seconds
    }
    
    // Say goodbye
    snprintf(buffer, sizeof(buffer), "/quit");
    send(sock, buffer, strlen(buffer), 0);
    
    close(sock);
    printf("Client %d finished\n", client_id);
    
    free(info);
    return NULL;
}

int main() {
    printf("=== Stress Test: Connecting %d clients ===\n", MAX_CLIENTS);
    
    pthread_t threads[MAX_CLIENTS];
    int thread_count = 0;
    
    srand(time(NULL));
    
    for (int i = 0; i < MAX_CLIENTS; i++) {
        // Create socket
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            perror("socket failed");
            continue;
        }
        
        // Configure server address
        struct sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(SERVER_PORT);
        inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);
        
        // Connect
        if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            printf("Client %d: Connection failed\n", i + 1);
            close(sock);
            continue;
        }
        
        // Create client info
        ClientInfo* info = malloc(sizeof(ClientInfo));
        info->id = i + 1;
        info->socket = sock;
        
        // Create thread
        if (pthread_create(&threads[thread_count], NULL, client_thread, info) == 0) {
            thread_count++;
            printf("Client %d connected\n", i + 1);
            usleep(100000);  // 100ms delay between connections
        } else {
            free(info);
            close(sock);
        }
    }
    
    printf("\n%d clients connected. Waiting for them to finish...\n", thread_count);
    
    // Wait for all threads
    for (int i = 0; i < thread_count; i++) {
        pthread_join(threads[i], NULL);
    }
    
    printf("Stress test completed!\n");
    return 0;
}
