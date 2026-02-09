#ifndef MCP_TRANSPORT_H
#define MCP_TRANSPORT_H

#include <stddef.h>
#include <stdbool.h>
#include <time.h>

// Transport types
typedef enum {
    MCP_TRANSPORT_STDIO,
    MCP_TRANSPORT_HTTP
} mcp_transport_type_t;

// Transport states
typedef enum {
    MCP_TRANSPORT_STATE_STOPPED,
    MCP_TRANSPORT_STATE_STARTING,
    MCP_TRANSPORT_STATE_RUNNING,
    MCP_TRANSPORT_STATE_STOPPING,
    MCP_TRANSPORT_STATE_ERROR
} mcp_transport_state_t;

// Forward declarations
typedef struct mcp_transport mcp_transport_t;
typedef struct mcp_connection mcp_connection_t;

// Transport callback functions
typedef void (*mcp_message_received_callback_t)(const char *message, size_t length,
                                               mcp_connection_t *connection, void *user_data);
typedef void (*mcp_connection_opened_callback_t)(mcp_connection_t *connection, void *user_data);
typedef void (*mcp_connection_closed_callback_t)(mcp_connection_t *connection, void *user_data);
typedef void (*mcp_transport_error_callback_t)(mcp_transport_t *transport, int error_code,
                                              const char *error_message, void *user_data);

// Transport configuration
typedef struct {
    mcp_transport_type_t type;

    // Common settings
    bool enable_logging;
    size_t max_message_size;
    size_t max_connections;
    time_t connection_timeout;

    // Type-specific settings
    union {
        struct {
            // STDIO has no specific configuration
            int dummy;
        } stdio;

        struct {
            int port;
            char *bind_address;
            char *endpoint_path;   // MCP endpoint path (default: "/mcp")
            bool enable_cors;
            size_t max_request_size;
        } http;
    } config;
} mcp_transport_config_t;

// Transport interface (virtual function table)
typedef struct {
    // Initialize the transport with configuration
    int (*init)(mcp_transport_t *transport, const mcp_transport_config_t *config);

    // Start the transport (begin listening/accepting connections)
    int (*start)(mcp_transport_t *transport);

    // Stop the transport gracefully
    int (*stop)(mcp_transport_t *transport);

    // Send message to a specific connection
    int (*send)(mcp_connection_t *connection, const char *message, size_t length);

    // Close a connection
    int (*close_connection)(mcp_connection_t *connection);

    // Get transport statistics
    int (*get_stats)(mcp_transport_t *transport, void *stats);

    // Cleanup resources
    void (*cleanup)(mcp_transport_t *transport);
} mcp_transport_interface_t;

// Transport structure
struct mcp_transport {
    mcp_transport_type_t type;
    mcp_transport_state_t state;
    const mcp_transport_interface_t *interface;
    void *private_data;

    // Configuration
    mcp_transport_config_t *config;

    // Callbacks
    mcp_message_received_callback_t on_message;
    mcp_connection_opened_callback_t on_connection_opened;
    mcp_connection_closed_callback_t on_connection_closed;
    mcp_transport_error_callback_t on_error;
    void *user_data;

    // Statistics
    size_t messages_sent;
    size_t messages_received;
    size_t connections_opened;
    size_t connections_closed;
    time_t started_time;
};

// Connection structure
struct mcp_connection {
    mcp_transport_t *transport;
    char *connection_id;
    char *session_id;
    bool is_active;
    time_t created_time;
    time_t last_activity;
    void *private_data;

    // Statistics
    size_t messages_sent;
    size_t messages_received;
    size_t bytes_sent;
    size_t bytes_received;
};

// Transport factory functions
mcp_transport_t *mcp_transport_create(mcp_transport_type_t type);
mcp_transport_t *mcp_transport_create_stdio(void);
mcp_transport_t *mcp_transport_create_http(int port, const char *bind_address);
mcp_transport_t *mcp_transport_create_http_with_path(int port, const char *bind_address, const char *endpoint_path);

// Transport lifecycle
int mcp_transport_init(mcp_transport_t *transport, const mcp_transport_config_t *config);
int mcp_transport_start(mcp_transport_t *transport);
int mcp_transport_stop(mcp_transport_t *transport);
void mcp_transport_destroy(mcp_transport_t *transport);

// Transport management
void mcp_transport_set_callbacks(mcp_transport_t *transport,
                                mcp_message_received_callback_t on_message,
                                mcp_connection_opened_callback_t on_opened,
                                mcp_connection_closed_callback_t on_closed,
                                mcp_transport_error_callback_t on_error,
                                void *user_data);

// Connection management
int mcp_connection_send(mcp_connection_t *connection, const char *message, size_t length);
int mcp_connection_close(mcp_connection_t *connection);
bool mcp_connection_is_active(const mcp_connection_t *connection);
const char *mcp_connection_get_id(const mcp_connection_t *connection);
const char *mcp_connection_get_session_id(const mcp_connection_t *connection);
int mcp_connection_set_session_id(mcp_connection_t *connection, const char *session_id);

// Configuration helpers
mcp_transport_config_t *mcp_transport_config_create_default(mcp_transport_type_t type);
mcp_transport_config_t *mcp_transport_config_create_stdio(void);
mcp_transport_config_t *mcp_transport_config_create_http(int port, const char *bind_address);
void mcp_transport_config_destroy(mcp_transport_config_t *config);

// Utility functions
const char *mcp_transport_type_to_string(mcp_transport_type_t type);
const char *mcp_transport_state_to_string(mcp_transport_state_t state);
mcp_transport_state_t mcp_transport_get_state(const mcp_transport_t *transport);

#endif // MCP_TRANSPORT_H
