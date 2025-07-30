#include "embed_mcp.h"
#include "protocol/mcp_protocol.h"
#include "transport/transport_interface.h"
#include "tools/tool_registry.h"
#include "tools/tool_interface.h"
#include "utils/logging.h"
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

// Global error message
static char g_error_message[512] = {0};
static volatile int g_running = 1;

// Server structure
struct embed_mcp_server {
    char *name;
    char *version;
    char *host;
    int port;
    char *path;
    int debug;

    mcp_protocol_t *protocol;
    mcp_transport_t *transport;
    mcp_tool_registry_t *tool_registry;
    mcp_connection_t *current_connection;

    int running;
};

// Signal handler for graceful shutdown
static void signal_handler(int sig) {
    (void)sig;
    g_running = 0;
}

// Set error message
static void set_error(const char *message) {
    strncpy(g_error_message, message ? message : "Unknown error", sizeof(g_error_message) - 1);
    g_error_message[sizeof(g_error_message) - 1] = '\0';
}

// Protocol send callback
static int protocol_send_callback(const char *data, size_t length, void *user_data) {
    embed_mcp_server_t *server = (embed_mcp_server_t*)user_data;
    
    if (!server || !server->current_connection) {
        return -1;
    }
    
    return mcp_connection_send(server->current_connection, data, length);
}

// Protocol request handler
static cJSON *protocol_request_handler(const mcp_request_t *request, void *user_data) {
    embed_mcp_server_t *server = (embed_mcp_server_t*)user_data;
    
    if (!server || !request || !request->method) {
        return NULL;
    }
    
    // Handle tools/list
    if (strcmp(request->method, "tools/list") == 0) {
        cJSON *tools = mcp_tool_registry_list_tools(server->tool_registry);
        if (!tools) return NULL;
        
        cJSON *result = cJSON_CreateObject();
        cJSON_AddItemToObject(result, "tools", tools);
        return result;
    }
    
    // Handle tools/call
    if (strcmp(request->method, "tools/call") == 0) {
        if (!request->params) return NULL;
        
        cJSON *name = cJSON_GetObjectItem(request->params, "name");
        cJSON *arguments = cJSON_GetObjectItem(request->params, "arguments");
        
        if (!name || !cJSON_IsString(name)) return NULL;
        
        return mcp_tool_registry_call_tool(server->tool_registry, name->valuestring, arguments);
    }
    
    return NULL;
}

// Transport callbacks
static void on_message_received(const char *message, size_t length,
                               mcp_connection_t *connection, void *user_data) {
    embed_mcp_server_t *server = (embed_mcp_server_t*)user_data;
    
    if (server->debug) {
        mcp_log_debug("Received message (%zu bytes): %.*s", length, (int)length, message);
    }
    
    server->current_connection = connection;
    mcp_protocol_handle_message(server->protocol, message);
    server->current_connection = NULL;
}

static void on_connection_opened(mcp_connection_t *connection, void *user_data) {
    embed_mcp_server_t *server = (embed_mcp_server_t*)user_data;
    
    if (server->debug) {
        mcp_log_info("Connection opened: %s", mcp_connection_get_id(connection));
    }
}

static void on_connection_closed(mcp_connection_t *connection, void *user_data) {
    embed_mcp_server_t *server = (embed_mcp_server_t*)user_data;
    
    if (server->debug) {
        mcp_log_info("Connection closed: %s", mcp_connection_get_id(connection));
    }
}

static void on_transport_error(mcp_transport_t *transport, int error_code,
                              const char *error_message, void *user_data) {
    (void)transport;
    (void)user_data;
    mcp_log_error("Transport error %d: %s", error_code, error_message);
}

// =============================================================================
// API Implementation
// =============================================================================

embed_mcp_server_t *embed_mcp_create(const embed_mcp_config_t *config) {
    if (!config || !config->name || !config->version) {
        set_error("Invalid configuration");
        return NULL;
    }
    
    embed_mcp_server_t *server = calloc(1, sizeof(embed_mcp_server_t));
    if (!server) {
        set_error("Memory allocation failed");
        return NULL;
    }
    
    // Copy configuration
    server->name = strdup(config->name);
    server->version = strdup(config->version);
    server->host = strdup(config->host ? config->host : "0.0.0.0");
    server->port = config->port > 0 ? config->port : 8080;
    server->path = strdup(config->path ? config->path : "/mcp");
    server->debug = config->debug;

    if (!server->name || !server->version || !server->host || !server->path) {
        embed_mcp_destroy(server);
        set_error("Memory allocation failed");
        return NULL;
    }
    
    // Create tool registry
    mcp_tool_registry_config_t registry_config = {0};
    registry_config.max_tools = config->max_tools > 0 ? config->max_tools : 100;
    registry_config.enable_builtin_tools = false;
    registry_config.enable_tool_stats = true;
    registry_config.strict_validation = true;
    registry_config.tool_timeout = 30; // 30 seconds

    server->tool_registry = mcp_tool_registry_create(&registry_config);
    if (!server->tool_registry) {
        embed_mcp_destroy(server);
        set_error("Failed to create tool registry");
        return NULL;
    }

    // Create protocol with default config
    server->protocol = mcp_protocol_create(NULL);
    
    if (!server->protocol) {
        embed_mcp_destroy(server);
        set_error("Failed to create protocol");
        return NULL;
    }
    
    // Set protocol callbacks
    mcp_protocol_set_send_callback(server->protocol, protocol_send_callback, server);
    mcp_protocol_set_request_handler(server->protocol, protocol_request_handler, server);

    // Initialize logging system
    mcp_log_config_t *log_config = mcp_log_config_create_default();
    if (log_config) {
        if (server->debug) {
            log_config->min_level = MCP_LOG_LEVEL_DEBUG;
        } else {
            log_config->min_level = MCP_LOG_LEVEL_INFO;
        }
        mcp_log_init(log_config);
        mcp_log_config_destroy(log_config);
    }

    return server;
}

void embed_mcp_destroy(embed_mcp_server_t *server) {
    if (!server) return;
    
    if (server->transport) {
        mcp_transport_destroy(server->transport);
    }
    
    if (server->protocol) {
        mcp_protocol_destroy(server->protocol);
    }
    
    if (server->tool_registry) {
        mcp_tool_registry_destroy(server->tool_registry);
    }
    
    free(server->name);
    free(server->version);
    free(server->host);
    free(server->path);
    free(server);
}



// Removed: Helper functions for old convenience APIs
// create_math_tool_schema() and create_text_tool_schema() were used by
// the removed embed_mcp_add_math_tool and embed_mcp_add_text_tool functions

// Removed: embed_mcp_add_math_tool and embed_mcp_add_text_tool
// These convenience functions were replaced by the unified pure function API

// Wrapper function to adapt our handler signature to the expected one
static cJSON *tool_handler_wrapper(const cJSON *args, void *user_data) {
    embed_mcp_tool_handler_t handler = (embed_mcp_tool_handler_t)user_data;
    return handler(args);
}

/*
 * RESERVED FOR FUTURE USE: Complex parameter structures
 *
 * This implementation is kept in code but commented out for future use.
 * Uncomment when we encounter complex nested parameter structures that
 * the pure function API cannot handle.
 */
/*
int embed_mcp_add_tool_with_schema(embed_mcp_server_t *server,
                                   const char *name,
                                   const char *description,
                                   const cJSON *schema,
                                   embed_mcp_tool_handler_t handler) {
    if (!server || !name || !description || !handler) {
        set_error("Invalid parameters");
        return -1;
    }

    // Create default schema if none provided
    cJSON *input_schema = NULL;
    if (schema) {
        input_schema = cJSON_Duplicate(schema, 1);
    } else {
        // Create a basic generic schema if no schema provided
        input_schema = cJSON_CreateObject();

        cJSON_AddStringToObject(input_schema, "$schema", "http://json-schema.org/draft-07/schema#");
        cJSON_AddStringToObject(input_schema, "type", "object");
        cJSON_AddStringToObject(input_schema, "title", "Tool Parameters");
        cJSON_AddStringToObject(input_schema, "description", "Parameters for the tool");

        // Empty properties object - allows any parameters
        cJSON *properties = cJSON_CreateObject();
        cJSON_AddItemToObject(input_schema, "properties", properties);

        // No required fields
        cJSON *required = cJSON_CreateArray();
        cJSON_AddItemToObject(input_schema, "required", required);

        cJSON_AddBoolToObject(input_schema, "additionalProperties", true);
    }

    // Create tool with schema
    mcp_tool_t *tool = mcp_tool_create(name, name, description, input_schema,
                                      tool_handler_wrapper, (void*)handler);

    // Clean up schema
    if (input_schema) {
        cJSON_Delete(input_schema);
    }

    if (!tool) {
        set_error("Failed to create tool");
        return -1;
    }

    // Register tool
    if (mcp_tool_registry_register_tool(server->tool_registry, tool) != 0) {
        mcp_tool_destroy(tool);
        set_error("Failed to register tool");
        return -1;
    }

    return 0;
}
*/



int embed_mcp_run(embed_mcp_server_t *server, embed_mcp_transport_t transport) {
    if (!server) {
        set_error("Invalid server");
        return -1;
    }

    // Create transport
    if (transport == EMBED_MCP_TRANSPORT_STDIO) {
        server->transport = mcp_transport_create_stdio();
    } else {
        server->transport = mcp_transport_create_http(server->port, server->host);
    }

    if (!server->transport) {
        set_error("Failed to create transport");
        return -1;
    }

    // Set transport callbacks
    mcp_transport_set_callbacks(server->transport,
                               on_message_received,
                               on_connection_opened,
                               on_connection_closed,
                               on_transport_error,
                               server);

    // Start transport
    if (mcp_transport_start(server->transport) != 0) {
        set_error("Failed to start transport");
        return -1;
    }

    // Setup signal handling
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    server->running = 1;

    if (server->debug) {
        const char *transport_name = (transport == EMBED_MCP_TRANSPORT_STDIO) ? "STDIO" : "HTTP";
        if (transport == EMBED_MCP_TRANSPORT_HTTP) {
            mcp_log_info("%s Server '%s' v%s started on %s:%d",
                        transport_name, server->name, server->version, server->host, server->port);
        } else {
            mcp_log_info("%s Server '%s' v%s started",
                        transport_name, server->name, server->version);
        }
    }

    // Main loop
    while (g_running && server->running) {
        usleep(100000); // 100ms
    }

    // Stop transport
    mcp_transport_stop(server->transport);

    if (server->debug) {
        mcp_log_info("Server stopped");
    }

    return 0;
}

void embed_mcp_stop(embed_mcp_server_t *server) {
    if (server) {
        server->running = 0;
    }
}



int embed_mcp_quick_start(const char *name, const char *version,
                          embed_mcp_transport_t transport, int port) {
    embed_mcp_config_t config = {
        .name = name,
        .version = version,
        .host = "0.0.0.0",
        .port = port,
        .path = "/mcp",
        .max_tools = 100,
        .debug = 1
    };

    embed_mcp_server_t *server = embed_mcp_create(&config);
    if (!server) return -1;

    int result = embed_mcp_run(server, transport);
    embed_mcp_destroy(server);

    return result;
}

// Helper function to create JSON Schema from parameter descriptions
static cJSON *create_schema_from_params(mcp_param_desc_t *params, size_t param_count) {
    cJSON *schema = cJSON_CreateObject();

    cJSON_AddStringToObject(schema, "$schema", "http://json-schema.org/draft-07/schema#");
    cJSON_AddStringToObject(schema, "type", "object");
    cJSON_AddStringToObject(schema, "title", "Tool Parameters");
    cJSON_AddStringToObject(schema, "description", "Parameters for the tool");

    cJSON *properties = cJSON_CreateObject();
    cJSON *required = cJSON_CreateArray();

    for (size_t i = 0; i < param_count; i++) {
        cJSON *param_schema = cJSON_CreateObject();
        cJSON_AddStringToObject(param_schema, "description", params[i].description);

        switch (params[i].category) {
            case MCP_PARAM_SINGLE:
                switch (params[i].single_type) {
                    case MCP_PARAM_INT:
                        cJSON_AddStringToObject(param_schema, "type", "integer");
                        break;
                    case MCP_PARAM_DOUBLE:
                        cJSON_AddStringToObject(param_schema, "type", "number");
                        break;
                    case MCP_PARAM_STRING:
                        cJSON_AddStringToObject(param_schema, "type", "string");
                        break;
                    case MCP_PARAM_BOOL:
                        cJSON_AddStringToObject(param_schema, "type", "boolean");
                        break;
                    default:
                        cJSON_AddStringToObject(param_schema, "type", "string");
                        break;
                }
                break;

            case MCP_PARAM_ARRAY: {
                cJSON_AddStringToObject(param_schema, "type", "array");
                cJSON *items = cJSON_CreateObject();
                cJSON_AddStringToObject(items, "description", params[i].array_desc.element_description);

                switch (params[i].array_desc.element_type) {
                    case MCP_PARAM_INT:
                        cJSON_AddStringToObject(items, "type", "integer");
                        break;
                    case MCP_PARAM_DOUBLE:
                        cJSON_AddStringToObject(items, "type", "number");
                        break;
                    case MCP_PARAM_STRING:
                        cJSON_AddStringToObject(items, "type", "string");
                        break;
                    case MCP_PARAM_BOOL:
                        cJSON_AddStringToObject(items, "type", "boolean");
                        break;
                    default:
                        cJSON_AddStringToObject(items, "type", "string");
                        break;
                }
                cJSON_AddItemToObject(param_schema, "items", items);
                break;
            }

            case MCP_PARAM_OBJECT:
                if (params[i].object_schema) {
                    cJSON *object_schema = cJSON_Parse(params[i].object_schema);
                    if (object_schema) {
                        cJSON_Delete(param_schema);
                        param_schema = object_schema;
                    } else {
                        cJSON_AddStringToObject(param_schema, "type", "object");
                    }
                } else {
                    cJSON_AddStringToObject(param_schema, "type", "object");
                }
                break;
        }

        cJSON_AddItemToObject(properties, params[i].name, param_schema);

        if (params[i].required) {
            cJSON_AddItemToArray(required, cJSON_CreateString(params[i].name));
        }
    }

    cJSON_AddItemToObject(schema, "properties", properties);
    cJSON_AddItemToObject(schema, "required", required);
    cJSON_AddBoolToObject(schema, "additionalProperties", false);

    return schema;
}



// =============================================================================
// Unified Pure Function API Implementation
// =============================================================================

// Universal wrapper that handles different function signatures
static cJSON *universal_pure_wrapper(const cJSON *args, void *user_data) {
    // user_data contains both function pointer and metadata
    typedef struct {
        void *func_ptr;
        mcp_param_desc_t *params;
        size_t param_count;
        mcp_return_type_t return_type;
    } wrapper_context_t;

    wrapper_context_t *ctx = (wrapper_context_t*)user_data;

    // Handle different parameter patterns
    if (ctx->param_count == 2 &&
        ctx->params[0].category == MCP_PARAM_SINGLE && ctx->params[0].single_type == MCP_PARAM_DOUBLE &&
        ctx->params[1].category == MCP_PARAM_SINGLE && ctx->params[1].single_type == MCP_PARAM_DOUBLE &&
        ctx->return_type == MCP_RETURN_DOUBLE) {

        // Math function: double func(double a, double b)
        typedef double (*math_func_t)(double, double);
        math_func_t func = (math_func_t)ctx->func_ptr;

        cJSON *a_json = cJSON_GetObjectItem(args, ctx->params[0].name);
        cJSON *b_json = cJSON_GetObjectItem(args, ctx->params[1].name);

        if (!a_json || !b_json || !cJSON_IsNumber(a_json) || !cJSON_IsNumber(b_json)) {
            return NULL;
        }

        double result = func(a_json->valuedouble, b_json->valuedouble);

        cJSON *response = cJSON_CreateObject();
        cJSON *content = cJSON_CreateArray();
        cJSON *text_content = cJSON_CreateObject();
        cJSON_AddStringToObject(text_content, "type", "text");

        char result_text[256];
        snprintf(result_text, sizeof(result_text), "%.1f", result);
        cJSON_AddStringToObject(text_content, "text", result_text);

        cJSON_AddItemToArray(content, text_content);
        cJSON_AddItemToObject(response, "content", content);
        return response;
    }

    else if (ctx->param_count == 1 &&
             ctx->params[0].category == MCP_PARAM_SINGLE && ctx->params[0].single_type == MCP_PARAM_STRING &&
             ctx->return_type == MCP_RETURN_STRING) {

        // Text function: char* func(const char* input)
        typedef char* (*text_func_t)(const char*);
        text_func_t func = (text_func_t)ctx->func_ptr;

        cJSON *input_json = cJSON_GetObjectItem(args, ctx->params[0].name);
        if (!input_json || !cJSON_IsString(input_json)) {
            return NULL;
        }

        char *result = func(input_json->valuestring);
        if (!result) {
            return NULL;
        }

        cJSON *response = cJSON_CreateObject();
        cJSON *content = cJSON_CreateArray();
        cJSON *text_content = cJSON_CreateObject();
        cJSON_AddStringToObject(text_content, "type", "text");
        cJSON_AddStringToObject(text_content, "text", result);

        cJSON_AddItemToArray(content, text_content);
        cJSON_AddItemToObject(response, "content", content);

        free(result);
        return response;
    }

    else if (ctx->param_count == 1 &&
             ctx->params[0].category == MCP_PARAM_ARRAY && ctx->params[0].array_desc.element_type == MCP_PARAM_DOUBLE &&
             ctx->return_type == MCP_RETURN_DOUBLE) {

        // Array function: double func(double* array, size_t count)
        typedef double (*array_func_t)(double*, size_t);
        array_func_t func = (array_func_t)ctx->func_ptr;

        cJSON *array_json = cJSON_GetObjectItem(args, ctx->params[0].name);
        if (!array_json || !cJSON_IsArray(array_json)) {
            return NULL;
        }

        int count = cJSON_GetArraySize(array_json);
        if (count == 0) {
            return NULL;
        }

        double *numbers = malloc(count * sizeof(double));
        if (!numbers) {
            return NULL;
        }

        for (int i = 0; i < count; i++) {
            cJSON *item = cJSON_GetArrayItem(array_json, i);
            if (cJSON_IsNumber(item)) {
                numbers[i] = item->valuedouble;
            } else {
                free(numbers);
                return NULL;
            }
        }

        double result = func(numbers, count);
        free(numbers);

        cJSON *response = cJSON_CreateObject();
        cJSON *content = cJSON_CreateArray();
        cJSON *text_content = cJSON_CreateObject();
        cJSON_AddStringToObject(text_content, "type", "text");

        char result_text[256];
        snprintf(result_text, sizeof(result_text), "Sum of %d numbers: %.2f", count, result);
        cJSON_AddStringToObject(text_content, "text", result_text);

        cJSON_AddItemToArray(content, text_content);
        cJSON_AddItemToObject(response, "content", content);
        return response;
    }

    else if (ctx->param_count == 0 && ctx->return_type == MCP_RETURN_INT) {

        // Status function: int func(void)
        typedef int (*status_func_t)(void);
        status_func_t func = (status_func_t)ctx->func_ptr;

        int result = func();

        cJSON *response = cJSON_CreateObject();
        cJSON *content = cJSON_CreateArray();
        cJSON *text_content = cJSON_CreateObject();
        cJSON_AddStringToObject(text_content, "type", "text");

        char result_text[256];
        snprintf(result_text, sizeof(result_text), "%d", result);
        cJSON_AddStringToObject(text_content, "text", result_text);

        cJSON_AddItemToArray(content, text_content);
        cJSON_AddItemToObject(response, "content", content);
        return response;
    }

    // Unsupported function signature
    return NULL;
}

/*
 * COMMENTED OUT: Pure Function API Implementation
 *
 * This implementation is kept for reference but commented out in favor of
 * the more flexible embed_mcp_add_tool API.
 */
/*
int embed_mcp_add_pure_function(embed_mcp_server_t *server,
                                const char *name,
                                const char *description,
                                mcp_param_desc_t *params,
                                size_t param_count,
                                mcp_return_type_t return_type,
                                void *function_ptr) {
    if (!server || !name || !description || !function_ptr) {
        set_error("Invalid parameters");
        return -1;
    }

    // Create wrapper context
    typedef struct {
        void *func_ptr;
        mcp_param_desc_t *params;
        size_t param_count;
        mcp_return_type_t return_type;
    } wrapper_context_t;

    wrapper_context_t *ctx = malloc(sizeof(wrapper_context_t));
    if (!ctx) {
        set_error("Memory allocation failed");
        return -1;
    }

    ctx->func_ptr = function_ptr;
    ctx->param_count = param_count;
    ctx->return_type = return_type;

    // Copy parameters
    if (param_count > 0) {
        ctx->params = malloc(param_count * sizeof(mcp_param_desc_t));
        if (!ctx->params) {
            free(ctx);
            set_error("Memory allocation failed");
            return -1;
        }
        memcpy(ctx->params, params, param_count * sizeof(mcp_param_desc_t));
    } else {
        ctx->params = NULL;
    }

    // Create input schema
    cJSON *input_schema = NULL;
    if (param_count > 0) {
        input_schema = create_schema_from_params(params, param_count);
        if (!input_schema) {
            free(ctx->params);
            free(ctx);
            set_error("Failed to create input schema");
            return -1;
        }
    } else {
        // No parameters - create empty schema
        input_schema = cJSON_CreateObject();
        cJSON_AddStringToObject(input_schema, "$schema", "http://json-schema.org/draft-07/schema#");
        cJSON_AddStringToObject(input_schema, "type", "object");
        cJSON_AddItemToObject(input_schema, "properties", cJSON_CreateObject());
        cJSON_AddItemToObject(input_schema, "required", cJSON_CreateArray());
        cJSON_AddBoolToObject(input_schema, "additionalProperties", false);
    }

    // Create tool with universal wrapper
    mcp_tool_t *tool = mcp_tool_create(name, name, description, input_schema,
                                      (mcp_tool_execute_func_t)universal_pure_wrapper, ctx);

    cJSON_Delete(input_schema);

    if (!tool) {
        free(ctx->params);
        free(ctx);
        set_error("Failed to create tool");
        return -1;
    }

    // Register tool
    if (mcp_tool_registry_register_tool(server->tool_registry, tool) != 0) {
        mcp_tool_destroy(tool);
        free(ctx->params);
        free(ctx);
        set_error("Failed to register tool");
        return -1;
    }

    return 0;
}
*/



const char *embed_mcp_get_error(void) {
    return g_error_message[0] ? g_error_message : "No error";
}
