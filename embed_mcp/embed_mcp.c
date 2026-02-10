#include "embed_mcp.h"
#include "protocol/mcp_protocol.h"
#include "transport/transport_interface.h"
#include "tools/tool_registry.h"
#include "tools/tool_interface.h"
#include "tools/resource_registry.h"
#include "application/session_manager.h"
#include "hal/platform_hal.h"
#include "hal/hal_common.h"
#include "utils/logging.h"
#include "utils/error_codes.h"
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
    mcp_resource_registry_t *resource_registry;
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

static double* param_get_double_array(mcp_param_accessor_t* self, const char* name, size_t* count);
static char** param_get_string_array(mcp_param_accessor_t* self, const char* name, size_t* count);
static int64_t* param_get_int_array(mcp_param_accessor_t* self, const char* name, size_t* count);

static int param_try_get_int(mcp_param_accessor_t* self, const char* name, int64_t* out) {
    param_accessor_data_t* data = (param_accessor_data_t*)self->data;
    const cJSON* item = cJSON_GetObjectItem(data->args, name);
    if (!item || !cJSON_IsNumber(item) || !out) {
        return 0;
    }
    *out = (int64_t)cJSON_GetNumberValue(item);
    return 1;
}

static int param_try_get_double(mcp_param_accessor_t* self, const char* name, double* out) {
    param_accessor_data_t* data = (param_accessor_data_t*)self->data;
    const cJSON* item = cJSON_GetObjectItem(data->args, name);
    if (!item || !cJSON_IsNumber(item) || !out) {
        return 0;
    }
    *out = cJSON_GetNumberValue(item);
    return 1;
}

static int param_try_get_string(mcp_param_accessor_t* self, const char* name, const char** out) {
    param_accessor_data_t* data = (param_accessor_data_t*)self->data;
    const cJSON* item = cJSON_GetObjectItem(data->args, name);
    if (!item || !cJSON_IsString(item) || !out) {
        return 0;
    }
    *out = cJSON_GetStringValue(item);
    return 1;
}

static int param_try_get_bool(mcp_param_accessor_t* self, const char* name, int* out) {
    param_accessor_data_t* data = (param_accessor_data_t*)self->data;
    const cJSON* item = cJSON_GetObjectItem(data->args, name);
    if (!item || !cJSON_IsBool(item) || !out) {
        return 0;
    }
    *out = cJSON_IsTrue(item) ? 1 : 0;
    return 1;
}

static int param_try_get_double_array(mcp_param_accessor_t* self, const char* name, double** out, size_t* count) {
    if (!out || !count) {
        return 0;
    }
    *out = param_get_double_array(self, name, count);
    return (*out != NULL);
}

static int param_try_get_string_array(mcp_param_accessor_t* self, const char* name, char*** out, size_t* count) {
    if (!out || !count) {
        return 0;
    }
    *out = param_get_string_array(self, name, count);
    return (*out != NULL);
}

static int param_try_get_int_array(mcp_param_accessor_t* self, const char* name, int64_t** out, size_t* count) {
    if (!out || !count) {
        return 0;
    }
    *out = param_get_int_array(self, name, count);
    return (*out != NULL);
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

// Update dynamic capabilities based on registered features
static void update_dynamic_capabilities(embed_mcp_server_t *server) {
    if (!server || !server->protocol || !server->protocol->config || !server->protocol->config->capabilities) {
        return;
    }

    mcp_capabilities_t *capabilities = server->protocol->config->capabilities;

    // Update tools capability based on registered tools
    if (server->tool_registry) {
        size_t tool_count = mcp_tool_registry_get_tool_count(server->tool_registry);
        capabilities->server.tools = (tool_count > 0);
    }

    // Update resources capability based on registered resources
    if (server->resource_registry) {
        size_t resource_count = mcp_resource_registry_count(server->resource_registry);
        capabilities->server.resources = (resource_count > 0);
    }

    // Prompts capability - not implemented yet
    capabilities->server.prompts = false;

    // Logging is always available
    capabilities->server.logging = true;
}

// Protocol request handler
static cJSON *protocol_request_handler(const mcp_request_t *request, void *user_data) {
    embed_mcp_server_t *server = (embed_mcp_server_t*)user_data;

    if (!server || !request || !request->method) {
        mcp_log_debug("Invalid request parameters");
        return NULL;
    }

    if (server->debug) {
        mcp_log_debug("Handling request: %s", request->method);
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

    // Handle resources/list
    if (strcmp(request->method, "resources/list") == 0) {
        if (server->debug) {
            mcp_log_debug("Handling resources/list request");
        }

        cJSON *resources = mcp_resource_registry_list_resources(server->resource_registry);
        if (!resources) {
            if (server->debug) {
                mcp_log_debug("mcp_resource_registry_list_resources returned NULL");
            }
            return NULL;
        }

        if (server->debug) {
            mcp_log_debug("Creating result object for resources/list");
        }

        cJSON *result = cJSON_CreateObject();
        cJSON_AddItemToObject(result, "resources", resources);
        return result;
    }

    // Handle resources/read
    if (strcmp(request->method, "resources/read") == 0) {
        if (!request->params) return NULL;

        cJSON *uri_json = cJSON_GetObjectItem(request->params, "uri");
        if (!uri_json || !cJSON_IsString(uri_json)) return NULL;

        const char *uri = uri_json->valuestring;
        mcp_resource_content_t content;

        // Try static resources first
        int read_result = mcp_resource_registry_read_resource(server->resource_registry, uri, &content);

        // If static resource not found, try resource templates
        if (read_result != 0) {
            read_result = mcp_resource_registry_read_template(server->resource_registry, uri, &content);
        }

        if (read_result != 0) {
            return NULL;
        }

        // Create response
        cJSON *result = cJSON_CreateObject();
        cJSON *contents_array = cJSON_CreateArray();
        cJSON *content_obj = cJSON_CreateObject();

        cJSON_AddStringToObject(content_obj, "uri", uri);
        cJSON_AddStringToObject(content_obj, "mimeType", content.mime_type);

        if (content.is_binary) {
            // For binary data, we need to base64 encode it
            // For now, just return an error for binary resources
            cJSON_AddStringToObject(content_obj, "text", "[Binary content not supported yet]");
        } else {
            cJSON_AddStringToObject(content_obj, "text", (const char*)content.data);
        }

        cJSON_AddItemToArray(contents_array, content_obj);
        cJSON_AddItemToObject(result, "contents", contents_array);

        // Cleanup
        mcp_resource_content_cleanup(&content);

        return result;
    }

    // Handle resources/templates/list
    if (strcmp(request->method, "resources/templates/list") == 0) {
        if (server->debug) {
            mcp_log_debug("Handling resources/templates/list request");
        }

        cJSON *templates = mcp_resource_registry_list_templates(server->resource_registry);
        if (!templates) {
            if (server->debug) {
                mcp_log_debug("mcp_resource_registry_list_templates returned NULL");
            }
            return NULL;
        }

        if (server->debug) {
            mcp_log_debug("Creating result object for resources/templates/list");
        }

        cJSON *result = cJSON_CreateObject();
        cJSON_AddItemToObject(result, "resourceTemplates", templates);
        return result;
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

    // Streamable HTTP: ensure a session id exists after initialize.
    if (server->enable_sessions && server->session_manager && connection && !connection->session_id) {
        cJSON *root = cJSON_ParseWithLength(message, length);
        const cJSON *obj = NULL;

        if (root && cJSON_IsObject(root)) {
            obj = root;
        } else if (root && cJSON_IsArray(root) && cJSON_GetArraySize(root) > 0) {
            obj = cJSON_GetArrayItem(root, 0);
        }

        const cJSON *method = obj ? cJSON_GetObjectItem((cJSON*)obj, "method") : NULL;
        if (method && cJSON_IsString(method) && strcmp(method->valuestring, "initialize") == 0) {
            mcp_session_t *session = mcp_session_manager_create_session(server->session_manager, NULL);
            if (session) {
                mcp_connection_set_session_id(connection, mcp_session_get_id(session));
                mcp_session_unref(session);
            }
        }

        if (root) cJSON_Delete(root);
    }
    
    server->current_connection = connection;
    int result = mcp_protocol_handle_message(server->protocol, message);
    // Keep connection available until after message handling is complete
    // Don't set to NULL immediately as response sending might be synchronous
    if (result < 0) {
        mcp_log_error("Protocol message handling failed: %d", result);
    } else if (result > 0) {
        mcp_log_debug("Protocol message handled successfully, sent %d bytes", result);
    }
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
    mcp_result_t hal_result = hal_safe_get(&hal);
    if (hal_result != MCP_OK) {
        set_error(mcp_error_to_string(hal_result));
        return NULL;
    }

    // Use HAL memory allocation
    embed_mcp_server_t *server;
    hal_result = hal_safe_alloc(hal, sizeof(embed_mcp_server_t), (void**)&server);
    if (hal_result != MCP_OK) {
        set_error(mcp_error_to_string(hal_result));
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

    // Create resource registry
    server->resource_registry = mcp_resource_registry_create();
    if (!server->resource_registry) {
        embed_mcp_destroy(server);
        set_error("Failed to create resource registry");
        return NULL;
    }

    // Enable logging for resource registry if debug is enabled
    if (server->debug) {
        mcp_resource_registry_set_logging(server->resource_registry, 1);
    }

    // Create protocol config with user settings
    mcp_protocol_config_t *protocol_config = mcp_protocol_config_create_default();
    if (protocol_config) {
        // Set server info from user config
        mcp_protocol_config_set_server_info(protocol_config, config->name, config->version);

        // Set instructions from user config (optional)
        if (config->instructions) {
            mcp_protocol_config_set_instructions(protocol_config, config->instructions);
        }
    }

    // Create protocol with user config
    server->protocol = mcp_protocol_create(protocol_config);

    // Clean up protocol config (it's copied internally)
    if (protocol_config) {
        mcp_protocol_config_destroy(protocol_config);
    }
    
    if (!server->protocol) {
        embed_mcp_destroy(server);
        set_error("Failed to create protocol");
        return NULL;
    }
    
    // Set protocol callbacks
    mcp_protocol_set_send_callback(server->protocol, protocol_send_callback, server);
    mcp_protocol_set_request_handler(server->protocol, protocol_request_handler, server);

    // Update capabilities based on registered features
    update_dynamic_capabilities(server);

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

    if (server->resource_registry) {
        mcp_resource_registry_destroy(server->resource_registry);
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

















int embed_mcp_run(embed_mcp_server_t *server, embed_mcp_transport_t transport) {
    if (!server) {
        set_error("Invalid server");
        return -1;
    }

    // Create transport
    if (transport == EMBED_MCP_TRANSPORT_STDIO) {
        server->transport = mcp_transport_create_stdio();
    } else {
        server->transport = mcp_transport_create_http_with_path(server->port, server->host, server->path);
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
        // Poll transport for HTTP requests
        if (server->transport && transport == EMBED_MCP_TRANSPORT_HTTP) {
            extern int mcp_http_transport_poll(mcp_transport_t *transport);
            mcp_http_transport_poll(server->transport);
        }

        usleep(10000); // 10ms
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











const char *embed_mcp_get_error(void) {
    return g_error_message[0] ? g_error_message : "No error";
}

// Note: custom_func_data_t removed - replaced by universal wrapper system

// Universal function wrapper data
typedef struct {
    mcp_universal_func_t wrapper_func;
    const char** param_names;
    mcp_param_type_t* param_types;
    size_t param_count;
    mcp_return_type_t return_type;
    void* user_data;
} universal_func_data_t;

typedef struct {
    embed_mcp_tool_handler_t handler;
} schema_handler_data_t;

// Note: custom_function_wrapper removed - replaced by universal wrapper system

// Universal function wrapper that calls user-provided wrapper function
static cJSON* universal_function_wrapper(const cJSON *args, void *user_data) {
    universal_func_data_t* data = (universal_func_data_t*)user_data;

    // Create parameter accessor
    param_accessor_data_t accessor_data = { .args = args };
    mcp_param_accessor_t accessor = {
        .get_int = param_get_int,
        .get_double = param_get_double,
        .get_string = param_get_string,
        .get_bool = param_get_bool,
        .try_get_int = param_try_get_int,
        .try_get_double = param_try_get_double,
        .try_get_string = param_try_get_string,
        .try_get_bool = param_try_get_bool,
        .try_get_double_array = param_try_get_double_array,
        .try_get_string_array = param_try_get_string_array,
        .try_get_int_array = param_try_get_int_array,
        .get_double_array = param_get_double_array,
        .get_string_array = param_get_string_array,
        .get_int_array = param_get_int_array,
        .has_param = param_has_param,
        .get_param_count = param_get_param_count,
        .get_json = param_get_json,
        .data = &accessor_data
    };

    // Call the user's wrapper function with the parameter accessor
    void* result = data->wrapper_func(&accessor, data->user_data);

    // Convert result to structured data based on return type
    cJSON* result_data = NULL;
    switch (data->return_type) {
        case MCP_RETURN_INT:
            if (result) {
                result_data = cJSON_CreateNumber(*(int*)result);
                free(result);
            } else {
                result_data = cJSON_CreateNumber(0);
            }
            break;
        case MCP_RETURN_DOUBLE:
            if (result) {
                result_data = cJSON_CreateNumber(*(double*)result);
                free(result);
            } else {
                result_data = cJSON_CreateNumber(0.0);
            }
            break;
        case MCP_RETURN_STRING:
            if (result) {
                result_data = cJSON_CreateString((char*)result);
                free(result);
            } else {
                result_data = cJSON_CreateString("");
            }
            break;
        case MCP_RETURN_VOID:
            result_data = cJSON_CreateString("Operation completed");
            if (result) free(result);
            break;
        default:
            result_data = cJSON_CreateString("Unknown result type");
            if (result) free(result);
            break;
    }

    // Use the standard MCP tool success result format
    cJSON* mcp_result = mcp_tool_create_success_result(result_data);
    if (result_data) cJSON_Delete(result_data);

    return mcp_result;
}

static void universal_function_cleanup(void *user_data) {
    universal_func_data_t *data = (universal_func_data_t*)user_data;
    if (!data) {
        return;
    }

    if (data->param_names) {
        for (size_t i = 0; i < data->param_count; i++) {
            free((void*)data->param_names[i]);
        }
        free((void*)data->param_names);
    }

    if (data->param_types) {
        free(data->param_types);
    }

    free(data);
}

static void schema_handler_cleanup(void *user_data) {
    if (user_data) {
        free(user_data);
    }
}

static cJSON* schema_handler_wrapper(const cJSON *args, void *user_data) {
    schema_handler_data_t *data = (schema_handler_data_t*)user_data;
    if (!data || !data->handler) {
        return mcp_tool_create_error_result(MCP_TOOL_ERROR_INTERNAL,
                                            "Schema handler is not initialized",
                                            NULL);
    }

    cJSON *handler_result = data->handler(args);
    if (!handler_result) {
        return mcp_tool_create_execution_error("Schema tool handler returned null result");
    }

    cJSON *content = cJSON_GetObjectItem(handler_result, "content");
    cJSON *is_error = cJSON_GetObjectItem(handler_result, "isError");
    bool already_mcp_result = cJSON_IsArray(content) && cJSON_IsBool(is_error);
    if (already_mcp_result) {
        return handler_result;
    }

    cJSON *mcp_result = mcp_tool_create_success_result(handler_result);
    cJSON_Delete(handler_result);
    if (!mcp_result) {
        return mcp_tool_create_memory_error();
    }

    return mcp_result;
}

static int register_tool_internal(embed_mcp_server_t *server,
                                  const char *name,
                                  const char *description,
                                  const cJSON *input_schema,
                                  mcp_tool_execute_func_t execute_func,
                                  mcp_tool_cleanup_func_t cleanup_func,
                                  void *handler_data,
                                  const char *create_error,
                                  const char *register_error) {
    if (!server || !server->tool_registry) {
        set_error("Invalid server or tool registry not initialized");
        return -1;
    }

    mcp_tool_t *tool = mcp_tool_create_full(name,
                                            name,
                                            description,
                                            input_schema,
                                            NULL,
                                            execute_func,
                                            NULL,
                                            cleanup_func,
                                            handler_data);
    if (!tool) {
        if (cleanup_func && handler_data) {
            cleanup_func(handler_data);
        }
        set_error(create_error ? create_error : "Failed to create tool");
        return -1;
    }

    if (mcp_tool_registry_register_tool(server->tool_registry, tool) != 0) {
        mcp_tool_destroy(tool);
        set_error(register_error ? register_error : "Failed to register tool");
        return -1;
    }

    update_dynamic_capabilities(server);
    return 0;
}

static universal_func_data_t* create_universal_func_data(const char **traditional_names,
                                                         const mcp_param_desc_t *advanced_params,
                                                         mcp_param_type_t param_types[],
                                                         size_t param_count,
                                                         bool advanced_mode,
                                                         mcp_return_type_t return_type,
                                                         mcp_universal_func_t wrapper_func,
                                                         void *user_data) {
    universal_func_data_t *func_data = malloc(sizeof(universal_func_data_t));
    if (!func_data) {
        set_error("Memory allocation failed");
        return NULL;
    }

    func_data->wrapper_func = wrapper_func;
    func_data->param_count = param_count;
    func_data->return_type = return_type;
    func_data->user_data = user_data;
    func_data->param_names = NULL;
    func_data->param_types = NULL;

    if (param_count == 0) {
        return func_data;
    }

    func_data->param_names = malloc(param_count * sizeof(char*));
    func_data->param_types = malloc(param_count * sizeof(mcp_param_type_t));
    if (!func_data->param_names || !func_data->param_types) {
        universal_function_cleanup(func_data);
        set_error("Memory allocation failed");
        return NULL;
    }

    for (size_t i = 0; i < param_count; i++) {
        const char *name = advanced_mode ? advanced_params[i].name : traditional_names[i];
        func_data->param_names[i] = strdup(name);
        if (!func_data->param_names[i]) {
            universal_function_cleanup(func_data);
            set_error("Memory allocation failed");
            return NULL;
        }

        if (advanced_mode) {
            if (advanced_params[i].category == MCP_PARAM_SINGLE) {
                func_data->param_types[i] = advanced_params[i].single_type;
            } else if (advanced_params[i].category == MCP_PARAM_ARRAY) {
                func_data->param_types[i] = advanced_params[i].array_desc.element_type;
            } else {
                func_data->param_types[i] = MCP_PARAM_STRING;
            }
        } else {
            func_data->param_types[i] = param_types[i];
        }
    }

    return func_data;
}

static cJSON* create_input_schema_for_registration(const mcp_param_desc_t *advanced_params,
                                                   const char *param_descriptions[],
                                                   mcp_param_type_t param_types[],
                                                   universal_func_data_t *func_data,
                                                   size_t param_count,
                                                   bool advanced_mode) {
    if (param_count == 0) {
        return NULL;
    }

    mcp_param_desc_t *temp_params = NULL;
    const mcp_param_desc_t *schema_params = NULL;

    if (advanced_mode) {
        schema_params = advanced_params;
    } else {
        temp_params = malloc(param_count * sizeof(mcp_param_desc_t));
        if (!temp_params) {
            set_error("Memory allocation failed");
            return NULL;
        }

        for (size_t i = 0; i < param_count; i++) {
            temp_params[i].name = func_data->param_names[i];
            temp_params[i].description = param_descriptions[i];
            temp_params[i].category = MCP_PARAM_SINGLE;
            temp_params[i].required = 1;
            temp_params[i].single_type = param_types[i];
        }
        schema_params = temp_params;
    }

    cJSON *input_schema = create_schema_from_params((mcp_param_desc_t*)schema_params, param_count);
    if (temp_params) {
        free(temp_params);
    }

    if (!input_schema) {
        set_error("Failed to create input schema");
    }

    return input_schema;
}



// =============================================================================
// Resource API Implementation
// =============================================================================

int embed_mcp_add_text_resource(embed_mcp_server_t *server,
                                const char *uri,
                                const char *name,
                                const char *description,
                                const char *mime_type,
                                const char *content) {
    if (!server || !server->resource_registry) {
        set_error("Invalid server or resource registry not initialized");
        return -1;
    }

    int result = mcp_resource_registry_add_text(server->resource_registry,
                                               uri, name, description, mime_type, content);
    if (result != 0) {
        set_error("Failed to register text resource");
        return -1;
    }

    // Update capabilities to reflect that we now have resources
    update_dynamic_capabilities(server);

    return 0;
}

int embed_mcp_add_binary_resource(embed_mcp_server_t *server,
                                  const char *uri,
                                  const char *name,
                                  const char *description,
                                  const char *mime_type,
                                  const void *data,
                                  size_t size) {
    if (!server || !server->resource_registry) {
        set_error("Invalid server or resource registry not initialized");
        return -1;
    }

    int result = mcp_resource_registry_add_binary(server->resource_registry,
                                                 uri, name, description, mime_type, data, size);
    if (result != 0) {
        set_error("Failed to register binary resource");
        return -1;
    }

    // Update capabilities to reflect that we now have resources
    update_dynamic_capabilities(server);

    return 0;
}

int embed_mcp_add_file_resource(embed_mcp_server_t *server,
                                const char *uri,
                                const char *name,
                                const char *description,
                                const char *mime_type,
                                const char *file_path) {
    if (!server || !server->resource_registry) {
        set_error("Invalid server or resource registry not initialized");
        return -1;
    }

    int result = mcp_resource_registry_add_file(server->resource_registry,
                                               uri, name, description, mime_type, file_path);
    if (result != 0) {
        set_error("Failed to register file resource");
        return -1;
    }

    // Update capabilities to reflect that we now have resources
    update_dynamic_capabilities(server);

    return 0;
}

int embed_mcp_add_text_function_resource(embed_mcp_server_t *server,
                                         const char *uri,
                                         const char *name,
                                         const char *description,
                                         const char *mime_type,
                                         embed_mcp_text_resource_function_t function,
                                         void *user_data) {
    if (!server || !server->resource_registry) {
        set_error("Invalid server or resource registry not initialized");
        return -1;
    }

    int result = mcp_resource_registry_add_text_function(server->resource_registry,
                                                        uri, name, description, mime_type,
                                                        (mcp_resource_text_function_t)function, user_data);
    if (result != 0) {
        set_error("Failed to register text function resource");
        return -1;
    }

    // Update capabilities to reflect that we now have resources
    update_dynamic_capabilities(server);

    return 0;
}

int embed_mcp_add_binary_function_resource(embed_mcp_server_t *server,
                                           const char *uri,
                                           const char *name,
                                           const char *description,
                                           const char *mime_type,
                                           embed_mcp_binary_resource_function_t function,
                                           void *user_data) {
    if (!server || !server->resource_registry) {
        set_error("Invalid server or resource registry not initialized");
        return -1;
    }

    int result = mcp_resource_registry_add_binary_function(server->resource_registry,
                                                          uri, name, description, mime_type,
                                                          (mcp_resource_binary_function_t)function, user_data);
    if (result != 0) {
        set_error("Failed to register binary function resource");
        return -1;
    }

    // Update capabilities to reflect that we now have resources
    update_dynamic_capabilities(server);

    return 0;
}

size_t embed_mcp_get_resource_count(embed_mcp_server_t *server) {
    if (!server || !server->resource_registry) {
        return 0;
    }

    return mcp_resource_registry_count(server->resource_registry);
}

// =============================================================================
// Resource Templates API Implementation
// =============================================================================

int embed_mcp_add_resource_template(embed_mcp_server_t *server, mcp_resource_template_t *template) {
    if (!server || !server->resource_registry || !template) {
        set_error("Invalid parameters for resource template registration");
        return -1;
    }

    int result = mcp_resource_registry_add_template(server->resource_registry, template);
    if (result != 0) {
        set_error("Failed to register resource template");
        return -1;
    }

    // Update capabilities to reflect new templates
    update_dynamic_capabilities(server);

    return 0;
}

size_t embed_mcp_get_resource_template_count(embed_mcp_server_t *server) {
    if (!server || !server->resource_registry) {
        return 0;
    }

    return mcp_resource_registry_template_count(server->resource_registry);
}

int embed_mcp_add_tool(embed_mcp_server_t *server,
                       const char *name,
                       const char *description,
                       const void *param_names,
                       const char *param_descriptions[],
                       mcp_param_type_t param_types[],
                       size_t param_count,
                       mcp_return_type_t return_type,
                       mcp_universal_func_t wrapper_func,
                       void *user_data) {
    if (!server || !server->tool_registry) {
        set_error("Invalid server or tool registry not initialized");
        return -1;
    }

    if (!name || !description || !wrapper_func) {
        set_error("Invalid parameters: name, description, and wrapper_func are required");
        return -1;
    }

    // Detect advanced mode: if param_descriptions is NULL and param_types is NULL,
    // then param_names is actually a mcp_param_desc_t*
    bool advanced_mode = (param_count > 0 && param_descriptions == NULL && param_types == NULL);

    if (param_count > 0 && !param_names) {
        set_error("Invalid parameters: param_names is required when param_count > 0");
        return -1;
    }

    if (param_count > 0 && !advanced_mode && (!param_descriptions || !param_types)) {
        set_error("Invalid parameters: param_descriptions and param_types are required in traditional mode");
        return -1;
    }

    // Cast param_names to appropriate type based on mode
    const char **traditional_names = advanced_mode ? NULL : (const char**)param_names;
    const mcp_param_desc_t *advanced_params = advanced_mode ? (const mcp_param_desc_t*)param_names : NULL;

    universal_func_data_t *func_data = create_universal_func_data(traditional_names,
                                                                   advanced_params,
                                                                   param_types,
                                                                   param_count,
                                                                   advanced_mode,
                                                                   return_type,
                                                                   wrapper_func,
                                                                   user_data);
    if (!func_data) {
        return -1;
    }

    cJSON *input_schema = create_input_schema_for_registration(advanced_params,
                                                                param_descriptions,
                                                                param_types,
                                                                func_data,
                                                                param_count,
                                                                advanced_mode);
    if (!input_schema && param_count > 0) {
        universal_function_cleanup(func_data);
        return -1;
    }

    return register_tool_internal(server,
                                  name,
                                  description,
                                  input_schema,
                                  universal_function_wrapper,
                                  universal_function_cleanup,
                                  func_data,
                                  "Failed to create tool",
                                  "Failed to register tool");
}

int embed_mcp_add_tool_with_schema(embed_mcp_server_t *server,
                                   const char *name,
                                   const char *description,
                                   const cJSON *schema,
                                   embed_mcp_tool_handler_t handler) {
    if (!name || !description || !handler) {
        set_error("Invalid parameters: name, description, and handler are required");
        return -1;
    }

    schema_handler_data_t *handler_data = malloc(sizeof(schema_handler_data_t));
    if (!handler_data) {
        set_error("Memory allocation failed");
        return -1;
    }
    handler_data->handler = handler;

    return register_tool_internal(server,
                                  name,
                                  description,
                                  schema,
                                  schema_handler_wrapper,
                                  schema_handler_cleanup,
                                  handler_data,
                                  "Failed to create tool with schema",
                                  "Failed to register schema tool");
}
