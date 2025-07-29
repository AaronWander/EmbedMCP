#ifndef MCP_JSONRPC_H
#define MCP_JSONRPC_H

#include <stdbool.h>
#include <stddef.h>
#include "cJSON.h"
#include "message.h"

// JSON-RPC 2.0 specification constants
#define JSONRPC_VERSION "2.0"

// JSON-RPC field names
#define JSONRPC_FIELD_JSONRPC "jsonrpc"
#define JSONRPC_FIELD_ID "id"
#define JSONRPC_FIELD_METHOD "method"
#define JSONRPC_FIELD_PARAMS "params"
#define JSONRPC_FIELD_RESULT "result"
#define JSONRPC_FIELD_ERROR "error"
#define JSONRPC_FIELD_ERROR_CODE "code"
#define JSONRPC_FIELD_ERROR_MESSAGE "message"
#define JSONRPC_FIELD_ERROR_DATA "data"

// JSON-RPC Parser Context
typedef struct {
    bool strict_mode;        // Whether to enforce strict JSON-RPC compliance
    bool allow_extensions;   // Whether to allow MCP-specific extensions
    size_t max_message_size; // Maximum allowed message size
} jsonrpc_parser_config_t;

// JSON-RPC Parser
typedef struct jsonrpc_parser jsonrpc_parser_t;

// Parser creation and destruction
jsonrpc_parser_t *jsonrpc_parser_create(const jsonrpc_parser_config_t *config);
void jsonrpc_parser_destroy(jsonrpc_parser_t *parser);

// Message parsing functions
mcp_message_t *jsonrpc_parse_message(jsonrpc_parser_t *parser, const char *json_data);
mcp_request_t *jsonrpc_parse_request(jsonrpc_parser_t *parser, const char *json_data);
mcp_response_t *jsonrpc_parse_response(jsonrpc_parser_t *parser, const char *json_data);

// Message serialization functions
char *jsonrpc_serialize_message(const mcp_message_t *message);
char *jsonrpc_serialize_request(const mcp_request_t *request);
char *jsonrpc_serialize_response(const mcp_response_t *response);
char *jsonrpc_serialize_error(cJSON *id, int code, const char *message, cJSON *data);

// Validation functions
bool jsonrpc_validate_message(const cJSON *json);
bool jsonrpc_validate_request(const cJSON *json);
bool jsonrpc_validate_response(const cJSON *json);
bool jsonrpc_validate_error(const cJSON *json);

// Utility functions
bool jsonrpc_is_request(const cJSON *json);
bool jsonrpc_is_response(const cJSON *json);
bool jsonrpc_is_notification(const cJSON *json);
bool jsonrpc_is_error_response(const cJSON *json);

// ID handling
cJSON *jsonrpc_extract_id(const cJSON *json);
bool jsonrpc_id_match(const cJSON *id1, const cJSON *id2);
char *jsonrpc_id_to_string(const cJSON *id);

// Error handling
cJSON *jsonrpc_create_error_object(int code, const char *message, cJSON *data);
char *jsonrpc_create_error_response(cJSON *id, int code, const char *message, cJSON *data);

// Batch processing (for future extension)
typedef struct {
    mcp_message_t **messages;
    size_t count;
    size_t capacity;
} jsonrpc_batch_t;

jsonrpc_batch_t *jsonrpc_batch_create(void);
void jsonrpc_batch_destroy(jsonrpc_batch_t *batch);
int jsonrpc_batch_add_message(jsonrpc_batch_t *batch, mcp_message_t *message);
char *jsonrpc_batch_serialize(const jsonrpc_batch_t *batch);
jsonrpc_batch_t *jsonrpc_batch_parse(jsonrpc_parser_t *parser, const char *json_data);

// Configuration helpers
jsonrpc_parser_config_t *jsonrpc_config_create_default(void);
jsonrpc_parser_config_t *jsonrpc_config_create_strict(void);
jsonrpc_parser_config_t *jsonrpc_config_create_lenient(void);
void jsonrpc_config_destroy(jsonrpc_parser_config_t *config);

#endif // MCP_JSONRPC_H
