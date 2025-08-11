#ifndef MCP_MESSAGE_H
#define MCP_MESSAGE_H

#include <stdbool.h>
#include <stddef.h>
#include "cjson/cJSON.h"

// MCP Protocol Version
#define MCP_PROTOCOL_VERSION "2025-03-26"

// JSON-RPC Error Codes
#define JSONRPC_PARSE_ERROR     -32700
#define JSONRPC_INVALID_REQUEST -32600
#define JSONRPC_METHOD_NOT_FOUND -32601
#define JSONRPC_INVALID_PARAMS  -32602
#define JSONRPC_INTERNAL_ERROR  -32603

// MCP-specific error codes
#define MCP_ERROR_INVALID_PARAMS -32602
#define MCP_ERROR_METHOD_NOT_FOUND -32601
#define MCP_ERROR_INTERNAL_ERROR -32603

// MCP Message Types
typedef enum {
    MCP_MESSAGE_REQUEST,      // Has id, expects response
    MCP_MESSAGE_NOTIFICATION, // No id, no response expected
    MCP_MESSAGE_RESPONSE,     // Response to a request
    MCP_MESSAGE_ERROR         // Error response
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

// MCP Request Structure (simplified view of message)
typedef struct {
    char *jsonrpc;
    cJSON *id;
    char *method;
    cJSON *params;
    bool is_notification;  // true if this is a notification (no id)
} mcp_request_t;

// MCP Response Structure (simplified view of message)
typedef struct {
    char *jsonrpc;
    cJSON *id;
    cJSON *result;
    cJSON *error;
} mcp_response_t;

// MCP Error Structure
typedef struct {
    int code;
    char *message;
    cJSON *data;  // Optional additional error data
} mcp_error_t;

// Message creation functions
mcp_message_t *mcp_message_create_request(cJSON *id, const char *method, cJSON *params);
mcp_message_t *mcp_message_create_notification(const char *method, cJSON *params);
mcp_message_t *mcp_message_create_response(cJSON *id, cJSON *result);
mcp_message_t *mcp_message_create_error_response(cJSON *id, int code, const char *message, cJSON *data);

// Message parsing and serialization
mcp_message_t *mcp_message_parse(const char *json_data);
char *mcp_message_serialize(const mcp_message_t *message);

// Message validation
bool mcp_message_validate(const mcp_message_t *message);
bool mcp_request_validate(const mcp_request_t *request);
bool mcp_response_validate(const mcp_response_t *response);

// Message utilities
mcp_message_type_t mcp_message_get_type(const char *json_data);
bool mcp_message_has_id(const mcp_message_t *message);
bool mcp_message_is_notification(const mcp_message_t *message);

// Conversion functions
mcp_request_t *mcp_message_to_request(const mcp_message_t *message);
mcp_response_t *mcp_message_to_response(const mcp_message_t *message);
mcp_message_t *mcp_request_to_message(const mcp_request_t *request);
mcp_message_t *mcp_response_to_message(const mcp_response_t *response);

// Memory management
void mcp_message_destroy(mcp_message_t *message);
void mcp_request_destroy(mcp_request_t *request);
void mcp_response_destroy(mcp_response_t *response);
void mcp_error_destroy(mcp_error_t *error);

// Error creation helpers
mcp_error_t *mcp_error_create(int code, const char *message, cJSON *data);
cJSON *mcp_error_to_json(const mcp_error_t *error);
mcp_error_t *mcp_error_from_json(const cJSON *json);

#endif // MCP_MESSAGE_H
