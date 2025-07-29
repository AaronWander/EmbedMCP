#include "mcp_server.h"
#include <stdarg.h>
#include <errno.h>

#ifdef DEBUG
void mcp_debug_print(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fflush(stderr);
}
#endif

// Initialize the MCP server
int mcp_server_init(mcp_server_t *server) {
    if (!server) return -1;
    
    memset(server, 0, sizeof(mcp_server_t));
    server->initialized = false;
    server->client_capabilities = NULL;
    server->server_capabilities = mcp_create_server_capabilities();
    server->tools = NULL;
    server->tool_count = 0;
    server->session_count = 0;
    server->client_count = 0;

    // Initialize mutexes for all transport types
    if (pthread_mutex_init(&server->sessions_mutex, NULL) != 0) {
        mcp_debug_print("Failed to initialize sessions mutex\n");
        if (server->server_capabilities) {
            cJSON_Delete(server->server_capabilities);
        }
        return -1;
    }

    if (pthread_mutex_init(&server->clients_mutex, NULL) != 0) {
        mcp_debug_print("Failed to initialize clients mutex\n");
        pthread_mutex_destroy(&server->sessions_mutex);
        if (server->server_capabilities) {
            cJSON_Delete(server->server_capabilities);
        }
        return -1;
    }

    if (!server->server_capabilities) {
        pthread_mutex_destroy(&server->sessions_mutex);
        pthread_mutex_destroy(&server->clients_mutex);
        return -1;
    }
    
    mcp_debug_print("Server initialized with capabilities\n");
    return 0;
}

// Cleanup server resources
void mcp_server_cleanup(mcp_server_t *server) {
    if (!server) return;
    
    if (server->client_capabilities) {
        cJSON_Delete(server->client_capabilities);
    }
    if (server->server_capabilities) {
        cJSON_Delete(server->server_capabilities);
    }
    
    // Cleanup tools if any
    if (server->tools) {
        for (int i = 0; i < server->tool_count; i++) {
            free(server->tools[i].name);
            free(server->tools[i].title);
            free(server->tools[i].description);
            if (server->tools[i].input_schema) {
                cJSON_Delete(server->tools[i].input_schema);
            }
        }
        free(server->tools);
    }

    // Cleanup sessions
    for (int i = 0; i < server->session_count; i++) {
        if (server->sessions[i].client_capabilities) {
            cJSON_Delete(server->sessions[i].client_capabilities);
        }
    }

    // Destroy mutexes
    pthread_mutex_destroy(&server->sessions_mutex);
    pthread_mutex_destroy(&server->clients_mutex);

    mcp_debug_print("Server cleanup completed\n");
}

// Main server loop
int mcp_server_run(mcp_server_t *server) {
    if (!server) return -1;
    
    char *buffer = NULL;
    size_t buffer_size = 0;
    mcp_request_t request;
    
    mcp_debug_print("Starting main server loop\n");
    
    while (g_running) {
        // Read incoming message
        int read_result = mcp_read_message(&buffer, &buffer_size);
        if (read_result < 0) {
            if (errno == EINTR) continue; // Interrupted by signal
            mcp_debug_print("Failed to read message: %s\n", strerror(errno));
            break;
        }
        if (read_result == 0) {
            mcp_debug_print("EOF received, shutting down\n");
            break; // EOF
        }
        
        mcp_debug_print("Received message: %s\n", buffer);
        
        // Parse the request
        if (mcp_parse_request(buffer, &request) != 0) {
            mcp_debug_print("Failed to parse request\n");
            mcp_send_error(NULL, JSONRPC_PARSE_ERROR, "Parse error");
            continue;
        }
        
        // Handle the request based on method
        if (strcmp(request.method, "initialize") == 0) {
            mcp_handle_initialize(server, &request);
        } else if (strcmp(request.method, "notifications/initialized") == 0 ||
                   strcmp(request.method, "initialized") == 0) {
            // Client confirms initialization is complete
            mcp_debug_print("Client initialization confirmed (STDIO)\n");
            server->initialized = true;
            // For STDIO, notifications don't need responses
        } else if (request.is_notification) {
            // Handle other notifications - no response needed for STDIO
            mcp_debug_print("Received notification via STDIO: %s\n", request.method);
        } else if (strcmp(request.method, "tools/list") == 0) {
            if (!server->initialized) {
                mcp_send_error(request.id, JSONRPC_INVALID_REQUEST, "Server not initialized");
            } else {
                mcp_handle_list_tools(server, &request);
            }
        } else if (strcmp(request.method, "tools/call") == 0) {
            if (!server->initialized) {
                mcp_send_error(request.id, JSONRPC_INVALID_REQUEST, "Server not initialized");
            } else {
                mcp_handle_call_tool(server, &request);
            }
        } else {
            mcp_send_error(request.id, JSONRPC_METHOD_NOT_FOUND, "Method not found");
        }
        
        // Cleanup request
        mcp_request_cleanup(&request);
    }
    
    if (buffer) {
        free(buffer);
    }
    
    return 0;
}

// Global server reference for HTTP threads
mcp_server_t *g_http_server = NULL;

// HTTP server main loop
int mcp_server_run_http(mcp_server_t *server, int port) {
    if (!server) return -1;

    g_http_server = server; // Set global reference

    // Initialize HTTP server
    if (http_server_init(server, port) != 0) {
        mcp_debug_print("Failed to initialize HTTP server\n");
        return -1;
    }

    mcp_debug_print("HTTP server started on port %d\n", port);

    // Main accept loop
    while (g_running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        // Accept new connection
        int client_fd = accept(server->server_socket, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            if (errno == EINTR) continue; // Interrupted by signal
            mcp_debug_print("Failed to accept connection: %s\n", strerror(errno));
            continue;
        }

        mcp_debug_print("New HTTP client connected: %s:%d\n",
                       inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        // Create thread for client handling
        pthread_t client_thread;
        int *client_fd_ptr = malloc(sizeof(int));
        if (!client_fd_ptr) {
            close(client_fd);
            continue;
        }
        *client_fd_ptr = client_fd;

        if (pthread_create(&client_thread, NULL, http_client_handler, client_fd_ptr) != 0) {
            mcp_debug_print("Failed to create client thread: %s\n", strerror(errno));
            free(client_fd_ptr);
            close(client_fd);
            continue;
        }

        // Detach thread so it cleans up automatically
        pthread_detach(client_thread);
    }

    // Cleanup HTTP server
    http_server_cleanup(server);

    return 0;
}
