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

int embed_mcp_add_tool(embed_mcp_server_t *server,
                       const char *name,
                       const char *description,
                       embed_mcp_tool_handler_t handler) {
    return embed_mcp_add_tool_with_schema(server, name, description, NULL, handler);
}

// Helper function to create math tool schema
static cJSON *create_math_tool_schema(void) {
    cJSON *schema = cJSON_CreateObject();

    cJSON_AddStringToObject(schema, "$schema", "http://json-schema.org/draft-07/schema#");
    cJSON_AddStringToObject(schema, "type", "object");
    cJSON_AddStringToObject(schema, "title", "Math Operation Parameters");
    cJSON_AddStringToObject(schema, "description", "Parameters for mathematical operations");

    cJSON *properties = cJSON_CreateObject();

    // Add 'a' parameter
    cJSON *a_prop = cJSON_CreateObject();
    cJSON_AddStringToObject(a_prop, "type", "number");
    cJSON_AddStringToObject(a_prop, "title", "First Number");
    cJSON_AddStringToObject(a_prop, "description", "The first number for the operation");
    cJSON_AddItemToObject(properties, "a", a_prop);

    // Add 'b' parameter
    cJSON *b_prop = cJSON_CreateObject();
    cJSON_AddStringToObject(b_prop, "type", "number");
    cJSON_AddStringToObject(b_prop, "title", "Second Number");
    cJSON_AddStringToObject(b_prop, "description", "The second number for the operation");
    cJSON_AddItemToObject(properties, "b", b_prop);

    cJSON_AddItemToObject(schema, "properties", properties);

    // Add required fields
    cJSON *required = cJSON_CreateArray();
    cJSON_AddItemToArray(required, cJSON_CreateString("a"));
    cJSON_AddItemToArray(required, cJSON_CreateString("b"));
    cJSON_AddItemToObject(schema, "required", required);

    cJSON_AddBoolToObject(schema, "additionalProperties", false);

    return schema;
}

// Helper function to create text tool schema
static cJSON *create_text_tool_schema(const char *param_name, const char *param_description) {
    cJSON *schema = cJSON_CreateObject();

    cJSON_AddStringToObject(schema, "$schema", "http://json-schema.org/draft-07/schema#");
    cJSON_AddStringToObject(schema, "type", "object");
    cJSON_AddStringToObject(schema, "title", "Text Tool Parameters");
    cJSON_AddStringToObject(schema, "description", "Parameters for text processing");

    cJSON *properties = cJSON_CreateObject();

    cJSON *param_prop = cJSON_CreateObject();
    cJSON_AddStringToObject(param_prop, "type", "string");
    cJSON_AddStringToObject(param_prop, "title", param_name);
    cJSON_AddStringToObject(param_prop, "description", param_description);
    cJSON_AddItemToObject(properties, param_name, param_prop);

    cJSON_AddItemToObject(schema, "properties", properties);

    // Add required fields
    cJSON *required = cJSON_CreateArray();
    cJSON_AddItemToArray(required, cJSON_CreateString(param_name));
    cJSON_AddItemToObject(schema, "required", required);

    cJSON_AddBoolToObject(schema, "additionalProperties", false);

    return schema;
}

int embed_mcp_add_math_tool(embed_mcp_server_t *server,
                            const char *name,
                            const char *description,
                            embed_mcp_tool_handler_t handler) {
    cJSON *schema = create_math_tool_schema();
    int result = embed_mcp_add_tool_with_schema(server, name, description, schema, handler);
    cJSON_Delete(schema);
    return result;
}

int embed_mcp_add_text_tool(embed_mcp_server_t *server,
                            const char *name,
                            const char *description,
                            const char *param_name,
                            const char *param_description,
                            embed_mcp_tool_handler_t handler) {
    cJSON *schema = create_text_tool_schema(param_name, param_description);
    int result = embed_mcp_add_tool_with_schema(server, name, description, schema, handler);
    cJSON_Delete(schema);
    return result;
}

// Wrapper function to adapt our handler signature to the expected one
static cJSON *tool_handler_wrapper(const cJSON *args, void *user_data) {
    embed_mcp_tool_handler_t handler = (embed_mcp_tool_handler_t)user_data;
    return handler(args);
}

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

int embed_mcp_run_stdio(embed_mcp_server_t *server) {
    return embed_mcp_run(server, EMBED_MCP_TRANSPORT_STDIO);
}

int embed_mcp_run_http(embed_mcp_server_t *server) {
    return embed_mcp_run(server, EMBED_MCP_TRANSPORT_HTTP);
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

// Convenience functions
embed_mcp_server_t *embed_mcp_create_simple(const char *name, const char *version) {
    embed_mcp_config_t config = embed_mcp_config_default(name, version);
    return embed_mcp_create(&config);
}

int embed_mcp_quick_start(const char *name, const char *version,
                          embed_mcp_transport_t transport, int port) {
    embed_mcp_config_t config = embed_mcp_config_default(name, version);
    config.port = port;
    config.debug = 1;

    embed_mcp_server_t *server = embed_mcp_create(&config);
    if (!server) return -1;

    int result = embed_mcp_run(server, transport);
    embed_mcp_destroy(server);

    return result;
}

// Utility functions
embed_mcp_config_t embed_mcp_config_default(const char *name, const char *version) {
    embed_mcp_config_t config = {0};
    config.name = name;
    config.version = version;
    config.host = "0.0.0.0";
    config.port = 8080;
    config.path = "/mcp";
    config.max_tools = 100;
    config.debug = 0;
    return config;
}

const char *embed_mcp_get_error(void) {
    return g_error_message[0] ? g_error_message : "No error";
}
