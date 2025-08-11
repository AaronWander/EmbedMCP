#include "protocol/mcp_protocol.h"
#include "hal/platform_hal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Protocol lifecycle
mcp_protocol_t *mcp_protocol_create(const mcp_protocol_config_t *config) {
    const mcp_platform_hal_t *hal = mcp_platform_get_hal();
    if (!hal) return NULL;

    mcp_protocol_t *protocol = hal->memory.alloc(sizeof(mcp_protocol_t));
    if (!protocol) return NULL;
    memset(protocol, 0, sizeof(mcp_protocol_t));

    // Initialize configuration
    if (config) {
        protocol->config = hal->memory.alloc(sizeof(mcp_protocol_config_t));
        if (!protocol->config) {
            hal->memory.free(protocol);
            return NULL;
        }
        *protocol->config = *config;
        
        // Duplicate string fields
        if (config->server_name) {
            protocol->config->server_name = strdup(config->server_name);
        }
        if (config->server_version) {
            protocol->config->server_version = strdup(config->server_version);
        }
        if (config->instructions) {
            protocol->config->instructions = strdup(config->instructions);
        }
        if (config->capabilities) {
            protocol->config->capabilities = calloc(1, sizeof(mcp_capabilities_t));
            if (protocol->config->capabilities) {
                *protocol->config->capabilities = *config->capabilities;
            }
        }
    } else {
        protocol->config = mcp_protocol_config_create_default();
        if (!protocol->config) {
            free(protocol);
            return NULL;
        }
    }
    
    // Initialize state machine
    protocol->state_machine = mcp_protocol_state_create();
    if (!protocol->state_machine) {
        mcp_protocol_config_destroy(protocol->config);
        free(protocol);
        return NULL;
    }
    
    // Initialize JSON-RPC parser
    jsonrpc_parser_config_t parser_config = {
        .strict_mode = protocol->config->strict_mode,
        .allow_extensions = true,
        .max_message_size = protocol->config->max_message_size
    };
    
    protocol->parser = jsonrpc_parser_create(&parser_config);
    if (!protocol->parser) {
        mcp_protocol_state_destroy(protocol->state_machine);
        mcp_protocol_config_destroy(protocol->config);
        free(protocol);
        return NULL;
    }
    
    protocol->initialized = false;
    protocol->pending_requests = 0;
    protocol->last_activity = time(NULL);
    
    return protocol;
}

void mcp_protocol_destroy(mcp_protocol_t *protocol) {
    if (!protocol) return;

    const mcp_platform_hal_t *hal = mcp_platform_get_hal();

    jsonrpc_parser_destroy(protocol->parser);
    mcp_protocol_state_destroy(protocol->state_machine);
    mcp_protocol_config_destroy(protocol->config);

    if (hal) {
        hal->memory.free(protocol);
    }
}

// Configuration management
mcp_protocol_config_t *mcp_protocol_config_create_default(void) {
    const mcp_platform_hal_t *hal = mcp_platform_get_hal();
    if (!hal) return NULL;

    mcp_protocol_config_t *config = hal->memory.alloc(sizeof(mcp_protocol_config_t));
    if (!config) return NULL;
    memset(config, 0, sizeof(mcp_protocol_config_t));
    
    config->strict_mode = true;
    config->enable_logging = true;
    config->max_message_size = 1024 * 1024; // 1MB
    config->max_pending_requests = 100;
    config->request_timeout = 30; // 30 seconds
    
    config->server_name = strdup("EmbedMCP");
    config->server_version = strdup("1.0.0");
    config->instructions = NULL;  // Will be set by user configuration
    config->capabilities = mcp_capabilities_create_default();
    
    return config;
}

void mcp_protocol_config_destroy(mcp_protocol_config_t *config) {
    if (!config) return;

    const mcp_platform_hal_t *hal = mcp_platform_get_hal();

    if (hal) {
        if (config->server_name) hal->memory.free(config->server_name);
        if (config->server_version) hal->memory.free(config->server_version);
        if (config->instructions) hal->memory.free(config->instructions);

        mcp_capabilities_destroy(config->capabilities);
        hal->memory.free(config);
    }
}

int mcp_protocol_config_set_server_info(mcp_protocol_config_t *config,
                                       const char *name, const char *version) {
    if (!config) return -1;

    free(config->server_name);
    free(config->server_version);

    config->server_name = name ? strdup(name) : NULL;
    config->server_version = version ? strdup(version) : NULL;

    return 0;
}

int mcp_protocol_config_set_instructions(mcp_protocol_config_t *config,
                                        const char *instructions) {
    if (!config) return -1;

    free(config->instructions);
    config->instructions = instructions ? strdup(instructions) : NULL;

    return 0;
}

// Callback registration
void mcp_protocol_set_send_callback(mcp_protocol_t *protocol, 
                                   mcp_send_callback_t callback, void *user_data) {
    if (!protocol) return;
    
    protocol->send_callback = callback;
    protocol->user_data = user_data;
}

void mcp_protocol_set_error_callback(mcp_protocol_t *protocol, 
                                    mcp_error_callback_t callback, void *user_data) {
    if (!protocol) return;
    
    protocol->error_callback = callback;
    protocol->user_data = user_data;
}

void mcp_protocol_set_state_change_callback(mcp_protocol_t *protocol,
                                           mcp_state_change_callback_t callback, void *user_data) {
    if (!protocol) return;
    
    protocol->state_change_callback = callback;
    protocol->user_data = user_data;
}

void mcp_protocol_set_request_handler(mcp_protocol_t *protocol,
                                     mcp_request_handler_t handler, void *user_data) {
    if (!protocol) return;
    
    protocol->request_handler = handler;
    protocol->user_data = user_data;
}

// Message handling
int mcp_protocol_handle_message(mcp_protocol_t *protocol, const char *json_data) {
    if (!protocol || !json_data) return -1;
    
    protocol->last_activity = time(NULL);
    
    mcp_message_t *message = jsonrpc_parse_message(protocol->parser, json_data);
    if (!message) {
        if (protocol->error_callback) {
            protocol->error_callback(JSONRPC_PARSE_ERROR, "Failed to parse JSON-RPC message", protocol->user_data);
        }
        return mcp_protocol_send_parse_error(protocol, NULL);
    }
    
    int result = 0;
    
    switch (message->type) {
        case MCP_MESSAGE_REQUEST: {
            mcp_request_t *request = mcp_message_to_request(message);
            if (request) {
                result = mcp_protocol_handle_request(protocol, request);
                mcp_request_destroy(request);
            } else {
                result = mcp_protocol_send_invalid_request_error(protocol, message->id);
            }
            break;
        }
        
        case MCP_MESSAGE_NOTIFICATION: {
            mcp_request_t *notification = mcp_message_to_request(message);
            if (notification) {
                result = mcp_protocol_handle_notification(protocol, notification);
                mcp_request_destroy(notification);
            }
            break;
        }
        
        case MCP_MESSAGE_RESPONSE:
        case MCP_MESSAGE_ERROR: {
            mcp_response_t *response = mcp_message_to_response(message);
            if (response) {
                result = mcp_protocol_handle_response(protocol, response);
                mcp_response_destroy(response);
            }
            break;
        }
        
        default:
            result = mcp_protocol_send_invalid_request_error(protocol, message->id);
            break;
    }
    
    mcp_message_destroy(message);
    return result;
}

int mcp_protocol_handle_request(mcp_protocol_t *protocol, const mcp_request_t *request) {
    if (!protocol || !request) return -1;

    cJSON *result = NULL;

    // Handle built-in methods directly without state checks
    if (strcmp(request->method, MCP_METHOD_INITIALIZE) == 0) {
        result = mcp_protocol_handle_initialize(protocol, request);
    } else if (strcmp(request->method, MCP_METHOD_PING) == 0) {
        result = mcp_protocol_handle_ping(protocol, request);
    } else if (protocol->request_handler) {
        // Delegate to application-level handler (tools/list, tools/call, etc.)
        result = protocol->request_handler(request, protocol->user_data);
    } else {
        return mcp_protocol_send_method_not_found_error(protocol, request->id, request->method);
    }

    if (result) {
        int send_result = mcp_protocol_send_response(protocol, request->id, result);
        cJSON_Delete(result);
        return send_result;
    } else {
        return mcp_protocol_send_internal_error(protocol, request->id, "Request handler returned null");
    }
}

int mcp_protocol_handle_response(mcp_protocol_t *protocol, const mcp_response_t *response) {
    if (!protocol || !response) return -1;
    
    // For now, we don't track pending requests, so just log the response
    if (protocol->config->enable_logging) {
        char *id_str = jsonrpc_id_to_string(response->id);
        fprintf(stderr, "Received response for request ID: %s\n", id_str ? id_str : "null");
        free(id_str);
    }
    
    return 0;
}

int mcp_protocol_handle_notification(mcp_protocol_t *protocol, const mcp_request_t *notification) {
    if (!protocol || !notification) return -1;
    
    // Handle built-in notifications
    if (strcmp(notification->method, MCP_METHOD_INITIALIZED) == 0) {
        return mcp_protocol_handle_initialized(protocol, notification);
    }
    
    // For other notifications, just log them for now
    if (protocol->config->enable_logging) {
        fprintf(stderr, "Received notification: %s\n", notification->method);
    }
    
    return 0;
}

// Message sending
int mcp_protocol_send_response(mcp_protocol_t *protocol, cJSON *id, cJSON *result) {
    if (!protocol || !protocol->send_callback) return -1;
    
    mcp_response_t response = {
        .jsonrpc = "2.0",
        .id = id,
        .result = result,
        .error = NULL
    };
    
    char *json_str = jsonrpc_serialize_response(&response);
    if (!json_str) return -1;
    
    int send_result = protocol->send_callback(json_str, strlen(json_str), protocol->user_data);
    free(json_str);
    
    return send_result;
}

int mcp_protocol_send_error_response(mcp_protocol_t *protocol, cJSON *id, 
                                    int code, const char *message, cJSON *data) {
    if (!protocol || !protocol->send_callback) return -1;
    
    char *json_str = jsonrpc_serialize_error(id, code, message, data);
    if (!json_str) return -1;
    
    int send_result = protocol->send_callback(json_str, strlen(json_str), protocol->user_data);
    free(json_str);
    
    return send_result;
}

int mcp_protocol_send_request(mcp_protocol_t *protocol, cJSON *id,
                             const char *method, cJSON *params) {
    if (!protocol || !protocol->send_callback || !method) return -1;

    mcp_request_t request = {
        .jsonrpc = "2.0",
        .id = id,
        .method = (char*)method,
        .params = params,
        .is_notification = false
    };

    char *json_str = jsonrpc_serialize_request(&request);
    if (!json_str) return -1;

    int send_result = protocol->send_callback(json_str, strlen(json_str), protocol->user_data);
    free(json_str);

    if (send_result == 0) {
        protocol->pending_requests++;
    }

    return send_result;
}

// Built-in method handlers
cJSON *mcp_protocol_handle_initialize(mcp_protocol_t *protocol, const mcp_request_t *request) {
    if (!protocol || !request) return NULL;

    // Simplified initialization - no complex state management
    // Parse initialize parameters
    if (!request->params || !cJSON_IsObject(request->params)) {
        return NULL;
    }

    cJSON *protocol_version = cJSON_GetObjectItem(request->params, "protocolVersion");
    if (!protocol_version || !cJSON_IsString(protocol_version)) {
        return NULL;
    }

    // Create response directly
    cJSON *result = cJSON_CreateObject();
    if (!result) return NULL;

    cJSON_AddStringToObject(result, "protocolVersion", MCP_PROTOCOL_VERSION);

    // Server info - use configuration
    cJSON *server_info = mcp_protocol_create_server_info(protocol);
    if (server_info) {
        cJSON_AddItemToObject(result, "serverInfo", server_info);
    }

    // Server capabilities - use configuration
    cJSON *capabilities = mcp_protocol_create_capabilities_json(protocol);
    if (capabilities) {
        cJSON_AddItemToObject(result, "capabilities", capabilities);
    }

    // Instructions (optional) - use configuration
    if (protocol->config && protocol->config->instructions &&
        strlen(protocol->config->instructions) > 0) {
        cJSON_AddStringToObject(result, "instructions", protocol->config->instructions);
    }

    protocol->initialized = true;

    return result;
}

int mcp_protocol_handle_initialized(mcp_protocol_t *protocol, const mcp_request_t *notification) {
    if (!protocol || !notification) return -1;

    // Simplified - just acknowledge the notification
    // No complex state management needed
    return 0;
}

cJSON *mcp_protocol_handle_ping(mcp_protocol_t *protocol, const mcp_request_t *request) {
    if (!protocol || !request) return NULL;

    // According to MCP spec, ping response must be an empty object
    cJSON *result = cJSON_CreateObject();

    return result;
}

// Session info (kept for potential future use)
const mcp_session_info_t *mcp_protocol_get_session_info(const mcp_protocol_t *protocol) {
    return protocol ? &protocol->state_machine->session_info : NULL;
}

// Utility functions
bool mcp_protocol_is_builtin_method(const char *method) {
    if (!method) return false;

    return strcmp(method, MCP_METHOD_INITIALIZE) == 0 ||
           strcmp(method, MCP_METHOD_INITIALIZED) == 0 ||
           strcmp(method, MCP_METHOD_PING) == 0;
}

const char *mcp_protocol_get_version(void) {
    return MCP_PROTOCOL_VERSION;
}

cJSON *mcp_protocol_create_server_info(const mcp_protocol_t *protocol) {
    if (!protocol || !protocol->config) return NULL;

    cJSON *server_info = cJSON_CreateObject();
    if (!server_info) return NULL;

    if (protocol->config->server_name) {
        cJSON_AddStringToObject(server_info, "name", protocol->config->server_name);
    }

    if (protocol->config->server_version) {
        cJSON_AddStringToObject(server_info, "version", protocol->config->server_version);
    }

    return server_info;
}

cJSON *mcp_protocol_create_capabilities_json(const mcp_protocol_t *protocol) {
    if (!protocol || !protocol->config || !protocol->config->capabilities) return NULL;

    return mcp_capabilities_to_json(protocol->config->capabilities);
}



// Error helpers
int mcp_protocol_send_parse_error(mcp_protocol_t *protocol, cJSON *id) {
    return mcp_protocol_send_error_response(protocol, id, JSONRPC_PARSE_ERROR, "Parse error", NULL);
}

int mcp_protocol_send_invalid_request_error(mcp_protocol_t *protocol, cJSON *id) {
    return mcp_protocol_send_error_response(protocol, id, JSONRPC_INVALID_REQUEST, "Invalid request", NULL);
}

int mcp_protocol_send_method_not_found_error(mcp_protocol_t *protocol, cJSON *id, const char *method) {
    cJSON *data = cJSON_CreateObject();
    if (data && method) {
        cJSON_AddStringToObject(data, "method", method);
    }

    int result = mcp_protocol_send_error_response(protocol, id, JSONRPC_METHOD_NOT_FOUND, "Method not found", data);

    if (data) cJSON_Delete(data);
    return result;
}

int mcp_protocol_send_invalid_params_error(mcp_protocol_t *protocol, cJSON *id, const char *details) {
    cJSON *data = NULL;
    if (details) {
        data = cJSON_CreateObject();
        cJSON_AddStringToObject(data, "details", details);
    }

    int result = mcp_protocol_send_error_response(protocol, id, JSONRPC_INVALID_PARAMS, "Invalid params", data);

    if (data) cJSON_Delete(data);
    return result;
}

int mcp_protocol_send_internal_error(mcp_protocol_t *protocol, cJSON *id, const char *details) {
    cJSON *data = NULL;
    if (details) {
        data = cJSON_CreateObject();
        cJSON_AddStringToObject(data, "details", details);
    }

    int result = mcp_protocol_send_error_response(protocol, id, JSONRPC_INTERNAL_ERROR, "Internal error", data);

    if (data) cJSON_Delete(data);
    return result;
}

int mcp_protocol_send_notification(mcp_protocol_t *protocol, const char *method, cJSON *params) {
    if (!protocol || !protocol->send_callback || !method) return -1;
    
    mcp_request_t notification = {
        .jsonrpc = "2.0",
        .id = NULL,
        .method = (char*)method,
        .params = params,
        .is_notification = true
    };
    
    char *json_str = jsonrpc_serialize_request(&notification);
    if (!json_str) return -1;
    
    int send_result = protocol->send_callback(json_str, strlen(json_str), protocol->user_data);
    free(json_str);
    
    return send_result;
}
