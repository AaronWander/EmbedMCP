#include "transport/http_transport.h"
#include "hal/platform_hal.h"
#include "utils/logging.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/select.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <netinet/in.h>

// HTTP transport interface implementation
const mcp_transport_interface_t mcp_http_transport_interface = {
    .init = mcp_http_transport_init_impl,
    .start = mcp_http_transport_start_impl,
    .stop = mcp_http_transport_stop_impl,
    .send = mcp_http_transport_send_impl,
    .close_connection = mcp_http_transport_close_connection_impl,
    .get_stats = mcp_http_transport_get_stats_impl,
    .cleanup = mcp_http_transport_cleanup_impl
};

// HTTP-specific functions
int mcp_http_transport_init_impl(mcp_transport_t *transport, const mcp_transport_config_t *config) {
    if (!transport || !config || config->type != MCP_TRANSPORT_HTTP) return -1;
    
    mcp_http_transport_data_t *data = calloc(1, sizeof(mcp_http_transport_data_t));
    if (!data) return -1;
    
    // Store configuration
    transport->config = calloc(1, sizeof(mcp_transport_config_t));
    if (transport->config) {
        *transport->config = *config;
        // Duplicate string fields
        if (config->config.http.bind_address) {
            transport->config->config.http.bind_address = strdup(config->config.http.bind_address);
        }
    }
    
    // Initialize HTTP data
    data->server_socket = -1;
    data->port = config->config.http.port;
    data->bind_address = config->config.http.bind_address ? strdup(config->config.http.bind_address) : strdup("0.0.0.0");
    data->enable_cors = config->config.http.enable_cors;
    data->max_request_size = config->config.http.max_request_size;
    data->server_running = false;
    data->connections = NULL;
    data->connection_count = 0;
    data->max_connections = config->max_connections;
    
    // Initialize mutex
    if (pthread_mutex_init(&data->connections_mutex, NULL) != 0) {
        free(data->bind_address);
        free(data);
        return -1;
    }
    
    // Create CORS headers if enabled
    if (data->enable_cors) {
        data->cors_headers = strdup(
            "Access-Control-Allow-Origin: *\r\n"
            "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
            "Access-Control-Allow-Headers: Content-Type, Authorization\r\n"
        );
    }
    
    data->server_header = strdup("Server: EmbedMCP/1.0\r\n");
    
    transport->private_data = data;
    
    mcp_log_info("HTTP transport initialized on %s:%d", data->bind_address, data->port);
    
    return 0;
}

int mcp_http_transport_start_impl(mcp_transport_t *transport) {
    if (!transport || !transport->private_data) return -1;
    
    mcp_http_transport_data_t *data = (mcp_http_transport_data_t*)transport->private_data;
    
    // Create server socket
    data->server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (data->server_socket < 0) {
        mcp_log_error("Failed to create HTTP server socket: %s", strerror(errno));
        return -1;
    }
    
    // Set socket options
    int opt = 1;
    if (setsockopt(data->server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        mcp_log_error("Failed to set socket options: %s", strerror(errno));
        close(data->server_socket);
        data->server_socket = -1;
        return -1;
    }
    
    // Bind to address
    memset(&data->server_addr, 0, sizeof(data->server_addr));
    data->server_addr.sin_family = AF_INET;
    data->server_addr.sin_port = htons(data->port);
    
    if (strcmp(data->bind_address, "0.0.0.0") == 0) {
        data->server_addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        if (inet_pton(AF_INET, data->bind_address, &data->server_addr.sin_addr) <= 0) {
            mcp_log_error("Invalid bind address: %s", data->bind_address);
            close(data->server_socket);
            data->server_socket = -1;
            return -1;
        }
    }
    
    if (bind(data->server_socket, (struct sockaddr*)&data->server_addr, sizeof(data->server_addr)) < 0) {
        mcp_log_error("Failed to bind HTTP server socket: %s", strerror(errno));
        close(data->server_socket);
        data->server_socket = -1;
        return -1;
    }
    
    // Listen for connections
    if (listen(data->server_socket, (int)data->max_connections) < 0) {
        mcp_log_error("Failed to listen on HTTP server socket: %s", strerror(errno));
        close(data->server_socket);
        data->server_socket = -1;
        return -1;
    }
    
    // Allocate connections array
    data->connections = calloc(data->max_connections, sizeof(mcp_connection_t*));
    if (!data->connections) {
        close(data->server_socket);
        data->server_socket = -1;
        return -1;
    }
    
    // Start server thread using HAL
    const mcp_platform_hal_t *hal = mcp_platform_get_hal();
    if (!hal) {
        mcp_log_error("Failed to get platform HAL");
        return -1;
    }

    data->server_running = true;
    void *thread_handle;
    int thread_result = hal->thread.create(&thread_handle, mcp_http_server_thread, transport, 0);
    if (thread_result == 0) {
        // Store thread handle in a way compatible with pthread_t
        data->server_thread = *(pthread_t*)&thread_handle;
    } else {
        mcp_log_error("Failed to create HTTP server thread");
        data->server_running = false;
        free(data->connections);
        data->connections = NULL;
        close(data->server_socket);
        data->server_socket = -1;
        return -1;
    }
    
    mcp_log_info("HTTP server started on %s:%d", data->bind_address, data->port);
    
    return 0;
}

int mcp_http_transport_stop_impl(mcp_transport_t *transport) {
    if (!transport || !transport->private_data) return -1;
    
    mcp_http_transport_data_t *data = (mcp_http_transport_data_t*)transport->private_data;
    
    // Stop server thread
    data->server_running = false;
    
    // Close server socket to unblock accept()
    if (data->server_socket >= 0) {
        close(data->server_socket);
        data->server_socket = -1;
    }
    
    // Wait for server thread to finish
    if (pthread_join(data->server_thread, NULL) != 0) {
        mcp_log_warn("Failed to join HTTP server thread");
    }
    
    // Close all client connections
    pthread_mutex_lock(&data->connections_mutex);
    for (size_t i = 0; i < data->max_connections; i++) {
        if (data->connections[i]) {
            mcp_http_transport_close_connection_impl(data->connections[i]);
        }
    }
    pthread_mutex_unlock(&data->connections_mutex);
    
    mcp_log_info("HTTP server stopped");
    
    return 0;
}

int mcp_http_transport_send_impl(mcp_connection_t *connection, const char *message, size_t length) {
    if (!connection || !message || length == 0) return -1;
    
    mcp_http_connection_data_t *conn_data = (mcp_http_connection_data_t*)connection->private_data;
    if (!conn_data || conn_data->socket_fd < 0) return -1;
    
    // Create HTTP response
    mcp_http_response_t response = {0};
    response.status_code = HTTP_STATUS_OK;
    response.body = (char*)message;
    response.body_length = length;
    response.close_connection = false;
    
    int result = mcp_http_send_response(connection, &response);
    
    if (result == 0) {
        connection->messages_sent++;
        connection->bytes_sent += length;
        connection->last_activity = time(NULL);
    }
    
    return result;
}

int mcp_http_transport_close_connection_impl(mcp_connection_t *connection) {
    if (!connection) return -1;
    
    mcp_http_connection_data_t *conn_data = (mcp_http_connection_data_t*)connection->private_data;
    if (conn_data) {
        if (conn_data->socket_fd >= 0) {
            close(conn_data->socket_fd);
            conn_data->socket_fd = -1;
        }
        
        if (conn_data->thread_active) {
            // Cancel the handler thread
            pthread_cancel(conn_data->handler_thread);
            pthread_join(conn_data->handler_thread, NULL);
            conn_data->thread_active = false;
        }
        
        free(conn_data->request_buffer);
        free(conn_data);
    }
    
    connection->is_active = false;
    
    // Notify connection closed
    if (connection->transport && connection->transport->on_connection_closed) {
        connection->transport->on_connection_closed(connection, connection->transport->user_data);
    }
    
    return 0;
}

int mcp_http_transport_get_stats_impl(mcp_transport_t *transport, void *stats) {
    // Implementation for getting HTTP transport statistics
    (void)transport;
    (void)stats;
    return 0;
}

void mcp_http_transport_cleanup_impl(mcp_transport_t *transport) {
    if (!transport || !transport->private_data) return;
    
    mcp_http_transport_data_t *data = (mcp_http_transport_data_t*)transport->private_data;
    
    // Cleanup mutex
    pthread_mutex_destroy(&data->connections_mutex);
    
    // Free allocated memory
    free(data->bind_address);
    free(data->cors_headers);
    free(data->server_header);
    free(data->connections);
    
    // Free private data
    free(data);
    transport->private_data = NULL;
}

// HTTP server thread
void *mcp_http_server_thread(void *arg) {
    mcp_transport_t *transport = (mcp_transport_t*)arg;
    mcp_http_transport_data_t *data = (mcp_http_transport_data_t*)transport->private_data;

    mcp_log_info("HTTP server thread started");

    while (data->server_running) {
        // Accept new connections
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(data->server_socket, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            if (data->server_running) {
                mcp_log_error("Failed to accept HTTP connection: %s", strerror(errno));
            }
            continue;
        }

        // Create connection
        mcp_connection_t *connection = mcp_http_connection_create(transport, client_fd, &client_addr);
        if (!connection) {
            mcp_log_error("Failed to create HTTP connection");
            close(client_fd);
            continue;
        }

        // Add to connections list
        if (mcp_http_add_connection(transport, connection) != 0) {
            mcp_log_error("Failed to add HTTP connection");
            mcp_http_connection_destroy(connection);
            continue;
        }

        // Notify connection opened
        if (transport->on_connection_opened) {
            transport->on_connection_opened(connection, transport->user_data);
        }

        transport->connections_opened++;
    }

    mcp_log_info("HTTP server thread stopped");
    return NULL;
}

// HTTP connection management
mcp_connection_t *mcp_http_connection_create(mcp_transport_t *transport, int socket_fd,
                                           const struct sockaddr_in *client_addr) {
    if (!transport || socket_fd < 0 || !client_addr) return NULL;

    mcp_connection_t *connection = calloc(1, sizeof(mcp_connection_t));
    if (!connection) return NULL;

    mcp_http_connection_data_t *conn_data = calloc(1, sizeof(mcp_http_connection_data_t));
    if (!conn_data) {
        free(connection);
        return NULL;
    }

    // Generate connection ID
    char conn_id[64];
    snprintf(conn_id, sizeof(conn_id), "http-%d-%ld", socket_fd, time(NULL));

    connection->transport = transport;
    connection->connection_id = strdup(conn_id);
    connection->session_id = NULL;
    connection->is_active = true;
    connection->created_time = time(NULL);
    connection->last_activity = time(NULL);
    connection->private_data = conn_data;
    connection->messages_sent = 0;
    connection->messages_received = 0;
    connection->bytes_sent = 0;
    connection->bytes_received = 0;

    // Initialize connection data
    conn_data->socket_fd = socket_fd;
    conn_data->client_addr = *client_addr;
    conn_data->thread_active = false;
    conn_data->keep_alive = false;
    conn_data->last_request_time = time(NULL);
    conn_data->requests_handled = 0;
    conn_data->request_buffer = NULL;
    conn_data->request_buffer_size = 0;
    conn_data->request_buffer_capacity = 8192; // 8KB initial buffer

    // Allocate request buffer
    conn_data->request_buffer = malloc(conn_data->request_buffer_capacity);
    if (!conn_data->request_buffer) {
        free(connection->connection_id);
        free(conn_data);
        free(connection);
        return NULL;
    }

    // Start connection handler thread using HAL
    const mcp_platform_hal_t *hal = mcp_platform_get_hal();
    if (!hal) {
        mcp_log_error("Failed to get platform HAL");
        free(conn_data->request_buffer);
        free(connection->connection_id);
        free(conn_data);
        free(connection);
        return NULL;
    }

    void *thread_handle;
    int thread_result = hal->thread.create(&thread_handle, mcp_http_connection_handler_thread, connection, 0);
    if (thread_result == 0) {
        conn_data->handler_thread = *(pthread_t*)&thread_handle;
        conn_data->thread_active = true;
    } else {
        mcp_log_error("Failed to create HTTP connection handler thread");
        free(conn_data->request_buffer);
        free(connection->connection_id);
        free(conn_data);
        free(connection);
        return NULL;
    }

    return connection;
}

void mcp_http_connection_destroy(mcp_connection_t *connection) {
    if (!connection) return;

    mcp_http_transport_close_connection_impl(connection);

    free(connection->connection_id);
    free(connection->session_id);
    free(connection);
}

int mcp_http_add_connection(mcp_transport_t *transport, mcp_connection_t *connection) {
    if (!transport || !connection) return -1;

    mcp_http_transport_data_t *data = (mcp_http_transport_data_t*)transport->private_data;
    if (!data) return -1;

    pthread_mutex_lock(&data->connections_mutex);

    // Find empty slot
    for (size_t i = 0; i < data->max_connections; i++) {
        if (!data->connections[i]) {
            data->connections[i] = connection;
            data->connection_count++;
            pthread_mutex_unlock(&data->connections_mutex);
            return 0;
        }
    }

    pthread_mutex_unlock(&data->connections_mutex);

    mcp_log_error("Maximum HTTP connections reached");
    return -1;
}

int mcp_http_remove_connection(mcp_transport_t *transport, mcp_connection_t *connection) {
    if (!transport || !connection) return -1;

    mcp_http_transport_data_t *data = (mcp_http_transport_data_t*)transport->private_data;
    if (!data) return -1;

    pthread_mutex_lock(&data->connections_mutex);

    // Find and remove connection
    for (size_t i = 0; i < data->max_connections; i++) {
        if (data->connections[i] == connection) {
            data->connections[i] = NULL;
            data->connection_count--;
            pthread_mutex_unlock(&data->connections_mutex);
            return 0;
        }
    }

    pthread_mutex_unlock(&data->connections_mutex);

    return -1;
}

// HTTP connection handler thread
void *mcp_http_connection_handler_thread(void *arg) {
    mcp_connection_t *connection = (mcp_connection_t*)arg;
    mcp_http_connection_data_t *conn_data = (mcp_http_connection_data_t*)connection->private_data;

    char buffer[8192];
    ssize_t bytes_read;

    while (connection->is_active && conn_data->socket_fd >= 0) {
        // Read HTTP request
        bytes_read = recv(conn_data->socket_fd, buffer, sizeof(buffer) - 1, 0);
        if (bytes_read <= 0) {
            if (bytes_read < 0 && errno != ECONNRESET) {
                mcp_log_error("HTTP connection read error: %s", strerror(errno));
            }
            break;
        }

        buffer[bytes_read] = '\0';
        connection->bytes_received += bytes_read;
        connection->last_activity = time(NULL);

        // Parse HTTP request
        mcp_http_request_t request = {0};
        if (mcp_http_parse_request(buffer, &request) != 0) {
            mcp_http_send_error_response(connection, HTTP_STATUS_BAD_REQUEST, "Bad Request");
            mcp_http_request_cleanup(&request);
            continue;
        }

        // Handle different HTTP methods
        int result = 0;
        if (strcmp(request.method, "POST") == 0) {
            result = mcp_http_handle_post(connection, &request);
        } else if (strcmp(request.method, "GET") == 0) {
            result = mcp_http_handle_get(connection, &request);
        } else if (strcmp(request.method, "OPTIONS") == 0) {
            result = mcp_http_handle_options(connection, &request);
        } else {
            result = mcp_http_send_error_response(connection, HTTP_STATUS_METHOD_NOT_ALLOWED, "Method Not Allowed");
        }

        mcp_http_request_cleanup(&request);

        if (result != 0) {
            break;
        }

        conn_data->requests_handled++;
        conn_data->last_request_time = time(NULL);

        // Check if connection should be kept alive
        if (!conn_data->keep_alive) {
            break;
        }
    }

    // Remove connection from transport
    mcp_http_remove_connection(connection->transport, connection);

    // Notify connection closed
    if (connection->transport->on_connection_closed) {
        connection->transport->on_connection_closed(connection, connection->transport->user_data);
    }

    connection->transport->connections_closed++;

    return NULL;
}

// HTTP request parsing
int mcp_http_parse_request(const char *raw_request, mcp_http_request_t *request) {
    if (!raw_request || !request) return -1;

    // Initialize request structure
    memset(request, 0, sizeof(mcp_http_request_t));

    // Parse request line
    char *request_copy = strdup(raw_request);
    if (!request_copy) return -1;

    char *line = strtok(request_copy, "\r\n");
    if (!line) {
        free(request_copy);
        return -1;
    }

    // Parse method, path, and protocol
    char *method = strtok(line, " ");
    char *path = strtok(NULL, " ");
    char *protocol = strtok(NULL, " ");

    if (!method || !path || !protocol) {
        free(request_copy);
        return -1;
    }

    request->method = strdup(method);
    request->path = strdup(path);
    request->protocol = strdup(protocol);

    // Parse headers
    char *headers_start = strstr(raw_request, "\r\n") + 2;
    char *body_start = strstr(headers_start, "\r\n\r\n");

    if (body_start) {
        size_t headers_len = body_start - headers_start;
        request->headers = malloc(headers_len + 1);
        if (request->headers) {
            strncpy(request->headers, headers_start, headers_len);
            request->headers[headers_len] = '\0';
        }

        // Parse body
        body_start += 4; // Skip \r\n\r\n
        request->body_length = strlen(body_start);
        if (request->body_length > 0) {
            request->body = strdup(body_start);
        }
    }

    // Parse specific headers
    if (request->headers) {
        char *content_type = strstr(request->headers, "Content-Type:");
        if (content_type) {
            content_type += 13; // Skip "Content-Type:"
            while (*content_type == ' ') content_type++; // Skip spaces
            char *end = strstr(content_type, "\r\n");
            if (end) {
                size_t len = end - content_type;
                request->content_type = malloc(len + 1);
                if (request->content_type) {
                    strncpy(request->content_type, content_type, len);
                    request->content_type[len] = '\0';
                }
            }
        }

        char *content_length = strstr(request->headers, "Content-Length:");
        if (content_length) {
            content_length += 15; // Skip "Content-Length:"
            request->content_length = strtoul(content_length, NULL, 10);
        }

        char *connection = strstr(request->headers, "Connection:");
        if (connection) {
            connection += 11; // Skip "Connection:"
            while (*connection == ' ') connection++; // Skip spaces
            char *end = strstr(connection, "\r\n");
            if (end) {
                size_t len = end - connection;
                request->connection = malloc(len + 1);
                if (request->connection) {
                    strncpy(request->connection, connection, len);
                    request->connection[len] = '\0';
                }
            }
        }
    }

    free(request_copy);
    return 0;
}

void mcp_http_request_cleanup(mcp_http_request_t *request) {
    if (!request) return;

    free(request->method);
    free(request->path);
    free(request->protocol);
    free(request->headers);
    free(request->body);
    free(request->content_type);
    free(request->connection);

    memset(request, 0, sizeof(mcp_http_request_t));
}

// HTTP response sending
int mcp_http_send_response(mcp_connection_t *connection, const mcp_http_response_t *response) {
    if (!connection || !response) return -1;

    mcp_http_connection_data_t *conn_data = (mcp_http_connection_data_t*)connection->private_data;
    if (!conn_data || conn_data->socket_fd < 0) return -1;

    mcp_http_transport_data_t *transport_data = (mcp_http_transport_data_t*)connection->transport->private_data;

    // Create response headers
    char *headers = mcp_http_create_response_headers(
        response->status_code,
        "application/json",
        response->body_length,
        transport_data->enable_cors
    );

    if (!headers) return -1;

    // Send status line
    char status_line[256];
    snprintf(status_line, sizeof(status_line), "HTTP/1.1 %d %s\r\n",
             response->status_code,
             response->status_message ? response->status_message : "OK");

    if (send(conn_data->socket_fd, status_line, strlen(status_line), 0) < 0) {
        free(headers);
        return -1;
    }

    // Send headers
    if (send(conn_data->socket_fd, headers, strlen(headers), 0) < 0) {
        free(headers);
        return -1;
    }

    // Send server header
    if (transport_data->server_header) {
        if (send(conn_data->socket_fd, transport_data->server_header, strlen(transport_data->server_header), 0) < 0) {
            free(headers);
            return -1;
        }
    }

    // Send CORS headers if enabled
    if (transport_data->cors_headers) {
        if (send(conn_data->socket_fd, transport_data->cors_headers, strlen(transport_data->cors_headers), 0) < 0) {
            free(headers);
            return -1;
        }
    }

    // Send connection header
    const char *conn_header = response->close_connection ? "Connection: close\r\n" : "Connection: keep-alive\r\n";
    if (send(conn_data->socket_fd, conn_header, strlen(conn_header), 0) < 0) {
        free(headers);
        return -1;
    }

    // Send empty line to end headers
    if (send(conn_data->socket_fd, "\r\n", 2, 0) < 0) {
        free(headers);
        return -1;
    }

    // Send body if present
    if (response->body && response->body_length > 0) {
        if (send(conn_data->socket_fd, response->body, response->body_length, 0) < 0) {
            free(headers);
            return -1;
        }
    }

    free(headers);

    // Update connection state
    conn_data->keep_alive = !response->close_connection;

    return 0;
}

// HTTP utility functions
char *mcp_http_create_response_headers(int status_code, const char *content_type,
                                      size_t content_length, bool enable_cors) {
    (void)status_code; // Status code is handled in status line

    char *headers = malloc(1024);
    if (!headers) return NULL;

    snprintf(headers, 1024,
             "Content-Type: %s\r\n"
             "Content-Length: %zu\r\n",
             content_type ? content_type : "application/json",
             content_length);

    if (enable_cors) {
        strcat(headers, "Access-Control-Allow-Origin: *\r\n");
        strcat(headers, "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n");
        strcat(headers, "Access-Control-Allow-Headers: Content-Type, Authorization\r\n");
    }

    return headers;
}

int mcp_http_send_error_response(mcp_connection_t *connection, int status_code, const char *message) {
    if (!connection) return -1;

    // Create error JSON
    char error_body[512];
    snprintf(error_body, sizeof(error_body),
             "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":%d,\"message\":\"%s\"},\"id\":null}",
             status_code, message ? message : "Unknown error");

    mcp_http_response_t response = {0};
    response.status_code = status_code;
    response.body = error_body;
    response.body_length = strlen(error_body);
    response.close_connection = true;

    return mcp_http_send_response(connection, &response);
}

int mcp_http_send_json_response(mcp_connection_t *connection, int status_code, const char *json_data) {
    if (!connection || !json_data) return -1;

    mcp_http_response_t response = {0};
    response.status_code = status_code;
    response.body = (char*)json_data;
    response.body_length = strlen(json_data);
    response.close_connection = false;

    return mcp_http_send_response(connection, &response);
}

// HTTP method handlers
int mcp_http_handle_post(mcp_connection_t *connection, const mcp_http_request_t *request) {
    if (!connection || !request) return -1;

    // Check if this is a valid MCP endpoint
    bool is_mcp_endpoint = (strcmp(request->path, "/") == 0 ||
                           strcmp(request->path, "/mcp") == 0);
    bool is_sse_endpoint = (strcmp(request->path, "/sse") == 0);
    bool is_messages_endpoint = (strcmp(request->path, "/messages") == 0);

    if (!is_mcp_endpoint && !is_sse_endpoint && !is_messages_endpoint) {
        return mcp_http_send_error_response(connection, HTTP_STATUS_NOT_FOUND, "Endpoint not found");
    }

    // Handle SSE endpoint specially
    if (is_sse_endpoint) {
        return mcp_http_handle_sse_request(connection, request);
    }

    // Check if this is a JSON-RPC request
    if (!request->body || request->body_length == 0) {
        return mcp_http_send_error_response(connection, HTTP_STATUS_BAD_REQUEST, "Missing request body");
    }

    // Skip Content-Type check for compatibility with MCP Inspector and other clients

    // Forward the JSON-RPC message to the transport's message handler
    if (connection->transport->on_message) {
        connection->transport->on_message(request->body, request->body_length, connection, connection->transport->user_data);
    }

    connection->messages_received++;

    return 0; // Response will be sent by the message handler
}

int mcp_http_handle_get(mcp_connection_t *connection, const mcp_http_request_t *request) {
    if (!connection || !request) return -1;

    // Health check endpoint
    if (strcmp(request->path, "/health") == 0) {
        const char *health_response = "{\"status\":\"ok\",\"server\":\"EmbedMCP\",\"version\":\"1.0.0\"}";
        return mcp_http_send_json_response(connection, HTTP_STATUS_OK, health_response);
    }

    // MCP endpoint info (for GET requests to main endpoints)
    if (strcmp(request->path, "/") == 0 || strcmp(request->path, "/mcp") == 0) {
        const char *mcp_info = "{"
            "\"server\":\"EmbedMCP\","
            "\"version\":\"1.0.0\","
            "\"protocol\":\"MCP\","
            "\"transport\":\"HTTP\","
            "\"endpoints\":{\"/\":\"MCP JSON-RPC\",\"/mcp\":\"MCP JSON-RPC\",\"/health\":\"Health Check\"}"
            "}";
        return mcp_http_send_json_response(connection, HTTP_STATUS_OK, mcp_info);
    }

    // Default: return method not allowed for other GET requests
    return mcp_http_send_error_response(connection, HTTP_STATUS_NOT_FOUND, "Endpoint not found");
}

int mcp_http_handle_options(mcp_connection_t *connection, const mcp_http_request_t *request) {
    (void)request; // OPTIONS requests don't need to examine the request details

    if (!connection) return -1;

    // Send CORS preflight response
    mcp_http_response_t response = {0};
    response.status_code = HTTP_STATUS_OK;
    response.body = "";
    response.body_length = 0;
    response.close_connection = false;

    return mcp_http_send_response(connection, &response);
}

// Handle SSE endpoint for Streamable HTTP
int mcp_http_handle_sse_request(mcp_connection_t *connection, const mcp_http_request_t *request) {
    if (!connection || !request) return -1;

    mcp_http_connection_data_t *conn_data = (mcp_http_connection_data_t*)connection->private_data;
    if (!conn_data || conn_data->socket_fd < 0) return -1;

    // Send SSE headers
    const char *sse_headers =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/event-stream\r\n"
        "Cache-Control: no-cache\r\n"
        "Connection: keep-alive\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Headers: Content-Type, Authorization\r\n"
        "\r\n";

    if (send(conn_data->socket_fd, sse_headers, strlen(sse_headers), 0) < 0) {
        return -1;
    }

    // Send initial connection event
    const char *connect_event = "event: connect\ndata: {\"type\":\"connect\"}\n\n";
    if (send(conn_data->socket_fd, connect_event, strlen(connect_event), 0) < 0) {
        return -1;
    }

    // Keep connection alive for SSE
    conn_data->keep_alive = true;

    return 0;
}
