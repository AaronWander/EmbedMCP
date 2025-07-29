#ifndef MCP_REQUEST_ROUTER_H
#define MCP_REQUEST_ROUTER_H

#include "protocol/message.h"
#include "tools/tool_registry.h"
#include <stdbool.h>
#include <pthread.h>
#include "cJSON.h"

// Forward declarations
typedef struct mcp_request_router mcp_request_router_t;
typedef struct mcp_request_handler mcp_request_handler_t;

// Request handler function type
typedef cJSON *(*mcp_request_handler_func_t)(const mcp_request_t *request, void *user_data);

// Request handler structure
struct mcp_request_handler {
    char *method_name;
    mcp_request_handler_func_t handler_func;
    void *user_data;
    bool is_builtin;
    
    // Handler metadata
    char *description;
    cJSON *parameter_schema;
    
    // Statistics
    size_t requests_handled;
    size_t errors_encountered;
    time_t last_used;
    
    // Internal
    struct mcp_request_handler *next;
};

// Request router configuration
typedef struct {
    bool enable_builtin_handlers;
    bool strict_method_validation;
    size_t max_handlers;
    time_t handler_timeout;
    bool enable_handler_stats;
} mcp_request_router_config_t;

// Request router structure
struct mcp_request_router {
    // Configuration
    mcp_request_router_config_t config;
    
    // Handler registry
    mcp_request_handler_t *handlers;
    size_t handler_count;
    
    // Tool registry integration
    mcp_tool_registry_t *tool_registry;
    
    // Thread safety
    pthread_rwlock_t handlers_lock;
    pthread_mutex_t router_mutex;
    
    // Statistics
    size_t total_requests_routed;
    size_t requests_handled;
    size_t requests_failed;
    size_t unknown_methods;
};

// Request router lifecycle
mcp_request_router_t *mcp_request_router_create(const mcp_request_router_config_t *config);
void mcp_request_router_destroy(mcp_request_router_t *router);

int mcp_request_router_start(mcp_request_router_t *router);
int mcp_request_router_stop(mcp_request_router_t *router);

// Handler registration
int mcp_request_router_register_handler(mcp_request_router_t *router,
                                       const char *method_name,
                                       mcp_request_handler_func_t handler_func,
                                       void *user_data,
                                       const char *description);
int mcp_request_router_unregister_handler(mcp_request_router_t *router,
                                         const char *method_name);
bool mcp_request_router_has_handler(const mcp_request_router_t *router,
                                   const char *method_name);

// Request routing
cJSON *mcp_request_router_route_request(mcp_request_router_t *router,
                                       const mcp_request_t *request);
int mcp_request_router_handle_notification(mcp_request_router_t *router,
                                          const mcp_request_t *notification);

// Built-in handlers
cJSON *mcp_request_router_handle_initialize(const mcp_request_t *request, void *user_data);
cJSON *mcp_request_router_handle_ping(const mcp_request_t *request, void *user_data);
cJSON *mcp_request_router_handle_list_tools(const mcp_request_t *request, void *user_data);
cJSON *mcp_request_router_handle_call_tool(const mcp_request_t *request, void *user_data);
cJSON *mcp_request_router_handle_list_resources(const mcp_request_t *request, void *user_data);
cJSON *mcp_request_router_handle_read_resource(const mcp_request_t *request, void *user_data);
cJSON *mcp_request_router_handle_list_prompts(const mcp_request_t *request, void *user_data);
cJSON *mcp_request_router_handle_get_prompt(const mcp_request_t *request, void *user_data);

int mcp_request_router_handle_initialized(const mcp_request_t *notification, void *user_data);
int mcp_request_router_handle_set_level(const mcp_request_t *notification, void *user_data);

// Handler utilities
mcp_request_handler_t *mcp_request_router_find_handler(const mcp_request_router_t *router,
                                                      const char *method_name);
cJSON *mcp_request_router_list_handlers(const mcp_request_router_t *router);
cJSON *mcp_request_router_get_handler_info(const mcp_request_router_t *router,
                                          const char *method_name);

// Tool registry integration
int mcp_request_router_set_tool_registry(mcp_request_router_t *router,
                                        mcp_tool_registry_t *tool_registry);
mcp_tool_registry_t *mcp_request_router_get_tool_registry(const mcp_request_router_t *router);

// Router statistics
cJSON *mcp_request_router_get_stats(const mcp_request_router_t *router);
void mcp_request_router_reset_stats(mcp_request_router_t *router);

// Router configuration
mcp_request_router_config_t *mcp_request_router_config_create_default(void);
void mcp_request_router_config_destroy(mcp_request_router_config_t *config);

// Error handling
cJSON *mcp_request_router_create_error_response(int code, const char *message, cJSON *data);
cJSON *mcp_request_router_create_method_not_found_error(const char *method);
cJSON *mcp_request_router_create_invalid_params_error(const char *details);
cJSON *mcp_request_router_create_internal_error(const char *details);

// Validation utilities
bool mcp_request_router_validate_request(const mcp_request_t *request);
bool mcp_request_router_validate_method_name(const char *method_name);
bool mcp_request_router_validate_parameters(const mcp_request_t *request,
                                           const cJSON *parameter_schema);

// Built-in method registration
int mcp_request_router_register_builtin_handlers(mcp_request_router_t *router);
int mcp_request_router_unregister_builtin_handlers(mcp_request_router_t *router);

// Handler callbacks
typedef void (*mcp_handler_registered_callback_t)(mcp_request_router_t *router,
                                                 const char *method_name,
                                                 void *user_data);
typedef void (*mcp_handler_unregistered_callback_t)(mcp_request_router_t *router,
                                                   const char *method_name,
                                                   void *user_data);

void mcp_request_router_set_handler_registered_callback(mcp_request_router_t *router,
                                                       mcp_handler_registered_callback_t callback,
                                                       void *user_data);
void mcp_request_router_set_handler_unregistered_callback(mcp_request_router_t *router,
                                                         mcp_handler_unregistered_callback_t callback,
                                                         void *user_data);

#endif // MCP_REQUEST_ROUTER_H
