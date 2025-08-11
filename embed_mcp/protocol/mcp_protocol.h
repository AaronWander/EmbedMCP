#ifndef MCP_PROTOCOL_H
#define MCP_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include "cjson/cJSON.h"
#include "message.h"
#include "jsonrpc.h"
#include "protocol_state.h"

// Forward declarations
typedef struct mcp_protocol mcp_protocol_t;

// MCP Method Names
#define MCP_METHOD_INITIALIZE "initialize"
#define MCP_METHOD_INITIALIZED "notifications/initialized"
#define MCP_METHOD_PING "ping"
#define MCP_METHOD_LIST_TOOLS "tools/list"
#define MCP_METHOD_CALL_TOOL "tools/call"
#define MCP_METHOD_LIST_RESOURCES "resources/list"
#define MCP_METHOD_READ_RESOURCE "resources/read"
#define MCP_METHOD_LIST_PROMPTS "prompts/list"
#define MCP_METHOD_GET_PROMPT "prompts/get"
#define MCP_METHOD_SET_LEVEL "logging/setLevel"

// Protocol callback functions
typedef int (*mcp_send_callback_t)(const char *data, size_t length, void *user_data);
typedef void (*mcp_error_callback_t)(int code, const char *message, void *user_data);
typedef void (*mcp_state_change_callback_t)(mcp_protocol_state_t old_state, 
                                           mcp_protocol_state_t new_state, void *user_data);

// Request handler callback
typedef cJSON *(*mcp_request_handler_t)(const mcp_request_t *request, void *user_data);

// Protocol configuration
typedef struct {
    bool strict_mode;           // Enforce strict protocol compliance
    bool enable_logging;        // Enable protocol-level logging
    size_t max_message_size;    // Maximum message size
    size_t max_pending_requests; // Maximum pending requests
    time_t request_timeout;     // Request timeout in seconds
    
    // Server information
    char *server_name;
    char *server_version;
    char *instructions;         // Server usage instructions (optional)

    // Supported capabilities
    mcp_capabilities_t *capabilities;
} mcp_protocol_config_t;

// Protocol interface
struct mcp_protocol {
    // Configuration
    mcp_protocol_config_t *config;
    
    // State management
    mcp_protocol_state_machine_t *state_machine;
    
    // JSON-RPC parser
    jsonrpc_parser_t *parser;
    
    // Callbacks
    mcp_send_callback_t send_callback;
    mcp_error_callback_t error_callback;
    mcp_state_change_callback_t state_change_callback;
    mcp_request_handler_t request_handler;
    void *user_data;
    
    // Internal state
    bool initialized;
    size_t pending_requests;
    time_t last_activity;
};

// Protocol lifecycle
mcp_protocol_t *mcp_protocol_create(const mcp_protocol_config_t *config);
void mcp_protocol_destroy(mcp_protocol_t *protocol);

// Configuration management
mcp_protocol_config_t *mcp_protocol_config_create_default(void);
void mcp_protocol_config_destroy(mcp_protocol_config_t *config);
int mcp_protocol_config_set_server_info(mcp_protocol_config_t *config,
                                       const char *name, const char *version);
int mcp_protocol_config_set_instructions(mcp_protocol_config_t *config,
                                        const char *instructions);

// Callback registration
void mcp_protocol_set_send_callback(mcp_protocol_t *protocol, 
                                   mcp_send_callback_t callback, void *user_data);
void mcp_protocol_set_error_callback(mcp_protocol_t *protocol, 
                                    mcp_error_callback_t callback, void *user_data);
void mcp_protocol_set_state_change_callback(mcp_protocol_t *protocol,
                                           mcp_state_change_callback_t callback, void *user_data);
void mcp_protocol_set_request_handler(mcp_protocol_t *protocol,
                                     mcp_request_handler_t handler, void *user_data);

// Message handling
int mcp_protocol_handle_message(mcp_protocol_t *protocol, const char *json_data);
int mcp_protocol_handle_request(mcp_protocol_t *protocol, const mcp_request_t *request);
int mcp_protocol_handle_response(mcp_protocol_t *protocol, const mcp_response_t *response);
int mcp_protocol_handle_notification(mcp_protocol_t *protocol, const mcp_request_t *notification);

// Message sending
int mcp_protocol_send_response(mcp_protocol_t *protocol, cJSON *id, cJSON *result);
int mcp_protocol_send_error_response(mcp_protocol_t *protocol, cJSON *id, 
                                    int code, const char *message, cJSON *data);
int mcp_protocol_send_notification(mcp_protocol_t *protocol, const char *method, cJSON *params);
int mcp_protocol_send_request(mcp_protocol_t *protocol, cJSON *id, 
                             const char *method, cJSON *params);

// Built-in method handlers
cJSON *mcp_protocol_handle_initialize(mcp_protocol_t *protocol, const mcp_request_t *request);
int mcp_protocol_handle_initialized(mcp_protocol_t *protocol, const mcp_request_t *notification);
cJSON *mcp_protocol_handle_ping(mcp_protocol_t *protocol, const mcp_request_t *request);

// Session info
const mcp_session_info_t *mcp_protocol_get_session_info(const mcp_protocol_t *protocol);

// Utility functions
bool mcp_protocol_is_builtin_method(const char *method);
const char *mcp_protocol_get_version(void);
cJSON *mcp_protocol_create_server_info(const mcp_protocol_t *protocol);
cJSON *mcp_protocol_create_capabilities_json(const mcp_protocol_t *protocol);

// Error helpers
int mcp_protocol_send_parse_error(mcp_protocol_t *protocol, cJSON *id);
int mcp_protocol_send_invalid_request_error(mcp_protocol_t *protocol, cJSON *id);
int mcp_protocol_send_method_not_found_error(mcp_protocol_t *protocol, cJSON *id, const char *method);
int mcp_protocol_send_invalid_params_error(mcp_protocol_t *protocol, cJSON *id, const char *details);
int mcp_protocol_send_internal_error(mcp_protocol_t *protocol, cJSON *id, const char *details);

#endif // MCP_PROTOCOL_H
