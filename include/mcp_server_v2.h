#ifndef MCP_SERVER_V2_H
#define MCP_SERVER_V2_H

#include <stdbool.h>
#include <stddef.h>

// Core includes
#include "core/mcp_protocol.h"
#include "core/mcp_session.h"
#include "transport/transport_interface.h"

// Server configuration
typedef struct {
    int max_sessions;
    int session_timeout;
    bool debug_mode;
} mcp_server_config_t;

// Server structure
typedef struct {
    mcp_server_config_t config;
    mcp_session_manager_t *session_manager;
    mcp_protocol_t *protocol;
    mcp_transport_t *transport;
    bool running;
} mcp_server_v2_t;

// Server lifecycle
mcp_server_v2_t *mcp_server_create(const mcp_server_config_t *config);
void mcp_server_destroy(mcp_server_v2_t *server);

// Server operations
int mcp_server_set_transport(mcp_server_v2_t *server, mcp_transport_t *transport);
int mcp_server_start(mcp_server_v2_t *server);
int mcp_server_stop(mcp_server_v2_t *server);
int mcp_server_run(mcp_server_v2_t *server); // Blocking run

// Default configuration
mcp_server_config_t mcp_server_default_config(void);

// Utility functions
void mcp_debug_print(const char *format, ...);

#endif // MCP_SERVER_V2_H
