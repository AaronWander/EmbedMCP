#include "embed_mcp.h"
#include "protocol/mcp_protocol.h"
#include "transport/transport_interface.h"
#include "tools/tool_registry.h"
#include "tools/tool_interface.h"
#include "application/session_manager.h"
#include "hal/platform_hal.h"
#include "hal/hal_common.h"
#include "utils/logging.h"
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

// Global error message
static char g_error_message[512] = {0};
static volatile int g_running = 1;

// HAL helper functions are now in hal_common.h/c

// Server structure
struct embed_mcp_server {
    char *name;
    char *version;
    char *host;
    int port;
    char *path;
    int debug;

    // Multi-session support
    int max_connections;
    int session_timeout;
    int enable_sessions;
    int auto_cleanup;

    mcp_protocol_t *protocol;
    mcp_transport_t *transport;
    mcp_tool_registry_t *tool_registry;
    mcp_session_manager_t *session_manager;
    mcp_connection_t *current_connection;  // For backward compatibility

    int running;
};

// Parameter accessor implementation
typedef struct {
    const cJSON* args;  // JSON arguments from MCP call
} param_accessor_data_t;

// Parameter accessor function implementations
static int64_t param_get_int(mcp_param_accessor_t* self, const char* name) {
    param_accessor_data_t* data = (param_accessor_data_t*)self->data;
    const cJSON* item = cJSON_GetObjectItem(data->args, name);
    if (!item || !cJSON_IsNumber(item)) {
        return 0;  // Default value for missing/invalid parameters
    }
    return (int64_t)cJSON_GetNumberValue(item);
}

static double param_get_double(mcp_param_accessor_t* self, const char* name) {
    param_accessor_data_t* data = (param_accessor_data_t*)self->data;
    const cJSON* item = cJSON_GetObjectItem(data->args, name);
    if (!item || !cJSON_IsNumber(item)) {
        return 0.0;  // Default value for missing/invalid parameters
    }
    return cJSON_GetNumberValue(item);
}

static const char* param_get_string(mcp_param_accessor_t* self, const char* name) {
    param_accessor_data_t* data = (param_accessor_data_t*)self->data;
    const cJSON* item = cJSON_GetObjectItem(data->args, name);
    if (!item || !cJSON_IsString(item)) {
        return "";  // Default value for missing/invalid parameters
    }
    return cJSON_GetStringValue(item);
}

static int param_get_bool(mcp_param_accessor_t* self, const char* name) {
    param_accessor_data_t* data = (param_accessor_data_t*)self->data;
    const cJSON* item = cJSON_GetObjectItem(data->args, name);
    if (!item || !cJSON_IsBool(item)) {
        return 0;  // Default value for missing/invalid parameters
    }
    return cJSON_IsTrue(item) ? 1 : 0;
}

static double* param_get_double_array(mcp_param_accessor_t* self, const char* name, size_t* count) {
    param_accessor_data_t* data = (param_accessor_data_t*)self->data;
    const cJSON* item = cJSON_GetObjectItem(data->args, name);
    if (!item || !cJSON_IsArray(item)) {
        *count = 0;
        return NULL;
    }

    int array_size = cJSON_GetArraySize(item);
    if (array_size <= 0) {
        *count = 0;
        return NULL;
    }

    double* result = malloc(array_size * sizeof(double));
    if (!result) {
        *count = 0;
        return NULL;
    }

    *count = array_size;
    for (int i = 0; i < array_size; i++) {
        const cJSON* element = cJSON_GetArrayItem(item, i);
        if (cJSON_IsNumber(element)) {
            result[i] = cJSON_GetNumberValue(element);
        } else {
            result[i] = 0.0;  // Default for invalid elements
        }
    }

    return result;
}

static char** param_get_string_array(mcp_param_accessor_t* self, const char* name, size_t* count) {
    param_accessor_data_t* data = (param_accessor_data_t*)self->data;
    const cJSON* item = cJSON_GetObjectItem(data->args, name);
    if (!item || !cJSON_IsArray(item)) {
        *count = 0;
        return NULL;
    }

    int array_size = cJSON_GetArraySize(item);
    if (array_size <= 0) {
        *count = 0;
        return NULL;
    }

    char** result = malloc(array_size * sizeof(char*));
    if (!result) {
        *count = 0;
        return NULL;
    }

    *count = array_size;
    for (int i = 0; i < array_size; i++) {
        const cJSON* element = cJSON_GetArrayItem(item, i);
        if (cJSON_IsString(element)) {
            result[i] = strdup(cJSON_GetStringValue(element));
        } else {
            result[i] = strdup("");  // Default for invalid elements
        }
    }

    return result;
}

static int64_t* param_get_int_array(mcp_param_accessor_t* self, const char* name, size_t* count) {
    param_accessor_data_t* data = (param_accessor_data_t*)self->data;
    const cJSON* item = cJSON_GetObjectItem(data->args, name);
    if (!item || !cJSON_IsArray(item)) {
        *count = 0;
        return NULL;
    }

    int array_size = cJSON_GetArraySize(item);
    if (array_size <= 0) {
        *count = 0;
        return NULL;
    }

    int64_t* result = malloc(array_size * sizeof(int64_t));
    if (!result) {
        *count = 0;
        return NULL;
    }

    *count = array_size;
    for (int i = 0; i < array_size; i++) {
        const cJSON* element = cJSON_GetArrayItem(item, i);
        if (cJSON_IsNumber(element)) {
            result[i] = (int64_t)cJSON_GetNumberValue(element);
        } else {
            result[i] = 0;  // Default for invalid elements
        }
    }

    return result;
}

static int param_has_param(mcp_param_accessor_t* self, const char* name) {
    param_accessor_data_t* data = (param_accessor_data_t*)self->data;
    return cJSON_HasObjectItem(data->args, name) ? 1 : 0;
}

static size_t param_get_param_count(mcp_param_accessor_t* self) {
    param_accessor_data_t* data = (param_accessor_data_t*)self->data;
    if (!cJSON_IsObject(data->args)) {
        return 0;
    }
    return (size_t)cJSON_GetArraySize(data->args);
}

static const cJSON* param_get_json(mcp_param_accessor_t* self, const char* name) {
    param_accessor_data_t* data = (param_accessor_data_t*)self->data;
    return cJSON_GetObjectItem(data->args, name);
}

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

    // Initialize platform HAL
    if (mcp_platform_init() != 0) {
        set_error("Platform initialization failed");
        return NULL;
    }

    const mcp_platform_hal_t *hal;
    hal_result_t hal_result = hal_safe_get(&hal);
    if (hal_result != HAL_OK) {
        set_error(hal_get_error_string(hal_result));
        return NULL;
    }

    // Use HAL memory allocation
    embed_mcp_server_t *server;
    hal_result = hal_safe_alloc(hal, sizeof(embed_mcp_server_t), (void**)&server);
    if (hal_result != HAL_OK) {
        set_error(hal_get_error_string(hal_result));
        return NULL;
    }
    memset(server, 0, sizeof(embed_mcp_server_t));
    
    // Copy configuration using HAL memory allocation
    server->name = hal_strdup(hal, config->name);
    server->version = hal_strdup(hal, config->version);
    server->host = hal_strdup(hal, config->host ? config->host : "0.0.0.0");
    server->path = hal_strdup(hal, config->path ? config->path : "/mcp");

    // Check if string allocation succeeded
    if (!server->name || !server->version || !server->host || !server->path) {
        // Cleanup on failure using unified hal_free
        hal_free(hal, server->name);
        hal_free(hal, server->version);
        hal_free(hal, server->host);
        hal_free(hal, server->path);
        hal_free(hal, server);
        set_error("String allocation failed");
        return NULL;
    }

    server->port = config->port > 0 ? config->port : 8080;
    server->debug = config->debug;

    // Multi-session configuration
    server->max_connections = config->max_connections > 0 ? config->max_connections : 10;
    server->session_timeout = config->session_timeout > 0 ? config->session_timeout : 3600;
    server->enable_sessions = config->enable_sessions != 0 ? config->enable_sessions : 1;
    server->auto_cleanup = config->auto_cleanup != 0 ? config->auto_cleanup : 1;

    // This check was moved earlier in the function
    
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

    // Create session manager if enabled
    if (server->enable_sessions) {
        mcp_session_manager_config_t *session_config = mcp_session_manager_config_create_default();
        if (session_config) {
            session_config->max_sessions = server->max_connections;
            session_config->default_session_timeout = server->session_timeout;
            session_config->auto_cleanup = server->auto_cleanup;

            server->session_manager = mcp_session_manager_create(session_config);
            mcp_session_manager_config_destroy(session_config);

            if (!server->session_manager) {
                embed_mcp_destroy(server);
                set_error("Failed to create session manager");
                return NULL;
            }
        }
    }

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

    // Get HAL for memory deallocation
    const mcp_platform_hal_t *hal = mcp_platform_get_hal();

    if (server->transport) {
        mcp_transport_destroy(server->transport);
    }

    if (server->protocol) {
        mcp_protocol_destroy(server->protocol);
    }

    if (server->tool_registry) {
        mcp_tool_registry_destroy(server->tool_registry);
    }

    if (server->session_manager) {
        mcp_session_manager_destroy(server->session_manager);
    }

    // Use HAL memory deallocation
    hal_free(hal, server->name);
    hal_free(hal, server->version);
    hal_free(hal, server->host);
    hal_free(hal, server->path);
    hal_free(hal, server);

    // Cleanup platform HAL
    mcp_platform_cleanup();
}



// Removed: Helper functions for old convenience APIs
// create_math_tool_schema() and create_text_tool_schema() were used by
// the removed embed_mcp_add_math_tool and embed_mcp_add_text_tool functions

// Removed: embed_mcp_add_math_tool and embed_mcp_add_text_tool
// These convenience functions were replaced by the unified pure function API

/*
 * COMMENTED OUT: Universal Function Wrapper
 *
 * This wrapper was used by the commented embed_mcp_add_pure_function API.
 * Kept for reference but commented out to avoid unused function warnings.
 */
/*
// Universal function wrapper - converts universal functions to JSON responses
static cJSON *universal_function_wrapper(const cJSON *args, void *user_data) {
    typedef struct {
        mcp_universal_func_t func;
        mcp_return_type_t return_type;
    } universal_func_data_t;

    universal_func_data_t* func_data = (universal_func_data_t*)user_data;

    // Create parameter accessor
    param_accessor_data_t accessor_data = { .args = args };
    mcp_param_accessor_t accessor = {
        .get_int = param_get_int,
        .get_double = param_get_double,
        .get_string = param_get_string,
        .get_bool = param_get_bool,
        .get_double_array = param_get_double_array,
        .get_string_array = param_get_string_array,
        .get_int_array = param_get_int_array,
        .has_param = param_has_param,
        .get_param_count = param_get_param_count,
        .get_json = param_get_json,
        .data = &accessor_data
    };

    // Call the universal function
    void* result = func_data->func(&accessor);

    // Convert result to JSON based on return type
    cJSON* json_result = cJSON_CreateObject();
    if (!json_result) {
        if (result) free(result);
        return NULL;
    }

    switch (func_data->return_type) {
        case MCP_RETURN_DOUBLE: {
            if (result) {
                double* double_result = (double*)result;
                cJSON_AddNumberToObject(json_result, "content", *double_result);
                free(result);
            } else {
                cJSON_AddNumberToObject(json_result, "content", 0.0);
            }
            break;
        }
        case MCP_RETURN_INT: {
            if (result) {
                int64_t* int_result = (int64_t*)result;
                cJSON_AddNumberToObject(json_result, "content", (double)*int_result);
                free(result);
            } else {
                cJSON_AddNumberToObject(json_result, "content", 0);
            }
            break;
        }
        case MCP_RETURN_STRING: {
            if (result) {
                char* string_result = (char*)result;
                cJSON_AddStringToObject(json_result, "content", string_result);
                free(result);
            } else {
                cJSON_AddStringToObject(json_result, "content", "");
            }
            break;
        }
        case MCP_RETURN_VOID:
            cJSON_AddStringToObject(json_result, "content", "Operation completed successfully");
            if (result) free(result);
            break;
        default:
            cJSON_AddStringToObject(json_result, "content", "Unknown return type");
            if (result) free(result);
            break;
    }

    return json_result;
}
*/

/*
 * COMMENTED OUT: Tool Handler Wrapper
 *
 * This wrapper was used by the commented embed_mcp_add_tool_with_schema API.
 * Kept for reference but commented out to avoid unused function warnings.
 */
/*
// Wrapper function to adapt our handler signature to the expected one (legacy)
static cJSON *tool_handler_wrapper(const cJSON *args, void *user_data) {
    embed_mcp_tool_handler_t handler = (embed_mcp_tool_handler_t)user_data;
    return handler(args);
}
*/

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

    // Start session manager if enabled
    if (server->session_manager) {
        if (mcp_session_manager_start(server->session_manager) != 0) {
            mcp_transport_destroy(server->transport);
            server->transport = NULL;
            set_error("Failed to start session manager");
            return -1;
        }
    }

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

    // Stop session manager if enabled
    if (server->session_manager) {
        mcp_session_manager_stop(server->session_manager);
    }

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

/*
 * COMMENTED OUT: Universal Pure Wrapper
 *
 * This wrapper was used by the commented embed_mcp_add_pure_function API.
 * Kept for reference but commented out to avoid unused function warnings.
 */
/*
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
*/

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
                                mcp_universal_func_t function_ptr) {
    if (!server || !name || !description || !function_ptr) {
        set_error("Invalid parameters");
        return -1;
    }

    // Create universal function data
    typedef struct {
        mcp_universal_func_t func;
        mcp_return_type_t return_type;
    } universal_func_data_t;

    universal_func_data_t *func_data = malloc(sizeof(universal_func_data_t));
    if (!func_data) {
        set_error("Memory allocation failed");
        return -1;
    }

    func_data->func = function_ptr;
    func_data->return_type = return_type;

    // Create input schema
    cJSON *input_schema = NULL;
    if (param_count > 0) {
        input_schema = create_schema_from_params(params, param_count);
        if (!input_schema) {
            free(func_data);
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
                                      universal_function_wrapper, func_data);

    cJSON_Delete(input_schema);

    if (!tool) {
        free(func_data);
        set_error("Failed to create tool");
        return -1;
    }

    // Register tool
    if (mcp_tool_registry_register_tool(server->tool_registry, tool) != 0) {
        mcp_tool_destroy(tool);
        free(func_data);
        set_error("Failed to register tool");
        return -1;
    }

    return 0;
}
*/



const char *embed_mcp_get_error(void) {
    return g_error_message[0] ? g_error_message : "No error";
}

// Simple function wrappers for different signatures
/*
 * COMMENTED OUT: Simple Function Type Wrappers
 *
 * These wrappers were used by the commented simple_function_wrapper.
 * Kept for reference but commented out to avoid unused function warnings.
 */
/*
static void* string_string_wrapper(mcp_param_accessor_t* params, void* func_ptr) {
    typedef char* (*func_t)(const char*);
    func_t func = (func_t)func_ptr;

    const char* input = params->get_string(params, "input");
    return func(input);
}

static void* int_int_int_wrapper(mcp_param_accessor_t* params, void* func_ptr) {
    typedef int (*func_t)(int, int);
    func_t func = (func_t)func_ptr;

    int a = (int)params->get_int(params, "a");
    int b = (int)params->get_int(params, "b");
    int result = func(a, b);

    int* ret = malloc(sizeof(int));
    if (!ret) return NULL;
    *ret = result;
    return ret;
}

static void* double_double_double_wrapper(mcp_param_accessor_t* params, void* func_ptr) {
    typedef double (*func_t)(double, double);
    func_t func = (func_t)func_ptr;

    double a = params->get_double(params, "a");
    double b = params->get_double(params, "b");
    double result = func(a, b);

    double* ret = malloc(sizeof(double));
    if (!ret) return NULL;
    *ret = result;
    return ret;
}

static void* double_array_size_wrapper(mcp_param_accessor_t* params, void* func_ptr) {
    typedef double (*func_t)(double*, size_t);
    func_t func = (func_t)func_ptr;

    size_t count;
    double* array = params->get_double_array(params, "numbers", &count);
    double result = func(array, count);

    if (array) free(array);  // Clean up array from get_double_array

    double* ret = malloc(sizeof(double));
    if (!ret) return NULL;
    *ret = result;
    return ret;
}
*/

/*
 * COMMENTED OUT: Simple Function Wrapper
 *
 * This wrapper was used by the commented embed_mcp_add_simple_func API.
 * Kept for reference but commented out to avoid unused function warnings.
 */
/*
// Simple function wrapper that calls the appropriate wrapper based on signature
typedef struct {
    mcp_func_signature_t signature;
    void* original_func;
} simple_func_data_t;

static cJSON* simple_function_wrapper(const cJSON *args, void *user_data) {
    simple_func_data_t* data = (simple_func_data_t*)user_data;

    // Create parameter accessor
    param_accessor_data_t accessor_data = { .args = args };
    mcp_param_accessor_t accessor = {
        .get_int = param_get_int,
        .get_double = param_get_double,
        .get_string = param_get_string,
        .get_bool = param_get_bool,
        .get_double_array = param_get_double_array,
        .get_string_array = param_get_string_array,
        .get_int_array = param_get_int_array,
        .has_param = param_has_param,
        .get_param_count = param_get_param_count,
        .get_json = param_get_json,
        .data = &accessor_data
    };

    void* result = NULL;

    // Call appropriate wrapper based on signature
    switch (data->signature) {
        case MCP_FUNC_STRING_STRING:
            result = string_string_wrapper(&accessor, data->original_func);
            break;
        case MCP_FUNC_INT_INT_INT:
            result = int_int_int_wrapper(&accessor, data->original_func);
            break;
        case MCP_FUNC_DOUBLE_DOUBLE_DOUBLE:
            result = double_double_double_wrapper(&accessor, data->original_func);
            break;
        case MCP_FUNC_DOUBLE_ARRAY_SIZE:
            result = double_array_size_wrapper(&accessor, data->original_func);
            break;
        default:
            return NULL;
    }

    // Convert result to JSON
    cJSON* json_result = cJSON_CreateObject();
    if (!json_result) {
        if (result) free(result);
        return NULL;
    }

    switch (data->signature) {
        case MCP_FUNC_STRING_STRING:
            if (result) {
                cJSON_AddStringToObject(json_result, "content", (char*)result);
                free(result);
            } else {
                cJSON_AddStringToObject(json_result, "content", "");
            }
            break;
        case MCP_FUNC_INT_INT_INT:
            if (result) {
                cJSON_AddNumberToObject(json_result, "content", *(int*)result);
                free(result);
            } else {
                cJSON_AddNumberToObject(json_result, "content", 0);
            }
            break;
        case MCP_FUNC_DOUBLE_DOUBLE_DOUBLE:
        case MCP_FUNC_DOUBLE_ARRAY_SIZE:
            if (result) {
                cJSON_AddNumberToObject(json_result, "content", *(double*)result);
                free(result);
            } else {
                cJSON_AddNumberToObject(json_result, "content", 0.0);
            }
            break;
        default:
            cJSON_AddStringToObject(json_result, "content", "Unknown result type");
            if (result) free(result);
            break;
    }

    return json_result;
}
*/

/*
 * COMMENTED OUT: Simple Function API Implementation
 *
 * This implementation is kept for reference but commented out in favor of
 * the more flexible embed_mcp_add_tool API.
 */
/*
int embed_mcp_add_simple_func(embed_mcp_server_t *server,
                              const char *name,
                              const char *description,
                              mcp_func_signature_t signature,
                              void *function_ptr) {
    if (!server || !name || !description || !function_ptr) {
        set_error("Invalid parameters");
        return -1;
    }

    // Create parameter descriptions based on signature
    mcp_param_desc_t *params = NULL;
    size_t param_count = 0;
    mcp_return_type_t return_type;

    switch (signature) {
        case MCP_FUNC_STRING_STRING:
            param_count = 1;
            params = malloc(sizeof(mcp_param_desc_t));
            params[0] = (mcp_param_desc_t)MCP_PARAM_STRING_DEF("input", "Input string", 1);
            return_type = MCP_RETURN_STRING;
            break;

        case MCP_FUNC_INT_INT_INT:
            param_count = 2;
            params = malloc(2 * sizeof(mcp_param_desc_t));
            params[0] = (mcp_param_desc_t)MCP_PARAM_INT_DEF("a", "First integer", 1);
            params[1] = (mcp_param_desc_t)MCP_PARAM_INT_DEF("b", "Second integer", 1);
            return_type = MCP_RETURN_INT;
            break;

        case MCP_FUNC_DOUBLE_DOUBLE_DOUBLE:
            param_count = 2;
            params = malloc(2 * sizeof(mcp_param_desc_t));
            params[0] = (mcp_param_desc_t)MCP_PARAM_DOUBLE_DEF("a", "First number", 1);
            params[1] = (mcp_param_desc_t)MCP_PARAM_DOUBLE_DEF("b", "Second number", 1);
            return_type = MCP_RETURN_DOUBLE;
            break;

        case MCP_FUNC_DOUBLE_ARRAY_SIZE:
            param_count = 1;
            params = malloc(sizeof(mcp_param_desc_t));
            params[0] = (mcp_param_desc_t)MCP_PARAM_ARRAY_DOUBLE_DEF("numbers", "Array of numbers", "A number", 1);
            return_type = MCP_RETURN_DOUBLE;
            break;

        default:
            set_error("Unsupported function signature");
            return -1;
    }

    // Create wrapper data
    simple_func_data_t *wrapper_data = malloc(sizeof(simple_func_data_t));
    if (!wrapper_data) {
        free(params);
        set_error("Memory allocation failed");
        return -1;
    }
    wrapper_data->signature = signature;
    wrapper_data->original_func = function_ptr;

    // Create input schema
    cJSON *input_schema = NULL;
    if (param_count > 0) {
        input_schema = create_schema_from_params(params, param_count);
        if (!input_schema) {
            free(params);
            free(wrapper_data);
            set_error("Failed to create input schema");
            return -1;
        }
    } else {
        input_schema = cJSON_CreateObject();
        cJSON_AddStringToObject(input_schema, "$schema", "http://json-schema.org/draft-07/schema#");
        cJSON_AddStringToObject(input_schema, "type", "object");
        cJSON_AddItemToObject(input_schema, "properties", cJSON_CreateObject());
        cJSON_AddItemToObject(input_schema, "required", cJSON_CreateArray());
        cJSON_AddBoolToObject(input_schema, "additionalProperties", false);
    }

    // Create tool with simple function wrapper
    mcp_tool_t *tool = mcp_tool_create(name, name, description, input_schema,
                                      simple_function_wrapper, wrapper_data);

    cJSON_Delete(input_schema);
    free(params);

    if (!tool) {
        free(wrapper_data);
        set_error("Failed to create tool");
        return -1;
    }

    // Register tool
    if (mcp_tool_registry_register_tool(server->tool_registry, tool) != 0) {
        mcp_tool_destroy(tool);
        free(wrapper_data);
        set_error("Failed to register tool");
        return -1;
    }

    return 0;
}
*/

// Custom function wrapper for arbitrary parameter combinations
typedef struct {
    void* original_func;
    const char** param_names;
    mcp_param_type_t* param_types;
    size_t param_count;
    mcp_return_type_t return_type;
} custom_func_data_t;

static cJSON* custom_function_wrapper(const cJSON *args, void *user_data) {
    custom_func_data_t* data = (custom_func_data_t*)user_data;

    // Create parameter accessor
    param_accessor_data_t accessor_data = { .args = args };
    mcp_param_accessor_t accessor = {
        .get_int = param_get_int,
        .get_double = param_get_double,
        .get_string = param_get_string,
        .get_bool = param_get_bool,
        .get_double_array = param_get_double_array,
        .get_string_array = param_get_string_array,
        .get_int_array = param_get_int_array,
        .has_param = param_has_param,
        .get_param_count = param_get_param_count,
        .get_json = param_get_json,
        .data = &accessor_data
    };

    // Extract parameters based on types
    void* params[16];  // Support up to 16 parameters
    if (data->param_count > 16) {
        return NULL;  // Too many parameters
    }

    for (size_t i = 0; i < data->param_count; i++) {
        switch (data->param_types[i]) {
            case MCP_PARAM_INT: {
                int* val = malloc(sizeof(int));
                *val = (int)accessor.get_int(&accessor, data->param_names[i]);
                params[i] = val;
                break;
            }
            case MCP_PARAM_DOUBLE: {
                double* val = malloc(sizeof(double));
                *val = accessor.get_double(&accessor, data->param_names[i]);
                params[i] = val;
                break;
            }
            case MCP_PARAM_STRING: {
                const char* str = accessor.get_string(&accessor, data->param_names[i]);
                params[i] = (void*)str;  // Don't malloc, it's const
                break;
            }
            case MCP_PARAM_CHAR: {
                const char* str = accessor.get_string(&accessor, data->param_names[i]);
                char* val = malloc(sizeof(char));
                *val = str[0];  // Take first character
                params[i] = val;
                break;
            }
            case MCP_PARAM_BOOL: {
                int* val = malloc(sizeof(int));
                *val = accessor.get_bool(&accessor, data->param_names[i]);
                params[i] = val;
                break;
            }
            default:
                params[i] = NULL;
                break;
        }
    }

    // Call function based on parameter count and types
    void* result = NULL;

    // Handle different function signatures
    if (data->param_count == 2 &&
        data->param_types[0] == MCP_PARAM_DOUBLE &&
        data->param_types[1] == MCP_PARAM_DOUBLE) {
        // double func(double, double)
        typedef double (*func_t)(double, double);
        func_t func = (func_t)data->original_func;

        double a = *(double*)params[0];
        double b = *(double*)params[1];

        double func_result = func(a, b);
        double* ret = malloc(sizeof(double));
        *ret = func_result;
        result = ret;

    } else if (data->param_count == 1 &&
               data->param_types[0] == MCP_PARAM_STRING) {
        // char* func(const char*)
        typedef char* (*func_t)(const char*);
        func_t func = (func_t)data->original_func;

        const char* input = (const char*)params[0];
        result = func(input);  // Function returns malloc'd string

    } else if (data->param_count == 3 &&
               data->param_types[0] == MCP_PARAM_INT &&
               data->param_types[1] == MCP_PARAM_CHAR &&
               data->param_types[2] == MCP_PARAM_DOUBLE) {
        // int func(int, char, double)
        typedef int (*func_t)(int, char, double);
        func_t func = (func_t)data->original_func;

        int base_points = *(int*)params[0];
        char grade = *(char*)params[1];
        double multiplier = *(double*)params[2];

        int func_result = func(base_points, grade, multiplier);
        int* ret = malloc(sizeof(int));
        *ret = func_result;
        result = ret;

    } else if (data->param_count == 4 &&
               data->param_types[0] == MCP_PARAM_CHAR &&
               data->param_types[1] == MCP_PARAM_INT &&
               data->param_types[2] == MCP_PARAM_INT &&
               data->param_types[3] == MCP_PARAM_CHAR) {
        // int func(char, int, int, char) - original example
        typedef int (*func_t)(char, int, int, char);
        func_t func = (func_t)data->original_func;

        char c = *(char*)params[0];
        int a = *(int*)params[1];
        int b = *(int*)params[2];
        char d = *(char*)params[3];

        int func_result = func(c, a, b, d);
        int* ret = malloc(sizeof(int));
        *ret = func_result;
        result = ret;
    }

    // Clean up parameter memory
    for (size_t i = 0; i < data->param_count; i++) {
        if (data->param_types[i] != MCP_PARAM_STRING && params[i]) {
            free(params[i]);
        }
    }

    // Convert result to MCP-compliant JSON format
    cJSON* json_result = cJSON_CreateObject();
    if (!json_result) {
        if (result) free(result);
        return NULL;
    }

    // Create content array according to MCP spec
    cJSON* content = cJSON_CreateArray();
    if (!content) {
        cJSON_Delete(json_result);
        if (result) free(result);
        return NULL;
    }

    // Create text content block
    cJSON* text_block = cJSON_CreateObject();
    if (!text_block) {
        cJSON_Delete(content);
        cJSON_Delete(json_result);
        if (result) free(result);
        return NULL;
    }

    cJSON_AddStringToObject(text_block, "type", "text");

    // Convert result to text based on type
    char text_buffer[256];
    switch (data->return_type) {
        case MCP_RETURN_INT:
            if (result) {
                snprintf(text_buffer, sizeof(text_buffer), "%d", *(int*)result);
                free(result);
            } else {
                strcpy(text_buffer, "0");
            }
            break;
        case MCP_RETURN_DOUBLE:
            if (result) {
                snprintf(text_buffer, sizeof(text_buffer), "%.6g", *(double*)result);
                free(result);
            } else {
                strcpy(text_buffer, "0.0");
            }
            break;
        case MCP_RETURN_STRING:
            if (result) {
                strncpy(text_buffer, (char*)result, sizeof(text_buffer) - 1);
                text_buffer[sizeof(text_buffer) - 1] = '\0';
                free(result);
            } else {
                strcpy(text_buffer, "");
            }
            break;
        case MCP_RETURN_VOID:
            strcpy(text_buffer, "Operation completed");
            if (result) free(result);
            break;
        default:
            strcpy(text_buffer, "Unknown result type");
            if (result) free(result);
            break;
    }

    cJSON_AddStringToObject(text_block, "text", text_buffer);
    cJSON_AddItemToArray(content, text_block);
    cJSON_AddItemToObject(json_result, "content", content);

    return json_result;
}

int embed_mcp_add_tool(embed_mcp_server_t *server,
                       const char *name,
                       const char *description,
                       const char *param_names[],
                       mcp_param_type_t param_types[],
                       size_t param_count,
                       mcp_return_type_t return_type,
                       void *function_ptr) {
    if (!server || !name || !description || !function_ptr) {
        set_error("Invalid parameters");
        return -1;
    }

    if (param_count > 16) {
        set_error("Too many parameters (max 16)");
        return -1;
    }

    // Create custom function data
    custom_func_data_t *func_data = malloc(sizeof(custom_func_data_t));
    if (!func_data) {
        set_error("Memory allocation failed");
        return -1;
    }

    func_data->original_func = function_ptr;
    func_data->param_count = param_count;
    func_data->return_type = return_type;

    // Copy parameter names
    func_data->param_names = malloc(param_count * sizeof(char*));
    if (!func_data->param_names) {
        free(func_data);
        set_error("Memory allocation failed");
        return -1;
    }

    for (size_t i = 0; i < param_count; i++) {
        func_data->param_names[i] = strdup(param_names[i]);
    }

    // Copy parameter types
    func_data->param_types = malloc(param_count * sizeof(mcp_param_type_t));
    if (!func_data->param_types) {
        for (size_t i = 0; i < param_count; i++) {
            free((void*)func_data->param_names[i]);
        }
        free(func_data->param_names);
        free(func_data);
        set_error("Memory allocation failed");
        return -1;
    }

    memcpy(func_data->param_types, param_types, param_count * sizeof(mcp_param_type_t));

    // Create parameter descriptions
    mcp_param_desc_t *params = malloc(param_count * sizeof(mcp_param_desc_t));
    if (!params) {
        // Clean up
        for (size_t i = 0; i < param_count; i++) {
            free((void*)func_data->param_names[i]);
        }
        free(func_data->param_names);
        free(func_data->param_types);
        free(func_data);
        set_error("Memory allocation failed");
        return -1;
    }

    for (size_t i = 0; i < param_count; i++) {
        params[i].name = func_data->param_names[i];
        params[i].description = "Parameter";  // Generic description
        params[i].category = MCP_PARAM_SINGLE;
        params[i].required = 1;
        params[i].single_type = param_types[i];
    }

    // Create input schema
    cJSON *input_schema = create_schema_from_params(params, param_count);
    if (!input_schema) {
        // Clean up
        free(params);
        for (size_t i = 0; i < param_count; i++) {
            free((void*)func_data->param_names[i]);
        }
        free(func_data->param_names);
        free(func_data->param_types);
        free(func_data);
        set_error("Failed to create input schema");
        return -1;
    }

    // Create tool
    mcp_tool_t *tool = mcp_tool_create(name, name, description, input_schema,
                                      custom_function_wrapper, func_data);

    cJSON_Delete(input_schema);
    free(params);

    if (!tool) {
        // Clean up
        for (size_t i = 0; i < param_count; i++) {
            free((void*)func_data->param_names[i]);
        }
        free(func_data->param_names);
        free(func_data->param_types);
        free(func_data);
        set_error("Failed to create tool");
        return -1;
    }

    // Register tool
    if (mcp_tool_registry_register_tool(server->tool_registry, tool) != 0) {
        mcp_tool_destroy(tool);
        // Clean up
        for (size_t i = 0; i < param_count; i++) {
            free((void*)func_data->param_names[i]);
        }
        free(func_data->param_names);
        free(func_data->param_types);
        free(func_data);
        set_error("Failed to register tool");
        return -1;
    }

    return 0;
}
