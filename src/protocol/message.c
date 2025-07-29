#include "protocol/message.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Message creation functions
mcp_message_t *mcp_message_create_request(cJSON *id, const char *method, cJSON *params) {
    if (!method) return NULL;
    
    mcp_message_t *message = calloc(1, sizeof(mcp_message_t));
    if (!message) return NULL;
    
    message->type = MCP_MESSAGE_REQUEST;
    message->jsonrpc = strdup("2.0");
    message->id = id ? cJSON_Duplicate(id, 1) : NULL;
    message->method = strdup(method);
    message->params = params ? cJSON_Duplicate(params, 1) : NULL;
    message->result = NULL;
    message->error = NULL;
    
    if (!message->jsonrpc || !message->method) {
        mcp_message_destroy(message);
        return NULL;
    }
    
    return message;
}

mcp_message_t *mcp_message_create_notification(const char *method, cJSON *params) {
    if (!method) return NULL;
    
    mcp_message_t *message = calloc(1, sizeof(mcp_message_t));
    if (!message) return NULL;
    
    message->type = MCP_MESSAGE_NOTIFICATION;
    message->jsonrpc = strdup("2.0");
    message->id = NULL;  // Notifications don't have IDs
    message->method = strdup(method);
    message->params = params ? cJSON_Duplicate(params, 1) : NULL;
    message->result = NULL;
    message->error = NULL;
    
    if (!message->jsonrpc || !message->method) {
        mcp_message_destroy(message);
        return NULL;
    }
    
    return message;
}

mcp_message_t *mcp_message_create_response(cJSON *id, cJSON *result) {
    mcp_message_t *message = calloc(1, sizeof(mcp_message_t));
    if (!message) return NULL;
    
    message->type = MCP_MESSAGE_RESPONSE;
    message->jsonrpc = strdup("2.0");
    message->id = id ? cJSON_Duplicate(id, 1) : NULL;
    message->method = NULL;
    message->params = NULL;
    message->result = result ? cJSON_Duplicate(result, 1) : cJSON_CreateNull();
    message->error = NULL;
    
    if (!message->jsonrpc) {
        mcp_message_destroy(message);
        return NULL;
    }
    
    return message;
}

mcp_message_t *mcp_message_create_error_response(cJSON *id, int code, const char *message_text, cJSON *data) {
    mcp_message_t *message = calloc(1, sizeof(mcp_message_t));
    if (!message) return NULL;
    
    message->type = MCP_MESSAGE_ERROR;
    message->jsonrpc = strdup("2.0");
    message->id = id ? cJSON_Duplicate(id, 1) : NULL;
    message->method = NULL;
    message->params = NULL;
    message->result = NULL;
    
    // Create error object
    message->error = cJSON_CreateObject();
    if (!message->error) {
        mcp_message_destroy(message);
        return NULL;
    }
    
    cJSON_AddNumberToObject(message->error, "code", code);
    cJSON_AddStringToObject(message->error, "message", message_text ? message_text : "Unknown error");
    if (data) {
        cJSON_AddItemToObject(message->error, "data", cJSON_Duplicate(data, 1));
    }
    
    if (!message->jsonrpc) {
        mcp_message_destroy(message);
        return NULL;
    }
    
    return message;
}

// Message parsing
mcp_message_t *mcp_message_parse(const char *json_data) {
    if (!json_data) return NULL;
    
    cJSON *json = cJSON_Parse(json_data);
    if (!json) return NULL;
    
    mcp_message_t *message = calloc(1, sizeof(mcp_message_t));
    if (!message) {
        cJSON_Delete(json);
        return NULL;
    }
    
    // Parse jsonrpc field
    cJSON *jsonrpc = cJSON_GetObjectItem(json, "jsonrpc");
    if (jsonrpc && cJSON_IsString(jsonrpc)) {
        message->jsonrpc = strdup(jsonrpc->valuestring);
    }
    
    // Parse id field
    cJSON *id = cJSON_GetObjectItem(json, "id");
    if (id) {
        message->id = cJSON_Duplicate(id, 1);
    }
    
    // Parse method field
    cJSON *method = cJSON_GetObjectItem(json, "method");
    if (method && cJSON_IsString(method)) {
        message->method = strdup(method->valuestring);
    }
    
    // Parse params field
    cJSON *params = cJSON_GetObjectItem(json, "params");
    if (params) {
        message->params = cJSON_Duplicate(params, 1);
    }
    
    // Parse result field
    cJSON *result = cJSON_GetObjectItem(json, "result");
    if (result) {
        message->result = cJSON_Duplicate(result, 1);
    }
    
    // Parse error field
    cJSON *error = cJSON_GetObjectItem(json, "error");
    if (error) {
        message->error = cJSON_Duplicate(error, 1);
    }
    
    // Determine message type
    if (message->method) {
        message->type = message->id ? MCP_MESSAGE_REQUEST : MCP_MESSAGE_NOTIFICATION;
    } else if (message->error) {
        message->type = MCP_MESSAGE_ERROR;
    } else {
        message->type = MCP_MESSAGE_RESPONSE;
    }
    
    cJSON_Delete(json);
    
    if (!mcp_message_validate(message)) {
        mcp_message_destroy(message);
        return NULL;
    }
    
    return message;
}

// Message serialization
char *mcp_message_serialize(const mcp_message_t *message) {
    if (!message || !mcp_message_validate(message)) return NULL;
    
    cJSON *json = cJSON_CreateObject();
    if (!json) return NULL;
    
    // Add jsonrpc field
    cJSON_AddStringToObject(json, "jsonrpc", message->jsonrpc ? message->jsonrpc : "2.0");
    
    // Add id field (if present)
    if (message->id) {
        cJSON_AddItemToObject(json, "id", cJSON_Duplicate(message->id, 1));
    }
    
    // Add method field (for requests and notifications)
    if (message->method) {
        cJSON_AddStringToObject(json, "method", message->method);
    }
    
    // Add params field (if present)
    if (message->params) {
        cJSON_AddItemToObject(json, "params", cJSON_Duplicate(message->params, 1));
    }
    
    // Add result field (for successful responses)
    if (message->result) {
        cJSON_AddItemToObject(json, "result", cJSON_Duplicate(message->result, 1));
    }
    
    // Add error field (for error responses)
    if (message->error) {
        cJSON_AddItemToObject(json, "error", cJSON_Duplicate(message->error, 1));
    }
    
    char *json_string = cJSON_Print(json);
    cJSON_Delete(json);
    
    return json_string;
}

// Message validation
bool mcp_message_validate(const mcp_message_t *message) {
    if (!message) return false;
    
    // Must have jsonrpc field
    if (!message->jsonrpc || strcmp(message->jsonrpc, "2.0") != 0) {
        return false;
    }
    
    switch (message->type) {
        case MCP_MESSAGE_REQUEST:
            // Requests must have id and method
            return message->id && message->method && !message->result && !message->error;
            
        case MCP_MESSAGE_NOTIFICATION:
            // Notifications must have method but no id
            return !message->id && message->method && !message->result && !message->error;
            
        case MCP_MESSAGE_RESPONSE:
            // Responses must have id and result, but no method or error
            return message->id && !message->method && message->result && !message->error;
            
        case MCP_MESSAGE_ERROR:
            // Error responses must have id and error, but no method or result
            return message->id && !message->method && !message->result && message->error;
            
        default:
            return false;
    }
}

// Utility functions
mcp_message_type_t mcp_message_get_type(const char *json_data) {
    if (!json_data) return MCP_MESSAGE_ERROR;

    cJSON *json = cJSON_Parse(json_data);
    if (!json) return MCP_MESSAGE_ERROR;

    cJSON *method = cJSON_GetObjectItem(json, "method");
    cJSON *id = cJSON_GetObjectItem(json, "id");
    cJSON *error = cJSON_GetObjectItem(json, "error");

    mcp_message_type_t type;
    if (method) {
        type = id ? MCP_MESSAGE_REQUEST : MCP_MESSAGE_NOTIFICATION;
    } else if (error) {
        type = MCP_MESSAGE_ERROR;
    } else {
        type = MCP_MESSAGE_RESPONSE;
    }

    cJSON_Delete(json);
    return type;
}

bool mcp_message_has_id(const mcp_message_t *message) {
    return message && message->id;
}

bool mcp_message_is_notification(const mcp_message_t *message) {
    return message && message->type == MCP_MESSAGE_NOTIFICATION;
}

// Conversion functions
mcp_request_t *mcp_message_to_request(const mcp_message_t *message) {
    if (!message || (message->type != MCP_MESSAGE_REQUEST && message->type != MCP_MESSAGE_NOTIFICATION)) {
        return NULL;
    }

    mcp_request_t *request = calloc(1, sizeof(mcp_request_t));
    if (!request) return NULL;

    request->jsonrpc = message->jsonrpc ? strdup(message->jsonrpc) : NULL;
    request->id = message->id ? cJSON_Duplicate(message->id, 1) : NULL;
    request->method = message->method ? strdup(message->method) : NULL;
    request->params = message->params ? cJSON_Duplicate(message->params, 1) : NULL;
    request->is_notification = (message->type == MCP_MESSAGE_NOTIFICATION);

    return request;
}

mcp_response_t *mcp_message_to_response(const mcp_message_t *message) {
    if (!message || (message->type != MCP_MESSAGE_RESPONSE && message->type != MCP_MESSAGE_ERROR)) {
        return NULL;
    }

    mcp_response_t *response = calloc(1, sizeof(mcp_response_t));
    if (!response) return NULL;

    response->jsonrpc = message->jsonrpc ? strdup(message->jsonrpc) : NULL;
    response->id = message->id ? cJSON_Duplicate(message->id, 1) : NULL;
    response->result = message->result ? cJSON_Duplicate(message->result, 1) : NULL;
    response->error = message->error ? cJSON_Duplicate(message->error, 1) : NULL;

    return response;
}

// Validation functions
bool mcp_request_validate(const mcp_request_t *request) {
    if (!request) return false;

    // Must have jsonrpc and method
    if (!request->jsonrpc || strcmp(request->jsonrpc, "2.0") != 0) return false;
    if (!request->method) return false;

    // If not a notification, must have id
    if (!request->is_notification && !request->id) return false;

    // If is a notification, must not have id
    if (request->is_notification && request->id) return false;

    return true;
}

bool mcp_response_validate(const mcp_response_t *response) {
    if (!response) return false;

    // Must have jsonrpc and id
    if (!response->jsonrpc || strcmp(response->jsonrpc, "2.0") != 0) return false;
    if (!response->id) return false;

    // Must have either result or error, but not both
    if (response->result && response->error) return false;
    if (!response->result && !response->error) return false;

    return true;
}

// Error handling
mcp_error_t *mcp_error_create(int code, const char *message, cJSON *data) {
    mcp_error_t *error = calloc(1, sizeof(mcp_error_t));
    if (!error) return NULL;

    error->code = code;
    error->message = message ? strdup(message) : NULL;
    error->data = data ? cJSON_Duplicate(data, 1) : NULL;

    return error;
}

cJSON *mcp_error_to_json(const mcp_error_t *error) {
    if (!error) return NULL;

    cJSON *json = cJSON_CreateObject();
    if (!json) return NULL;

    cJSON_AddNumberToObject(json, "code", error->code);
    cJSON_AddStringToObject(json, "message", error->message ? error->message : "Unknown error");

    if (error->data) {
        cJSON_AddItemToObject(json, "data", cJSON_Duplicate(error->data, 1));
    }

    return json;
}

mcp_error_t *mcp_error_from_json(const cJSON *json) {
    if (!json || !cJSON_IsObject(json)) return NULL;

    cJSON *code_item = cJSON_GetObjectItem(json, "code");
    cJSON *message_item = cJSON_GetObjectItem(json, "message");
    cJSON *data_item = cJSON_GetObjectItem(json, "data");

    if (!code_item || !cJSON_IsNumber(code_item)) return NULL;
    if (!message_item || !cJSON_IsString(message_item)) return NULL;

    mcp_error_t *error = calloc(1, sizeof(mcp_error_t));
    if (!error) return NULL;

    error->code = (int)code_item->valuedouble;
    error->message = strdup(message_item->valuestring);
    error->data = data_item ? cJSON_Duplicate(data_item, 1) : NULL;

    return error;
}

// Memory management
void mcp_message_destroy(mcp_message_t *message) {
    if (!message) return;

    free(message->jsonrpc);
    free(message->method);

    if (message->id) cJSON_Delete(message->id);
    if (message->params) cJSON_Delete(message->params);
    if (message->result) cJSON_Delete(message->result);
    if (message->error) cJSON_Delete(message->error);

    free(message);
}

void mcp_request_destroy(mcp_request_t *request) {
    if (!request) return;

    free(request->jsonrpc);
    free(request->method);

    if (request->id) cJSON_Delete(request->id);
    if (request->params) cJSON_Delete(request->params);

    free(request);
}

void mcp_response_destroy(mcp_response_t *response) {
    if (!response) return;

    free(response->jsonrpc);

    if (response->id) cJSON_Delete(response->id);
    if (response->result) cJSON_Delete(response->result);
    if (response->error) cJSON_Delete(response->error);

    free(response);
}

void mcp_error_destroy(mcp_error_t *error) {
    if (!error) return;

    free(error->message);
    if (error->data) cJSON_Delete(error->data);

    free(error);
}
