#include "mcp_server.h"
#include <sys/select.h>
#include <fcntl.h>
#include <errno.h>

// Initialize HTTP server
int http_server_init(mcp_server_t *server, int port) {
    if (!server) return -1;
    
    server->server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server->server_socket < 0) {
        mcp_debug_print("Failed to create socket: %s\n", strerror(errno));
        return -1;
    }
    
    // Set socket options
    int opt = 1;
    if (setsockopt(server->server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        mcp_debug_print("Failed to set socket options: %s\n", strerror(errno));
        close(server->server_socket);
        return -1;
    }
    
    // Bind to address
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; // Accept connections from any interface
    address.sin_port = htons(port);
    
    if (bind(server->server_socket, (struct sockaddr*)&address, sizeof(address)) < 0) {
        mcp_debug_print("Failed to bind socket: %s\n", strerror(errno));
        close(server->server_socket);
        return -1;
    }
    
    // Listen for connections
    if (listen(server->server_socket, MAX_CLIENTS) < 0) {
        mcp_debug_print("Failed to listen on socket: %s\n", strerror(errno));
        close(server->server_socket);
        return -1;
    }
    
    server->http_port = port;
    // Note: session_count and client_count already initialized in mcp_server_init
    // Note: mutexes already initialized in mcp_server_init
    
    mcp_debug_print("HTTP server listening on 0.0.0.0:%d\n", port);
    return 0;
}

// Cleanup HTTP server
void http_server_cleanup(mcp_server_t *server) {
    if (!server) return;
    
    if (server->server_socket >= 0) {
        close(server->server_socket);
        server->server_socket = -1;
    }
    
    // Close all client connections
    pthread_mutex_lock(&server->clients_mutex);
    for (int i = 0; i < server->client_count; i++) {
        if (server->clients[i].socket_fd >= 0) {
            close(server->clients[i].socket_fd);
        }
    }
    server->client_count = 0;
    pthread_mutex_unlock(&server->clients_mutex);

    // Note: Sessions and mutexes will be cleaned up in mcp_server_cleanup
    
    mcp_debug_print("HTTP server cleanup completed\n");
}

// Generate a random session ID
char *http_generate_session_id(void) {
    static const char charset[] = "0123456789abcdef";
    char *session_id = malloc(SESSION_ID_LENGTH + 1);
    if (!session_id) return NULL;
    
    srand(time(NULL));
    for (int i = 0; i < SESSION_ID_LENGTH; i++) {
        session_id[i] = charset[rand() % (sizeof(charset) - 1)];
    }
    session_id[SESSION_ID_LENGTH] = '\0';
    
    return session_id;
}

// Add a new client
int mcp_add_client(mcp_server_t *server, int socket_fd) {
    if (!server) return -1;
    
    pthread_mutex_lock(&server->clients_mutex);
    
    if (server->client_count >= MAX_CLIENTS) {
        pthread_mutex_unlock(&server->clients_mutex);
        return -1;
    }
    
    mcp_client_t *client = &server->clients[server->client_count];
    client->socket_fd = socket_fd;
    client->sse_connected = false;
    client->last_activity = time(NULL);
    client->session_id[0] = '\0'; // No session ID for raw client connections
    
    server->client_count++;
    
    pthread_mutex_unlock(&server->clients_mutex);
    
    mcp_debug_print("Added client %d\n", socket_fd);
    return 0;
}

// Remove a client
void mcp_remove_client(mcp_server_t *server, int socket_fd) {
    if (!server) return;
    
    pthread_mutex_lock(&server->clients_mutex);
    
    for (int i = 0; i < server->client_count; i++) {
        if (server->clients[i].socket_fd == socket_fd) {
            mcp_debug_print("Removing client %d\n", socket_fd);
            
            // Close socket
            close(server->clients[i].socket_fd);
            
            // Shift remaining clients
            for (int j = i; j < server->client_count - 1; j++) {
                server->clients[j] = server->clients[j + 1];
            }
            server->client_count--;
            break;
        }
    }
    
    pthread_mutex_unlock(&server->clients_mutex);
}

// Find client by session ID
mcp_client_t *mcp_find_client(mcp_server_t *server, const char *session_id) {
    if (!server || !session_id) return NULL;
    
    pthread_mutex_lock(&server->clients_mutex);
    
    for (int i = 0; i < server->client_count; i++) {
        if (strcmp(server->clients[i].session_id, session_id) == 0) {
            pthread_mutex_unlock(&server->clients_mutex);
            return &server->clients[i];
        }
    }
    
    pthread_mutex_unlock(&server->clients_mutex);
    return NULL;
}

// Find client by socket
mcp_client_t *mcp_find_client_by_socket(mcp_server_t *server, int socket_fd) {
    if (!server) return NULL;
    
    pthread_mutex_lock(&server->clients_mutex);
    
    for (int i = 0; i < server->client_count; i++) {
        if (server->clients[i].socket_fd == socket_fd) {
            pthread_mutex_unlock(&server->clients_mutex);
            return &server->clients[i];
        }
    }
    
    pthread_mutex_unlock(&server->clients_mutex);
    return NULL;
}
