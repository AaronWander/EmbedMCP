#ifndef MCP_PROTOCOL_STATE_H
#define MCP_PROTOCOL_STATE_H

#include <stdbool.h>
#include <stddef.h>
#include <time.h>
#include "cjson/cJSON.h"

// MCP Protocol States
typedef enum {
    MCP_STATE_UNINITIALIZED,    // Initial state, waiting for initialize
    MCP_STATE_INITIALIZING,     // Initialize received, processing
    MCP_STATE_INITIALIZED,      // Initialize complete, waiting for initialized notification
    MCP_STATE_READY,           // Ready to handle requests
    MCP_STATE_ERROR,           // Error state
    MCP_STATE_SHUTDOWN         // Shutting down
} mcp_protocol_state_t;

// MCP Protocol Events
typedef enum {
    MCP_EVENT_INITIALIZE_REQUEST,     // Client sends initialize request
    MCP_EVENT_INITIALIZE_RESPONSE,    // Server sends initialize response
    MCP_EVENT_INITIALIZED_NOTIFICATION, // Client sends initialized notification
    MCP_EVENT_REQUEST,               // Regular request received
    MCP_EVENT_RESPONSE,              // Response received
    MCP_EVENT_NOTIFICATION,          // Notification received
    MCP_EVENT_ERROR,                 // Error occurred
    MCP_EVENT_SHUTDOWN              // Shutdown requested
} mcp_protocol_event_t;

// Protocol capabilities
typedef struct {
    // Server capabilities
    struct {
        bool tools;              // Supports tools
        bool resources;          // Supports resources
        bool prompts;           // Supports prompts
        bool logging;           // Supports logging
    } server;
    
    // Client capabilities
    struct {
        bool roots;             // Supports roots
        bool sampling;          // Supports sampling
    } client;
} mcp_capabilities_t;

// Protocol session information
typedef struct {
    char *protocol_version;     // Negotiated protocol version
    mcp_capabilities_t capabilities; // Negotiated capabilities
    
    // Client information
    struct {
        char *name;
        char *version;
    } client_info;
    
    // Server information
    struct {
        char *name;
        char *version;
    } server_info;
    
    time_t initialized_time;    // When the session was initialized
    time_t last_activity;       // Last activity timestamp
} mcp_session_info_t;

// Protocol state machine
typedef struct {
    mcp_protocol_state_t current_state;
    mcp_protocol_state_t previous_state;
    mcp_session_info_t session_info;
    
    // State transition tracking
    time_t state_entered_time;
    size_t transition_count;
    
    // Error information
    int last_error_code;
    char *last_error_message;
    
    // Configuration
    bool strict_mode;           // Enforce strict protocol compliance
    size_t max_pending_requests; // Maximum pending requests
    time_t request_timeout;     // Request timeout in seconds
} mcp_protocol_state_machine_t;

// State machine creation and destruction
mcp_protocol_state_machine_t *mcp_protocol_state_create(void);
void mcp_protocol_state_destroy(mcp_protocol_state_machine_t *state_machine);

// State transitions
bool mcp_protocol_state_transition(mcp_protocol_state_machine_t *state_machine, 
                                  mcp_protocol_event_t event);
bool mcp_protocol_state_can_transition(const mcp_protocol_state_machine_t *state_machine,
                                      mcp_protocol_event_t event);

// State queries
mcp_protocol_state_t mcp_protocol_state_get_current(const mcp_protocol_state_machine_t *state_machine);
mcp_protocol_state_t mcp_protocol_state_get_previous(const mcp_protocol_state_machine_t *state_machine);
bool mcp_protocol_state_is_ready(const mcp_protocol_state_machine_t *state_machine);
bool mcp_protocol_state_is_initialized(const mcp_protocol_state_machine_t *state_machine);
bool mcp_protocol_state_can_handle_requests(const mcp_protocol_state_machine_t *state_machine);

// Session management
int mcp_protocol_state_initialize_session(mcp_protocol_state_machine_t *state_machine,
                                         const char *protocol_version,
                                         const cJSON *client_capabilities,
                                         const cJSON *client_info);
int mcp_protocol_state_finalize_initialization(mcp_protocol_state_machine_t *state_machine);
void mcp_protocol_state_reset_session(mcp_protocol_state_machine_t *state_machine);

// Capabilities management
mcp_capabilities_t *mcp_capabilities_create_default(void);
void mcp_capabilities_destroy(mcp_capabilities_t *capabilities);
bool mcp_capabilities_merge(mcp_capabilities_t *target, const mcp_capabilities_t *source);
cJSON *mcp_capabilities_to_json(const mcp_capabilities_t *capabilities);
mcp_capabilities_t *mcp_capabilities_from_json(const cJSON *json);

// Session info management
void mcp_session_info_init(mcp_session_info_t *info);
void mcp_session_info_cleanup(mcp_session_info_t *info);
cJSON *mcp_session_info_to_json(const mcp_session_info_t *info);

// Error handling
void mcp_protocol_state_set_error(mcp_protocol_state_machine_t *state_machine,
                                 int error_code, const char *error_message);
void mcp_protocol_state_clear_error(mcp_protocol_state_machine_t *state_machine);
bool mcp_protocol_state_has_error(const mcp_protocol_state_machine_t *state_machine);

// Utility functions
const char *mcp_protocol_state_to_string(mcp_protocol_state_t state);
const char *mcp_protocol_event_to_string(mcp_protocol_event_t event);
bool mcp_protocol_version_is_supported(const char *version);

#endif // MCP_PROTOCOL_STATE_H
