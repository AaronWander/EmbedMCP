#include "tools/tool_registry.h"
#include "tools/builtin_tools.h"
#include "hal/platform_hal.h"
#include "utils/logging.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Tool registry lifecycle
mcp_tool_registry_t *mcp_tool_registry_create(const mcp_tool_registry_config_t *config) {
    const mcp_platform_hal_t *hal = mcp_platform_get_hal();
    if (!hal) return NULL;

    mcp_tool_registry_t *registry = hal->memory.alloc(sizeof(mcp_tool_registry_t));
    if (!registry) return NULL;
    memset(registry, 0, sizeof(mcp_tool_registry_t));
    
    // Initialize configuration
    if (config) {
        registry->config = *config;
    } else {
        registry->config.max_tools = 100;
        registry->config.enable_builtin_tools = true;
        registry->config.enable_tool_stats = true;
        registry->config.strict_validation = true;
        registry->config.tool_timeout = 30; // 30 seconds
    }
    
    // Initialize thread safety
    if (pthread_rwlock_init(&registry->tools_lock, NULL) != 0) {
        free(registry);
        return NULL;
    }
    
    if (pthread_mutex_init(&registry->registry_mutex, NULL) != 0) {
        pthread_rwlock_destroy(&registry->tools_lock);
        free(registry);
        return NULL;
    }
    
    // Initialize tool storage
    registry->tools = NULL;
    registry->tool_count = 0;
    
    // Initialize statistics
    registry->total_tools_registered = 0;
    registry->tools_unregistered = 0;
    registry->total_calls_made = 0;
    registry->total_calls_successful = 0;
    registry->total_calls_failed = 0;
    
    mcp_log_info("Tool registry created with max_tools=%zu", registry->config.max_tools);
    
    return registry;
}

void mcp_tool_registry_destroy(mcp_tool_registry_t *registry) {
    if (!registry) return;

    const mcp_platform_hal_t *hal = mcp_platform_get_hal();

    // Unregister all tools
    pthread_rwlock_wrlock(&registry->tools_lock);

    mcp_tool_entry_t *current = registry->tools;
    while (current) {
        mcp_tool_entry_t *next = current->next;
        mcp_tool_unref(current->tool);
        if (hal) {
            hal->memory.free(current);
        }
        current = next;
    }

    pthread_rwlock_unlock(&registry->tools_lock);

    // Cleanup thread safety
    pthread_rwlock_destroy(&registry->tools_lock);
    pthread_mutex_destroy(&registry->registry_mutex);

    if (hal) {
        hal->memory.free(registry);
    }

    mcp_log_info("Tool registry destroyed");
}

int mcp_tool_registry_start(mcp_tool_registry_t *registry) {
    if (!registry) return -1;
    
    // Register built-in tools if enabled
    if (registry->config.enable_builtin_tools) {
        int registered = mcp_builtin_tools_register_all(registry);
        mcp_log_info("Registered %d built-in tools", registered);
    }
    
    return 0;
}

int mcp_tool_registry_stop(mcp_tool_registry_t *registry) {
    if (!registry) return -1;
    
    // Unregister built-in tools if they were registered
    if (registry->config.enable_builtin_tools) {
        mcp_builtin_tools_unregister_all(registry);
    }
    
    return 0;
}

// Tool registration
int mcp_tool_registry_register_tool(mcp_tool_registry_t *registry, mcp_tool_t *tool) {
    if (!registry || !tool) return -1;
    
    // Validate tool
    if (registry->config.strict_validation && !mcp_tool_validate(tool)) {
        mcp_log_error("Tool validation failed for '%s'", mcp_tool_get_name(tool));
        return -1;
    }
    
    const char *tool_name = mcp_tool_get_name(tool);
    
    pthread_rwlock_wrlock(&registry->tools_lock);
    
    // Check if tool already exists
    if (mcp_tool_registry_find_tool_entry(registry, tool_name)) {
        pthread_rwlock_unlock(&registry->tools_lock);
        mcp_log_error("Tool '%s' already registered", tool_name);
        return -1;
    }
    
    // Check maximum tools limit
    if (registry->tool_count >= registry->config.max_tools) {
        pthread_rwlock_unlock(&registry->tools_lock);
        mcp_log_error("Maximum tools limit reached (%zu)", registry->config.max_tools);
        return -1;
    }
    
    // Create tool entry
    mcp_tool_entry_t *entry = calloc(1, sizeof(mcp_tool_entry_t));
    if (!entry) {
        pthread_rwlock_unlock(&registry->tools_lock);
        return -1;
    }
    
    entry->tool = mcp_tool_ref(tool);
    entry->registered_time = time(NULL);
    entry->is_builtin = false; // Will be set by built-in tool registration
    entry->calls_made = 0;
    entry->calls_successful = 0;
    entry->calls_failed = 0;
    entry->last_called = 0;
    entry->total_execution_time = 0.0;
    entry->average_execution_time = 0.0;
    entry->next = registry->tools;
    
    registry->tools = entry;
    registry->tool_count++;
    registry->total_tools_registered++;
    
    pthread_rwlock_unlock(&registry->tools_lock);
    
    mcp_log_debug("Tool '%s' registered successfully", tool_name);
    
    return 0;
}

int mcp_tool_registry_unregister_tool(mcp_tool_registry_t *registry, const char *tool_name) {
    if (!registry || !tool_name) return -1;
    
    pthread_rwlock_wrlock(&registry->tools_lock);
    
    mcp_tool_entry_t *prev = NULL;
    mcp_tool_entry_t *current = registry->tools;
    
    while (current) {
        if (strcmp(mcp_tool_get_name(current->tool), tool_name) == 0) {
            // Remove from list
            if (prev) {
                prev->next = current->next;
            } else {
                registry->tools = current->next;
            }
            
            // Cleanup entry
            mcp_tool_unref(current->tool);
            free(current);
            
            registry->tool_count--;
            registry->tools_unregistered++;
            
            pthread_rwlock_unlock(&registry->tools_lock);
            
            mcp_log_debug("Tool '%s' unregistered successfully", tool_name);
            return 0;
        }
        
        prev = current;
        current = current->next;
    }
    
    pthread_rwlock_unlock(&registry->tools_lock);
    
    mcp_log_error("Tool '%s' not found for unregistration", tool_name);
    return -1;
}

bool mcp_tool_registry_has_tool(const mcp_tool_registry_t *registry, const char *tool_name) {
    if (!registry || !tool_name) return false;
    
    pthread_rwlock_rdlock((pthread_rwlock_t*)&registry->tools_lock);
    
    mcp_tool_entry_t *entry = mcp_tool_registry_find_tool_entry(registry, tool_name);
    bool found = (entry != NULL);
    
    pthread_rwlock_unlock((pthread_rwlock_t*)&registry->tools_lock);
    
    return found;
}

// Tool lookup
mcp_tool_t *mcp_tool_registry_find_tool(const mcp_tool_registry_t *registry, const char *tool_name) {
    if (!registry || !tool_name) return NULL;
    
    pthread_rwlock_rdlock((pthread_rwlock_t*)&registry->tools_lock);
    
    mcp_tool_entry_t *entry = mcp_tool_registry_find_tool_entry(registry, tool_name);
    mcp_tool_t *tool = entry ? mcp_tool_ref(entry->tool) : NULL;
    
    pthread_rwlock_unlock((pthread_rwlock_t*)&registry->tools_lock);
    
    return tool;
}

mcp_tool_entry_t *mcp_tool_registry_find_tool_entry(const mcp_tool_registry_t *registry, const char *tool_name) {
    if (!registry || !tool_name) return NULL;
    
    mcp_tool_entry_t *current = registry->tools;
    while (current) {
        if (strcmp(mcp_tool_get_name(current->tool), tool_name) == 0) {
            return current;
        }
        current = current->next;
    }
    
    return NULL;
}

// Tool execution
cJSON *mcp_tool_registry_call_tool(mcp_tool_registry_t *registry, const char *tool_name, const cJSON *parameters) {
    if (!registry || !tool_name) {
        return mcp_tool_registry_create_tool_not_found_error(tool_name);
    }
    
    pthread_rwlock_rdlock(&registry->tools_lock);
    
    mcp_tool_entry_t *entry = mcp_tool_registry_find_tool_entry(registry, tool_name);
    if (!entry) {
        pthread_rwlock_unlock(&registry->tools_lock);
        return mcp_tool_registry_create_tool_not_found_error(tool_name);
    }
    
    mcp_tool_t *tool = mcp_tool_ref(entry->tool);
    
    pthread_rwlock_unlock(&registry->tools_lock);
    
    // Execute tool and measure time
    clock_t start_time = clock();
    cJSON *result = mcp_tool_execute(tool, parameters);
    clock_t end_time = clock();
    
    double execution_time = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;
    
    // Update statistics
    pthread_rwlock_wrlock(&registry->tools_lock);
    
    entry = mcp_tool_registry_find_tool_entry(registry, tool_name);
    if (entry && registry->config.enable_tool_stats) {
        entry->calls_made++;
        entry->last_called = time(NULL);
        entry->total_execution_time += execution_time;
        entry->average_execution_time = entry->total_execution_time / entry->calls_made;
        
        if (result && cJSON_GetObjectItem(result, "success") && cJSON_IsTrue(cJSON_GetObjectItem(result, "success"))) {
            entry->calls_successful++;
            registry->total_calls_successful++;
        } else {
            entry->calls_failed++;
            registry->total_calls_failed++;
        }
        
        registry->total_calls_made++;
    }
    
    pthread_rwlock_unlock(&registry->tools_lock);
    
    mcp_tool_unref(tool);
    
    return result;
}

// Tool listing
cJSON *mcp_tool_registry_list_tools(const mcp_tool_registry_t *registry) {
    if (!registry) return NULL;
    
    pthread_rwlock_rdlock((pthread_rwlock_t*)&registry->tools_lock);
    
    cJSON *tools_array = cJSON_CreateArray();
    if (!tools_array) {
        pthread_rwlock_unlock((pthread_rwlock_t*)&registry->tools_lock);
        return NULL;
    }
    
    mcp_tool_entry_t *current = registry->tools;
    while (current) {
        cJSON *tool_def = mcp_tool_to_mcp_tool_definition(current->tool);
        if (tool_def) {
            cJSON_AddItemToArray(tools_array, tool_def);
        }
        current = current->next;
    }
    
    pthread_rwlock_unlock((pthread_rwlock_t*)&registry->tools_lock);
    
    return tools_array;
}

size_t mcp_tool_registry_get_tool_count(const mcp_tool_registry_t *registry) {
    if (!registry) return 0;
    
    pthread_rwlock_rdlock((pthread_rwlock_t*)&registry->tools_lock);
    size_t count = registry->tool_count;
    pthread_rwlock_unlock((pthread_rwlock_t*)&registry->tools_lock);
    
    return count;
}

// Configuration helpers
mcp_tool_registry_config_t *mcp_tool_registry_config_create_default(void) {
    mcp_tool_registry_config_t *config = calloc(1, sizeof(mcp_tool_registry_config_t));
    if (!config) return NULL;
    
    config->max_tools = 100;
    config->enable_builtin_tools = true;
    config->enable_tool_stats = true;
    config->strict_validation = true;
    config->tool_timeout = 30;
    
    return config;
}

void mcp_tool_registry_config_destroy(mcp_tool_registry_config_t *config) {
    if (config) {
        free(config);
    }
}

// Error helpers
cJSON *mcp_tool_registry_create_tool_not_found_error(const char *tool_name) {
    cJSON *data = cJSON_CreateObject();
    if (data && tool_name) {
        cJSON_AddStringToObject(data, "tool_name", tool_name);
    }
    
    cJSON *result = mcp_tool_create_error_result(MCP_TOOL_ERROR_NOT_FOUND, "Tool not found", data);
    
    if (data) cJSON_Delete(data);
    return result;
}

// Stub implementations for missing functions
int mcp_builtin_tools_unregister_all(mcp_tool_registry_t *registry) {
    (void)registry;
    return 0; // TODO: Implement
}
