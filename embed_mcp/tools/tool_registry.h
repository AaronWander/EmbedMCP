#ifndef MCP_TOOL_REGISTRY_H
#define MCP_TOOL_REGISTRY_H

#include "tool_interface.h"
#include <stdbool.h>
#include <time.h>
#include <pthread.h>
#include "cjson/cJSON.h"

// Forward declarations
typedef struct mcp_tool_registry mcp_tool_registry_t;
typedef struct mcp_tool_entry mcp_tool_entry_t;

// Tool entry structure
struct mcp_tool_entry {
    mcp_tool_t *tool;
    
    // Registration information
    time_t registered_time;
    bool is_builtin;
    
    // Statistics
    size_t calls_made;
    size_t calls_successful;
    size_t calls_failed;
    time_t last_called;
    double total_execution_time;
    double average_execution_time;
    
    // Internal
    struct mcp_tool_entry *next;
};

// Tool registry configuration
typedef struct {
    size_t max_tools;
    bool enable_builtin_tools;
    bool enable_tool_stats;
    bool strict_validation;
    time_t tool_timeout;
} mcp_tool_registry_config_t;

// Tool registry structure
struct mcp_tool_registry {
    // Configuration
    mcp_tool_registry_config_t config;
    
    // Tool storage
    mcp_tool_entry_t *tools;
    size_t tool_count;
    
    // Thread safety
    pthread_rwlock_t tools_lock;
    pthread_mutex_t registry_mutex;
    
    // Statistics
    size_t total_tools_registered;
    size_t tools_unregistered;
    size_t total_calls_made;
    size_t total_calls_successful;
    size_t total_calls_failed;
};

// Tool registry lifecycle
mcp_tool_registry_t *mcp_tool_registry_create(const mcp_tool_registry_config_t *config);
void mcp_tool_registry_destroy(mcp_tool_registry_t *registry);

int mcp_tool_registry_start(mcp_tool_registry_t *registry);
int mcp_tool_registry_stop(mcp_tool_registry_t *registry);

// Tool registration
int mcp_tool_registry_register_tool(mcp_tool_registry_t *registry, mcp_tool_t *tool);
int mcp_tool_registry_unregister_tool(mcp_tool_registry_t *registry, const char *tool_name);
bool mcp_tool_registry_has_tool(const mcp_tool_registry_t *registry, const char *tool_name);

// Tool lookup
mcp_tool_t *mcp_tool_registry_find_tool(const mcp_tool_registry_t *registry, const char *tool_name);
mcp_tool_entry_t *mcp_tool_registry_find_tool_entry(const mcp_tool_registry_t *registry, 
                                                   const char *tool_name);

// Tool execution
cJSON *mcp_tool_registry_call_tool(mcp_tool_registry_t *registry,
                                  const char *tool_name,
                                  const cJSON *parameters);

// Tool listing
cJSON *mcp_tool_registry_list_tools(const mcp_tool_registry_t *registry);
cJSON *mcp_tool_registry_get_tool_info(const mcp_tool_registry_t *registry, const char *tool_name);
size_t mcp_tool_registry_get_tool_count(const mcp_tool_registry_t *registry);

// Built-in tools
int mcp_tool_registry_register_builtin_tools(mcp_tool_registry_t *registry);
int mcp_tool_registry_unregister_builtin_tools(mcp_tool_registry_t *registry);

// Tool validation
bool mcp_tool_registry_validate_tool(const mcp_tool_t *tool);
bool mcp_tool_registry_validate_parameters(const mcp_tool_t *tool, const cJSON *parameters);

// Registry statistics
cJSON *mcp_tool_registry_get_stats(const mcp_tool_registry_t *registry);
cJSON *mcp_tool_registry_get_tool_stats(const mcp_tool_registry_t *registry, const char *tool_name);
void mcp_tool_registry_reset_stats(mcp_tool_registry_t *registry);

// Registry configuration
mcp_tool_registry_config_t *mcp_tool_registry_config_create_default(void);
void mcp_tool_registry_config_destroy(mcp_tool_registry_config_t *config);

// Registry callbacks
typedef void (*mcp_tool_registered_callback_t)(mcp_tool_registry_t *registry,
                                              const mcp_tool_t *tool,
                                              void *user_data);
typedef void (*mcp_tool_unregistered_callback_t)(mcp_tool_registry_t *registry,
                                                const char *tool_name,
                                                void *user_data);
typedef void (*mcp_tool_called_callback_t)(mcp_tool_registry_t *registry,
                                          const char *tool_name,
                                          const cJSON *parameters,
                                          const cJSON *result,
                                          bool success,
                                          double execution_time,
                                          void *user_data);

void mcp_tool_registry_set_tool_registered_callback(mcp_tool_registry_t *registry,
                                                   mcp_tool_registered_callback_t callback,
                                                   void *user_data);
void mcp_tool_registry_set_tool_unregistered_callback(mcp_tool_registry_t *registry,
                                                     mcp_tool_unregistered_callback_t callback,
                                                     void *user_data);
void mcp_tool_registry_set_tool_called_callback(mcp_tool_registry_t *registry,
                                               mcp_tool_called_callback_t callback,
                                               void *user_data);

// Error handling
cJSON *mcp_tool_registry_create_error_result(int code, const char *message, cJSON *data);
cJSON *mcp_tool_registry_create_tool_not_found_error(const char *tool_name);
cJSON *mcp_tool_registry_create_invalid_params_error(const char *details);
cJSON *mcp_tool_registry_create_execution_error(const char *details);

// Utility functions
char **mcp_tool_registry_get_tool_names(const mcp_tool_registry_t *registry, size_t *count);
void mcp_tool_registry_free_tool_names(char **tool_names, size_t count);

#endif // MCP_TOOL_REGISTRY_H
