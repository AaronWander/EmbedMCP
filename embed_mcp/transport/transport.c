#include "transport/transport_interface.h"
#include "transport/stdio_transport.h"
#include "transport/http_transport.h"
#include "utils/logging.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Transport factory functions
mcp_transport_t *mcp_transport_create(mcp_transport_type_t type) {
    mcp_transport_t *transport = calloc(1, sizeof(mcp_transport_t));
    if (!transport) return NULL;
    
    transport->type = type;
    transport->state = MCP_TRANSPORT_STATE_STOPPED;
    transport->config = NULL;
    transport->private_data = NULL;
    transport->messages_sent = 0;
    transport->messages_received = 0;
    transport->connections_opened = 0;
    transport->connections_closed = 0;
    transport->started_time = 0;
    
    // Set the appropriate interface
    switch (type) {
        case MCP_TRANSPORT_STDIO:
            transport->interface = &mcp_stdio_transport_interface;
            break;
        case MCP_TRANSPORT_HTTP:
            transport->interface = &mcp_http_transport_interface;
            break;
        default:
            free(transport);
            return NULL;
    }
    
    return transport;
}

mcp_transport_t *mcp_transport_create_stdio(void) {
    mcp_transport_t *transport = mcp_transport_create(MCP_TRANSPORT_STDIO);
    if (!transport) return NULL;
    
    mcp_transport_config_t *config = mcp_transport_config_create_stdio();
    if (!config) {
        mcp_transport_destroy(transport);
        return NULL;
    }
    
    if (mcp_transport_init(transport, config) != 0) {
        mcp_transport_config_destroy(config);
        mcp_transport_destroy(transport);
        return NULL;
    }
    
    mcp_transport_config_destroy(config);
    return transport;
}

mcp_transport_t *mcp_transport_create_http(int port, const char *bind_address) {
    mcp_transport_t *transport = mcp_transport_create(MCP_TRANSPORT_HTTP);
    if (!transport) return NULL;

    mcp_transport_config_t *config = mcp_transport_config_create_http(port, bind_address);
    if (!config) {
        mcp_transport_destroy(transport);
        return NULL;
    }

    if (mcp_transport_init(transport, config) != 0) {
        mcp_transport_config_destroy(config);
        mcp_transport_destroy(transport);
        return NULL;
    }

    mcp_transport_config_destroy(config);
    return transport;
}

mcp_transport_t *mcp_transport_create_http_with_path(int port, const char *bind_address, const char *endpoint_path) {
    mcp_transport_t *transport = mcp_transport_create(MCP_TRANSPORT_HTTP);
    if (!transport) return NULL;

    mcp_transport_config_t *config = mcp_transport_config_create_http(port, bind_address);
    if (!config) {
        mcp_transport_destroy(transport);
        return NULL;
    }

    free(config->config.http.endpoint_path);
    config->config.http.endpoint_path = endpoint_path ? strdup(endpoint_path) : strdup("/mcp");

    if (mcp_transport_init(transport, config) != 0) {
        mcp_transport_config_destroy(config);
        mcp_transport_destroy(transport);
        return NULL;
    }

    mcp_transport_config_destroy(config);
    return transport;
}



// Transport lifecycle
int mcp_transport_init(mcp_transport_t *transport, const mcp_transport_config_t *config) {
    if (!transport || !transport->interface || !transport->interface->init) return -1;
    
    transport->state = MCP_TRANSPORT_STATE_STARTING;
    
    int result = transport->interface->init(transport, config);
    if (result == 0) {
        transport->state = MCP_TRANSPORT_STATE_STOPPED;
    } else {
        transport->state = MCP_TRANSPORT_STATE_ERROR;
    }
    
    return result;
}

int mcp_transport_start(mcp_transport_t *transport) {
    if (!transport || !transport->interface || !transport->interface->start) return -1;
    
    if (transport->state != MCP_TRANSPORT_STATE_STOPPED) return -1;
    
    transport->state = MCP_TRANSPORT_STATE_STARTING;
    
    int result = transport->interface->start(transport);
    if (result == 0) {
        transport->state = MCP_TRANSPORT_STATE_RUNNING;
        transport->started_time = time(NULL);
    } else {
        transport->state = MCP_TRANSPORT_STATE_ERROR;
    }
    
    return result;
}

int mcp_transport_stop(mcp_transport_t *transport) {
    if (!transport || !transport->interface || !transport->interface->stop) return -1;
    
    if (transport->state != MCP_TRANSPORT_STATE_RUNNING) return -1;
    
    transport->state = MCP_TRANSPORT_STATE_STOPPING;
    
    int result = transport->interface->stop(transport);
    if (result == 0) {
        transport->state = MCP_TRANSPORT_STATE_STOPPED;
    } else {
        transport->state = MCP_TRANSPORT_STATE_ERROR;
    }
    
    return result;
}

void mcp_transport_destroy(mcp_transport_t *transport) {
    if (!transport) return;
    
    // Stop the transport if it's running
    if (transport->state == MCP_TRANSPORT_STATE_RUNNING) {
        mcp_transport_stop(transport);
    }
    
    // Cleanup implementation-specific data
    if (transport->interface && transport->interface->cleanup) {
        transport->interface->cleanup(transport);
    }
    
    // Cleanup configuration
    mcp_transport_config_destroy(transport->config);
    
    free(transport);
}

// Transport management
void mcp_transport_set_callbacks(mcp_transport_t *transport,
                                mcp_message_received_callback_t on_message,
                                mcp_connection_opened_callback_t on_opened,
                                mcp_connection_closed_callback_t on_closed,
                                mcp_transport_error_callback_t on_error,
                                void *user_data) {
    if (!transport) return;
    
    transport->on_message = on_message;
    transport->on_connection_opened = on_opened;
    transport->on_connection_closed = on_closed;
    transport->on_error = on_error;
    transport->user_data = user_data;
}

// Connection management
int mcp_connection_send(mcp_connection_t *connection, const char *message, size_t length) {
    if (!connection || !connection->transport || !connection->transport->interface) return -1;
    
    if (!connection->transport->interface->send) return -1;
    
    int result = connection->transport->interface->send(connection, message, length);
    if (result == 0) {
        connection->messages_sent++;
        connection->bytes_sent += length;
        connection->last_activity = time(NULL);
        connection->transport->messages_sent++;
    }
    
    return result;
}

int mcp_connection_close(mcp_connection_t *connection) {
    if (!connection || !connection->transport || !connection->transport->interface) return -1;
    
    if (!connection->transport->interface->close_connection) return -1;
    
    int result = connection->transport->interface->close_connection(connection);
    if (result == 0) {
        connection->is_active = false;
        connection->transport->connections_closed++;
    }
    
    return result;
}

bool mcp_connection_is_active(const mcp_connection_t *connection) {
    return connection && connection->is_active;
}

const char *mcp_connection_get_id(const mcp_connection_t *connection) {
    return connection ? connection->connection_id : NULL;
}

const char *mcp_connection_get_session_id(const mcp_connection_t *connection) {
    return connection ? connection->session_id : NULL;
}

int mcp_connection_set_session_id(mcp_connection_t *connection, const char *session_id) {
    if (!connection) return -1;
    
    free(connection->session_id);
    connection->session_id = session_id ? strdup(session_id) : NULL;
    
    return 0;
}

// Configuration helpers
mcp_transport_config_t *mcp_transport_config_create_default(mcp_transport_type_t type) {
    switch (type) {
        case MCP_TRANSPORT_STDIO:
            return mcp_transport_config_create_stdio();
        case MCP_TRANSPORT_HTTP:
            return mcp_transport_config_create_http(8080, "0.0.0.0");
        default:
            return NULL;
    }
}

mcp_transport_config_t *mcp_transport_config_create_stdio(void) {
    mcp_transport_config_t *config = calloc(1, sizeof(mcp_transport_config_t));
    if (!config) return NULL;
    
    config->type = MCP_TRANSPORT_STDIO;
    config->enable_logging = false;
    config->max_message_size = 1024 * 1024; // 1MB
    config->max_connections = 1;
    config->connection_timeout = 0; // No timeout for stdio
    
    return config;
}

mcp_transport_config_t *mcp_transport_config_create_http(int port, const char *bind_address) {
    mcp_transport_config_t *config = calloc(1, sizeof(mcp_transport_config_t));
    if (!config) return NULL;
    
    config->type = MCP_TRANSPORT_HTTP;
    config->enable_logging = true;
    config->max_message_size = 1024 * 1024; // 1MB
    config->max_connections = 100;
    config->connection_timeout = 30; // 30 seconds
    
    config->config.http.port = port;
    config->config.http.bind_address = bind_address ? strdup(bind_address) : strdup("0.0.0.0");
    config->config.http.endpoint_path = strdup("/mcp");
    config->config.http.enable_cors = true;
    config->config.http.max_request_size = 1024 * 1024; // 1MB
    
    return config;
}



void mcp_transport_config_destroy(mcp_transport_config_t *config) {
    if (!config) return;
    
    switch (config->type) {
        case MCP_TRANSPORT_HTTP:
            free(config->config.http.bind_address);
            free(config->config.http.endpoint_path);
            break;
        default:
            break;
    }
    
    free(config);
}

// Utility functions
const char *mcp_transport_type_to_string(mcp_transport_type_t type) {
    switch (type) {
        case MCP_TRANSPORT_STDIO: return "STDIO";
        case MCP_TRANSPORT_HTTP: return "HTTP";
        default: return "UNKNOWN";
    }
}

const char *mcp_transport_state_to_string(mcp_transport_state_t state) {
    switch (state) {
        case MCP_TRANSPORT_STATE_STOPPED: return "STOPPED";
        case MCP_TRANSPORT_STATE_STARTING: return "STARTING";
        case MCP_TRANSPORT_STATE_RUNNING: return "RUNNING";
        case MCP_TRANSPORT_STATE_STOPPING: return "STOPPING";
        case MCP_TRANSPORT_STATE_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

mcp_transport_state_t mcp_transport_get_state(const mcp_transport_t *transport) {
    return transport ? transport->state : MCP_TRANSPORT_STATE_ERROR;
}
