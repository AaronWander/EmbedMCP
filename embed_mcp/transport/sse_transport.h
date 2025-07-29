#ifndef MCP_SSE_TRANSPORT_H
#define MCP_SSE_TRANSPORT_H

#include "transport_interface.h"
#include "http_transport.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

// SSE transport specific structures
typedef struct {
    // Inherits from HTTP transport
    mcp_http_transport_data_t http_base;
    
    // SSE-specific configuration
    time_t keepalive_interval;
    char *event_stream_path;
    
    // SSE connection management
    mcp_connection_t **sse_connections;
    size_t sse_connection_count;
    pthread_mutex_t sse_connections_mutex;
    
    // Keepalive management
    pthread_t keepalive_thread;
    bool keepalive_running;
} mcp_sse_transport_data_t;

// SSE connection data
typedef struct {
    // Inherits from HTTP connection
    mcp_http_connection_data_t http_base;
    
    // SSE-specific state
    bool is_sse_connection;
    time_t last_keepalive;
    size_t events_sent;
    
    // Event buffering
    char *event_buffer;
    size_t event_buffer_size;
    size_t event_buffer_capacity;
} mcp_sse_connection_data_t;

// SSE event structure
typedef struct {
    char *event_type;
    char *data;
    char *id;
    int retry;
} mcp_sse_event_t;

// SSE transport interface implementation
extern const mcp_transport_interface_t mcp_sse_transport_interface;

// SSE-specific functions
int mcp_sse_transport_init_impl(mcp_transport_t *transport, const mcp_transport_config_t *config);
int mcp_sse_transport_start_impl(mcp_transport_t *transport);
int mcp_sse_transport_stop_impl(mcp_transport_t *transport);
int mcp_sse_transport_send_impl(mcp_connection_t *connection, const char *message, size_t length);
int mcp_sse_transport_close_connection_impl(mcp_connection_t *connection);
int mcp_sse_transport_get_stats_impl(mcp_transport_t *transport, void *stats);
void mcp_sse_transport_cleanup_impl(mcp_transport_t *transport);

// SSE server functions
void *mcp_sse_keepalive_thread(void *arg);
int mcp_sse_handle_connection_request(mcp_connection_t *connection, const mcp_http_request_t *request);

// SSE connection management
mcp_connection_t *mcp_sse_connection_create(mcp_transport_t *transport, int socket_fd, 
                                          const struct sockaddr_in *client_addr);
void mcp_sse_connection_destroy(mcp_connection_t *connection);
int mcp_sse_add_connection(mcp_transport_t *transport, mcp_connection_t *connection);
int mcp_sse_remove_connection(mcp_transport_t *transport, mcp_connection_t *connection);
int mcp_sse_upgrade_connection(mcp_connection_t *connection);

// SSE event handling
int mcp_sse_send_event(mcp_connection_t *connection, const mcp_sse_event_t *event);
int mcp_sse_send_data(mcp_connection_t *connection, const char *data);
int mcp_sse_send_keepalive(mcp_connection_t *connection);
int mcp_sse_broadcast_event(mcp_transport_t *transport, const mcp_sse_event_t *event);

// SSE event utilities
mcp_sse_event_t *mcp_sse_event_create(const char *event_type, const char *data, 
                                     const char *id, int retry);
void mcp_sse_event_destroy(mcp_sse_event_t *event);
char *mcp_sse_event_serialize(const mcp_sse_event_t *event);

// SSE response utilities
int mcp_sse_send_connection_response(mcp_connection_t *connection);
int mcp_sse_send_error_event(mcp_connection_t *connection, int error_code, const char *message);

// SSE utility functions
bool mcp_sse_is_sse_request(const mcp_http_request_t *request);
char *mcp_sse_create_sse_headers(void);
int mcp_sse_validate_connection(mcp_connection_t *connection);

// SSE error handling
void mcp_sse_handle_error(mcp_transport_t *transport, int error_code, const char *message);

// SSE constants
#define SSE_DEFAULT_PATH "/events"
#define SSE_DEFAULT_KEEPALIVE_INTERVAL 30
#define SSE_CONTENT_TYPE "text/event-stream"
#define SSE_CACHE_CONTROL "no-cache"
#define SSE_CONNECTION "keep-alive"

// SSE event types
#define SSE_EVENT_MESSAGE "message"
#define SSE_EVENT_ERROR "error"
#define SSE_EVENT_KEEPALIVE "keepalive"

#endif // MCP_SSE_TRANSPORT_H
