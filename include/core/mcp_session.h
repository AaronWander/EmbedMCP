#ifndef MCP_SESSION_H
#define MCP_SESSION_H

#include <stdbool.h>
#include <time.h>
#include "transport/transport_interface.h"

// Session states
typedef enum {
    MCP_SESSION_CREATED,      // Session created, not initialized
    MCP_SESSION_INITIALIZING, // Initialize request received
    MCP_SESSION_INITIALIZED,  // Initialized notification received
    MCP_SESSION_CLOSED        // Session closed
} mcp_session_state_t;

// Session structure
typedef struct mcp_session {
    char *session_id;
    mcp_session_state_t state;
    mcp_connection_t *connection;
    time_t created_at;
    time_t last_activity;
    
    // Client information
    char *client_name;
    char *client_version;
    char *protocol_version;
    
    struct mcp_session *next; // For linked list
} mcp_session_t;

// Session manager
typedef struct {
    mcp_session_t *sessions;
    int session_count;
    int max_sessions;
} mcp_session_manager_t;

// Session management functions
mcp_session_manager_t *mcp_session_manager_create(int max_sessions);
void mcp_session_manager_destroy(mcp_session_manager_t *manager);

// Session operations
mcp_session_t *mcp_session_create(mcp_session_manager_t *manager, 
                                 mcp_connection_t *connection);
mcp_session_t *mcp_session_find(mcp_session_manager_t *manager, 
                               const char *session_id);
mcp_session_t *mcp_session_find_by_connection(mcp_session_manager_t *manager,
                                             mcp_connection_t *connection);

int mcp_session_initialize(mcp_session_t *session, 
                          const char *client_name,
                          const char *client_version,
                          const char *protocol_version);
int mcp_session_mark_initialized(mcp_session_t *session);
int mcp_session_close(mcp_session_manager_t *manager, mcp_session_t *session);

// Session utilities
bool mcp_session_is_valid(const mcp_session_t *session);
bool mcp_session_is_initialized(const mcp_session_t *session);
void mcp_session_update_activity(mcp_session_t *session);

// Session cleanup
int mcp_session_cleanup_expired(mcp_session_manager_t *manager, int timeout_seconds);

#endif // MCP_SESSION_H
