#ifndef TRANSPORT_INTERFACE_H
#define TRANSPORT_INTERFACE_H

#include <stddef.h>
#include <stdbool.h>

// Transport types
typedef enum {
    MCP_TRANSPORT_STDIO,
    MCP_TRANSPORT_HTTP,
    MCP_TRANSPORT_SSE
} mcp_transport_type_t;

// Forward declarations
typedef struct mcp_transport mcp_transport_t;
typedef struct mcp_connection mcp_connection_t;

// Transport callback functions
typedef void (*mcp_message_received_callback_t)(const char *message, size_t length, 
                                               mcp_connection_t *connection, void *user_data);
typedef void (*mcp_connection_opened_callback_t)(mcp_connection_t *connection, void *user_data);
typedef void (*mcp_connection_closed_callback_t)(mcp_connection_t *connection, void *user_data);

// Transport interface
typedef struct {
    // Initialize the transport
    int (*init)(mcp_transport_t *transport, void *config);
    
    // Start the transport (begin listening/accepting connections)
    int (*start)(mcp_transport_t *transport);
    
    // Stop the transport
    int (*stop)(mcp_transport_t *transport);
    
    // Send message to a specific connection
    int (*send)(mcp_connection_t *connection, const char *message, size_t length);
    
    // Close a connection
    int (*close_connection)(mcp_connection_t *connection);
    
    // Cleanup
    void (*cleanup)(mcp_transport_t *transport);
} mcp_transport_interface_t;

// Transport structure
struct mcp_transport {
    mcp_transport_type_t type;
    const mcp_transport_interface_t *interface;
    void *private_data;
    
    // Callbacks
    mcp_message_received_callback_t on_message;
    mcp_connection_opened_callback_t on_connection_opened;
    mcp_connection_closed_callback_t on_connection_closed;
    void *user_data;
};

// Connection structure
struct mcp_connection {
    mcp_transport_t *transport;
    char *session_id;
    bool is_initialized;
    void *private_data;
};

// Transport factory functions
mcp_transport_t *mcp_transport_create_stdio(void);
mcp_transport_t *mcp_transport_create_http(int port);
mcp_transport_t *mcp_transport_create_sse(int port);

// Transport management
void mcp_transport_set_callbacks(mcp_transport_t *transport,
                                mcp_message_received_callback_t on_message,
                                mcp_connection_opened_callback_t on_opened,
                                mcp_connection_closed_callback_t on_closed,
                                void *user_data);

void mcp_transport_destroy(mcp_transport_t *transport);

#endif // TRANSPORT_INTERFACE_H
