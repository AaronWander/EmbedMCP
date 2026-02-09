#include "protocol/jsonrpc.h"
#include "hal/platform_hal.h"
#include "hal/hal_common.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// JSON-RPC Parser structure
struct jsonrpc_parser {
    jsonrpc_parser_config_t config;
    size_t messages_parsed;
    size_t parse_errors;
};

// Parser creation and destruction
jsonrpc_parser_t *jsonrpc_parser_create(const jsonrpc_parser_config_t *config) {
    const mcp_platform_hal_t *hal = mcp_platform_get_hal();
    if (!hal) return NULL;

    jsonrpc_parser_t *parser = hal->memory.alloc(sizeof(jsonrpc_parser_t));
    if (!parser) return NULL;
    memset(parser, 0, sizeof(jsonrpc_parser_t));
    
    if (config) {
        parser->config = *config;
    } else {
        // Default configuration
        parser->config.strict_mode = true;
        parser->config.allow_extensions = true;
        parser->config.max_message_size = 1024 * 1024; // 1MB
    }
    
    return parser;
}

void jsonrpc_parser_destroy(jsonrpc_parser_t *parser) {
    if (!parser) return;
    const mcp_platform_hal_t *hal = mcp_platform_get_hal();
    if (hal) hal->memory.free(parser);
}

// Message parsing functions
mcp_message_t *jsonrpc_parse_message(jsonrpc_parser_t *parser, const char *json_data) {
    if (!parser || !json_data) return NULL;
    
    size_t data_len = strlen(json_data);
    if (data_len > parser->config.max_message_size) {
        parser->parse_errors++;
        return NULL;
    }
    
    mcp_message_t *message = mcp_message_parse(json_data);
    if (message) {
        parser->messages_parsed++;
    } else {
        parser->parse_errors++;
    }
    
    return message;
}

mcp_request_t *jsonrpc_parse_request(jsonrpc_parser_t *parser, const char *json_data) {
    mcp_message_t *message = jsonrpc_parse_message(parser, json_data);
    if (!message) return NULL;
    
    if (message->type != MCP_MESSAGE_REQUEST && message->type != MCP_MESSAGE_NOTIFICATION) {
        mcp_message_destroy(message);
        return NULL;
    }
    
    mcp_request_t *request = mcp_message_to_request(message);
    mcp_message_destroy(message);
    
    return request;
}

mcp_response_t *jsonrpc_parse_response(jsonrpc_parser_t *parser, const char *json_data) {
    mcp_message_t *message = jsonrpc_parse_message(parser, json_data);
    if (!message) return NULL;
    
    if (message->type != MCP_MESSAGE_RESPONSE && message->type != MCP_MESSAGE_ERROR) {
        mcp_message_destroy(message);
        return NULL;
    }
    
    mcp_response_t *response = mcp_message_to_response(message);
    mcp_message_destroy(message);
    
    return response;
}

// Message serialization functions
char *jsonrpc_serialize_message(const mcp_message_t *message) {
    return mcp_message_serialize(message);
}

char *jsonrpc_serialize_request(const mcp_request_t *request) {
    if (!request || !mcp_request_validate(request)) return NULL;
    
    cJSON *json = cJSON_CreateObject();
    if (!json) return NULL;
    
    cJSON_AddStringToObject(json, JSONRPC_FIELD_JSONRPC, request->jsonrpc ? request->jsonrpc : JSONRPC_VERSION);
    
    if (request->id) {
        cJSON_AddItemToObject(json, JSONRPC_FIELD_ID, cJSON_Duplicate(request->id, 1));
    }
    
    cJSON_AddStringToObject(json, JSONRPC_FIELD_METHOD, request->method);
    
    if (request->params) {
        cJSON_AddItemToObject(json, JSONRPC_FIELD_PARAMS, cJSON_Duplicate(request->params, 1));
    }
    
    char *json_string = cJSON_Print(json);
    cJSON_Delete(json);
    
    return json_string;
}

char *jsonrpc_serialize_response(const mcp_response_t *response) {
    if (!response || !mcp_response_validate(response)) return NULL;
    
    cJSON *json = cJSON_CreateObject();
    if (!json) return NULL;
    
    cJSON_AddStringToObject(json, JSONRPC_FIELD_JSONRPC, response->jsonrpc ? response->jsonrpc : JSONRPC_VERSION);
    
    if (response->id) {
        cJSON_AddItemToObject(json, JSONRPC_FIELD_ID, cJSON_Duplicate(response->id, 1));
    }
    
    if (response->result) {
        cJSON_AddItemToObject(json, JSONRPC_FIELD_RESULT, cJSON_Duplicate(response->result, 1));
    }
    
    if (response->error) {
        cJSON_AddItemToObject(json, JSONRPC_FIELD_ERROR, cJSON_Duplicate(response->error, 1));
    }
    
    char *json_string = cJSON_Print(json);
    cJSON_Delete(json);
    
    return json_string;
}

char *jsonrpc_serialize_error(cJSON *id, int code, const char *message, cJSON *data) {
    cJSON *json = cJSON_CreateObject();
    if (!json) return NULL;
    
    cJSON_AddStringToObject(json, JSONRPC_FIELD_JSONRPC, JSONRPC_VERSION);
    
    if (id) {
        cJSON_AddItemToObject(json, JSONRPC_FIELD_ID, cJSON_Duplicate(id, 1));
    } else {
        cJSON_AddNullToObject(json, JSONRPC_FIELD_ID);
    }
    
    cJSON *error_obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(error_obj, JSONRPC_FIELD_ERROR_CODE, code);
    cJSON_AddStringToObject(error_obj, JSONRPC_FIELD_ERROR_MESSAGE, message ? message : "Unknown error");
    
    if (data) {
        cJSON_AddItemToObject(error_obj, JSONRPC_FIELD_ERROR_DATA, cJSON_Duplicate(data, 1));
    }
    
    cJSON_AddItemToObject(json, JSONRPC_FIELD_ERROR, error_obj);
    
    char *json_string = cJSON_Print(json);
    cJSON_Delete(json);
    
    return json_string;
}

// Validation functions
bool jsonrpc_validate_message(const cJSON *json) {
    if (!json || !cJSON_IsObject(json)) return false;
    
    // Must have jsonrpc field with value "2.0"
    cJSON *jsonrpc = cJSON_GetObjectItem(json, JSONRPC_FIELD_JSONRPC);
    if (!jsonrpc || !cJSON_IsString(jsonrpc) || strcmp(jsonrpc->valuestring, JSONRPC_VERSION) != 0) {
        return false;
    }
    
    return true;
}

bool jsonrpc_validate_request(const cJSON *json) {
    if (!jsonrpc_validate_message(json)) return false;
    
    // Must have method field
    cJSON *method = cJSON_GetObjectItem(json, JSONRPC_FIELD_METHOD);
    if (!method || !cJSON_IsString(method)) return false;
    
    // If has id, it's a request; if no id, it's a notification
    // cJSON *id = cJSON_GetObjectItem(json, JSONRPC_FIELD_ID); // Unused for now
    
    // Must not have result or error fields
    if (cJSON_GetObjectItem(json, JSONRPC_FIELD_RESULT) || cJSON_GetObjectItem(json, JSONRPC_FIELD_ERROR)) {
        return false;
    }
    
    return true;
}

bool jsonrpc_validate_response(const cJSON *json) {
    if (!jsonrpc_validate_message(json)) return false;
    
    // Must have id field
    cJSON *id = cJSON_GetObjectItem(json, JSONRPC_FIELD_ID);
    if (!id) return false;
    
    // Must not have method field
    if (cJSON_GetObjectItem(json, JSONRPC_FIELD_METHOD)) return false;
    
    // Must have either result or error, but not both
    cJSON *result = cJSON_GetObjectItem(json, JSONRPC_FIELD_RESULT);
    cJSON *error = cJSON_GetObjectItem(json, JSONRPC_FIELD_ERROR);
    
    if ((result && error) || (!result && !error)) return false;
    
    return true;
}

bool jsonrpc_validate_error(const cJSON *json) {
    if (!json || !cJSON_IsObject(json)) return false;
    
    // Must have code and message fields
    cJSON *code = cJSON_GetObjectItem(json, JSONRPC_FIELD_ERROR_CODE);
    cJSON *message = cJSON_GetObjectItem(json, JSONRPC_FIELD_ERROR_MESSAGE);
    
    if (!code || !cJSON_IsNumber(code)) return false;
    if (!message || !cJSON_IsString(message)) return false;
    
    return true;
}

// Utility functions
bool jsonrpc_is_request(const cJSON *json) {
    if (!jsonrpc_validate_message(json)) return false;
    
    cJSON *method = cJSON_GetObjectItem(json, JSONRPC_FIELD_METHOD);
    cJSON *id = cJSON_GetObjectItem(json, JSONRPC_FIELD_ID);
    
    return method && cJSON_IsString(method) && id;
}

bool jsonrpc_is_response(const cJSON *json) {
    if (!jsonrpc_validate_message(json)) return false;
    
    cJSON *method = cJSON_GetObjectItem(json, JSONRPC_FIELD_METHOD);
    cJSON *id = cJSON_GetObjectItem(json, JSONRPC_FIELD_ID);
    cJSON *result = cJSON_GetObjectItem(json, JSONRPC_FIELD_RESULT);
    cJSON *error = cJSON_GetObjectItem(json, JSONRPC_FIELD_ERROR);
    
    return !method && id && (result || error);
}

bool jsonrpc_is_notification(const cJSON *json) {
    if (!jsonrpc_validate_message(json)) return false;
    
    cJSON *method = cJSON_GetObjectItem(json, JSONRPC_FIELD_METHOD);
    cJSON *id = cJSON_GetObjectItem(json, JSONRPC_FIELD_ID);
    
    return method && cJSON_IsString(method) && !id;
}

bool jsonrpc_is_error_response(const cJSON *json) {
    if (!jsonrpc_validate_message(json)) return false;
    
    cJSON *error = cJSON_GetObjectItem(json, JSONRPC_FIELD_ERROR);
    return error && cJSON_IsObject(error);
}

// ID handling
cJSON *jsonrpc_extract_id(const cJSON *json) {
    if (!json) return NULL;
    
    cJSON *id = cJSON_GetObjectItem(json, JSONRPC_FIELD_ID);
    return id ? cJSON_Duplicate(id, 1) : NULL;
}

bool jsonrpc_id_match(const cJSON *id1, const cJSON *id2) {
    if (!id1 && !id2) return true;
    if (!id1 || !id2) return false;
    
    if (cJSON_IsString(id1) && cJSON_IsString(id2)) {
        return strcmp(id1->valuestring, id2->valuestring) == 0;
    }
    
    if (cJSON_IsNumber(id1) && cJSON_IsNumber(id2)) {
        return id1->valuedouble == id2->valuedouble;
    }
    
    if (cJSON_IsNull(id1) && cJSON_IsNull(id2)) {
        return true;
    }
    
    return false;
}

char *jsonrpc_id_to_string(const cJSON *id) {
    const mcp_platform_hal_t *hal = mcp_platform_get_hal();
    if (!hal) return NULL;

    if (!id) return hal_strdup(hal, "null");
    
    if (cJSON_IsString(id)) {
        return hal_strdup(hal, id->valuestring);
    }
    
    if (cJSON_IsNumber(id)) {
        char *str = hal->memory.alloc(32);
        if (str) {
            snprintf(str, 32, "%.0f", id->valuedouble);
        }
        return str;
    }
    
    if (cJSON_IsNull(id)) {
        return hal_strdup(hal, "null");
    }
    
    return hal_strdup(hal, "unknown");
}

// Error creation helpers
cJSON *jsonrpc_create_error_object(int code, const char *message, cJSON *data) {
    cJSON *error = cJSON_CreateObject();
    if (!error) return NULL;

    cJSON_AddNumberToObject(error, JSONRPC_FIELD_ERROR_CODE, code);
    cJSON_AddStringToObject(error, JSONRPC_FIELD_ERROR_MESSAGE, message ? message : "Unknown error");

    if (data) {
        cJSON_AddItemToObject(error, JSONRPC_FIELD_ERROR_DATA, cJSON_Duplicate(data, 1));
    }

    return error;
}

char *jsonrpc_create_error_response(cJSON *id, int code, const char *message, cJSON *data) {
    return jsonrpc_serialize_error(id, code, message, data);
}

// Configuration helpers
jsonrpc_parser_config_t *jsonrpc_config_create_default(void) {
    const mcp_platform_hal_t *hal = mcp_platform_get_hal();
    if (!hal) return NULL;

    jsonrpc_parser_config_t *config = hal->memory.alloc(sizeof(jsonrpc_parser_config_t));
    if (!config) return NULL;
    memset(config, 0, sizeof(jsonrpc_parser_config_t));

    config->strict_mode = true;
    config->allow_extensions = true;
    config->max_message_size = 1024 * 1024; // 1MB

    return config;
}

jsonrpc_parser_config_t *jsonrpc_config_create_strict(void) {
    const mcp_platform_hal_t *hal = mcp_platform_get_hal();
    if (!hal) return NULL;

    jsonrpc_parser_config_t *config = hal->memory.alloc(sizeof(jsonrpc_parser_config_t));
    if (!config) return NULL;
    memset(config, 0, sizeof(jsonrpc_parser_config_t));

    config->strict_mode = true;
    config->allow_extensions = false;
    config->max_message_size = 512 * 1024; // 512KB

    return config;
}

jsonrpc_parser_config_t *jsonrpc_config_create_lenient(void) {
    const mcp_platform_hal_t *hal = mcp_platform_get_hal();
    if (!hal) return NULL;

    jsonrpc_parser_config_t *config = hal->memory.alloc(sizeof(jsonrpc_parser_config_t));
    if (!config) return NULL;
    memset(config, 0, sizeof(jsonrpc_parser_config_t));

    config->strict_mode = false;
    config->allow_extensions = true;
    config->max_message_size = 2 * 1024 * 1024; // 2MB

    return config;
}

void jsonrpc_config_destroy(jsonrpc_parser_config_t *config) {
    if (!config) return;
    const mcp_platform_hal_t *hal = mcp_platform_get_hal();
    if (hal) hal->memory.free(config);
}
