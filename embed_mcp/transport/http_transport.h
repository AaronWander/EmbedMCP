#ifndef MCP_HTTP_TRANSPORT_H
#define MCP_HTTP_TRANSPORT_H

#include "transport_interface.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

// HTTP transport specific structures
typedef struct {
    int server_socket;
    struct sockaddr_in server_addr;
    
    // Threading
    pthread_t server_thread;
    pthread_mutex_t connections_mutex;
    bool server_running;
    
    // Connection management
    mcp_connection_t **connections;
    size_t connection_count;
    size_t max_connections;
    
    // HTTP configuration
    char *bind_address;
    int port;
    char *endpoint_path;
    bool enable_cors;
    size_t max_request_size;
    
    // Request handling
    char *cors_headers;
    char *server_header;
} mcp_http_transport_data_t;

// HTTP connection data
typedef struct {
    int socket_fd;
    struct sockaddr_in client_addr;
    pthread_t handler_thread;
    bool thread_active;
    
    // HTTP state
    bool keep_alive;
    time_t last_request_time;
    size_t requests_handled;
    
    // Request parsing
    char *request_buffer;
    size_t request_buffer_size;
    size_t request_buffer_capacity;
} mcp_http_connection_data_t;

// HTTP request structure
typedef struct {
    char *method;
    char *path;
    char *protocol;
    char *headers;
    char *body;
    size_t body_length;
    
    // Parsed headers
    char *content_type;
    size_t content_length;
    char *connection;
    char *origin;
} mcp_http_request_t;

// HTTP response structure
typedef struct {
    int status_code;
    char *status_message;
    char *headers;
    char *body;
    size_t body_length;
    bool close_connection;
} mcp_http_response_t;

// HTTP transport interface implementation
extern const mcp_transport_interface_t mcp_http_transport_interface;

// HTTP-specific functions
int mcp_http_transport_init_impl(mcp_transport_t *transport, const mcp_transport_config_t *config);
int mcp_http_transport_start_impl(mcp_transport_t *transport);
int mcp_http_transport_stop_impl(mcp_transport_t *transport);
int mcp_http_transport_send_impl(mcp_connection_t *connection, const char *message, size_t length);
int mcp_http_transport_close_connection_impl(mcp_connection_t *connection);
int mcp_http_transport_get_stats_impl(mcp_transport_t *transport, void *stats);
void mcp_http_transport_cleanup_impl(mcp_transport_t *transport);

// HTTP server functions
void *mcp_http_server_thread(void *arg);
void *mcp_http_connection_handler_thread(void *arg);
int mcp_http_accept_connection(mcp_transport_t *transport);

// HTTP connection management
mcp_connection_t *mcp_http_connection_create(mcp_transport_t *transport, int socket_fd, 
                                           const struct sockaddr_in *client_addr);
void mcp_http_connection_destroy(mcp_connection_t *connection);
int mcp_http_add_connection(mcp_transport_t *transport, mcp_connection_t *connection);
int mcp_http_remove_connection(mcp_transport_t *transport, mcp_connection_t *connection);

// HTTP request/response handling
int mcp_http_parse_request(const char *raw_request, mcp_http_request_t *request);
void mcp_http_request_cleanup(mcp_http_request_t *request);
int mcp_http_send_response(mcp_connection_t *connection, const mcp_http_response_t *response);
void mcp_http_response_cleanup(mcp_http_response_t *response);

// HTTP utility functions
char *mcp_http_create_response_headers(int status_code, const char *content_type, 
                                      size_t content_length, bool enable_cors);
int mcp_http_send_error_response(mcp_connection_t *connection, int status_code, 
                                const char *message);
int mcp_http_send_json_response(mcp_connection_t *connection, int status_code, 
                               const char *json_data);

// HTTP method handlers
int mcp_http_handle_post(mcp_connection_t *connection, const mcp_http_request_t *request);
int mcp_http_handle_get(mcp_connection_t *connection, const mcp_http_request_t *request);
int mcp_http_handle_options(mcp_connection_t *connection, const mcp_http_request_t *request);
int mcp_http_handle_sse_request(mcp_connection_t *connection, const mcp_http_request_t *request);

// HTTP error handling
void mcp_http_handle_error(mcp_transport_t *transport, int error_code, const char *message);

// HTTP status codes
#define HTTP_STATUS_OK 200
#define HTTP_STATUS_BAD_REQUEST 400
#define HTTP_STATUS_NOT_FOUND 404
#define HTTP_STATUS_METHOD_NOT_ALLOWED 405
#define HTTP_STATUS_INTERNAL_SERVER_ERROR 500

// HTTP headers
#define HTTP_HEADER_CONTENT_TYPE "Content-Type"
#define HTTP_HEADER_CONTENT_LENGTH "Content-Length"
#define HTTP_HEADER_CONNECTION "Connection"
#define HTTP_HEADER_ORIGIN "Origin"
#define HTTP_HEADER_ACCESS_CONTROL_ALLOW_ORIGIN "Access-Control-Allow-Origin"
#define HTTP_HEADER_ACCESS_CONTROL_ALLOW_METHODS "Access-Control-Allow-Methods"
#define HTTP_HEADER_ACCESS_CONTROL_ALLOW_HEADERS "Access-Control-Allow-Headers"

#endif // MCP_HTTP_TRANSPORT_H
