#ifndef MCP_PROTOCOL_H
#define MCP_PROTOCOL_H

#include <stdbool.h>
#include <cjson/cJSON.h>

// MCP Protocol Version
#define MCP_PROTOCOL_VERSION "2025-06-18"

// JSON-RPC Error Codes
#define JSONRPC_PARSE_ERROR     -32700
#define JSONRPC_INVALID_REQUEST -32600
#define JSONRPC_METHOD_NOT_FOUND -32601
#define JSONRPC_INVALID_PARAMS  -32602
#define JSONRPC_INTERNAL_ERROR  -32603

// MCP Message Types
typedef enum {
    MCP_MESSAGE_REQUEST,      // Has id, expects response
    MCP_MESSAGE_NOTIFICATION, // No id, no response expected
    MCP_MESSAGE_RESPONSE      // Response to a request
} mcp_message_type_t;

// MCP Message Structure
typedef struct {
    mcp_message_type_t type;
    char *jsonrpc;           // Always "2.0"
    cJSON *id;               // Request ID (NULL for notifications)
    char *method;            // Method name (NULL for responses)
    cJSON *params;           // Parameters (optional)
    cJSON *result;           // Result (responses only)
    cJSON *error;            // Error (error responses only)
} mcp_message_t;

// MCP Protocol Interface
typedef struct mcp_protocol mcp_protocol_t;

// Protocol callback functions
typedef int (*mcp_send_callback_t)(const char *data, size_t length, void *user_data);
typedef void (*mcp_error_callback_t)(int code, const char *message, void *user_data);

// Protocol functions
mcp_protocol_t *mcp_protocol_create(mcp_send_callback_t send_cb, 
                                   mcp_error_callback_t error_cb,
                                   void *user_data);
void mcp_protocol_destroy(mcp_protocol_t *protocol);

// Message handling
int mcp_protocol_handle_message(mcp_protocol_t *protocol, const char *json_data);
int mcp_protocol_send_response(mcp_protocol_t *protocol, cJSON *id, cJSON *result);
int mcp_protocol_send_error(mcp_protocol_t *protocol, cJSON *id, int code, const char *message);
int mcp_protocol_send_notification(mcp_protocol_t *protocol, const char *method, cJSON *params);

// Message utilities
mcp_message_t *mcp_message_create_request(cJSON *id, const char *method, cJSON *params);
mcp_message_t *mcp_message_create_notification(const char *method, cJSON *params);
mcp_message_t *mcp_message_create_response(cJSON *id, cJSON *result);
mcp_message_t *mcp_message_create_error(cJSON *id, int code, const char *message);
void mcp_message_destroy(mcp_message_t *message);

// Message parsing
mcp_message_t *mcp_message_parse(const char *json_data);
char *mcp_message_serialize(const mcp_message_t *message);

#endif // MCP_PROTOCOL_H
