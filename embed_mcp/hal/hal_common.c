#include "hal_common.h"
#include <string.h>
#include <stdlib.h>

// Memory management helper function implementations
char* hal_strdup(const mcp_platform_hal_t *hal, const char *str) {
    if (!hal || !str) return NULL;
    
    size_t len = strlen(str) + 1;
    char *copy = hal->memory.alloc(len);
    if (copy) {
        memcpy(copy, str, len);
    }
    return copy;
}



void hal_free(const mcp_platform_hal_t *hal, void *ptr) {
    if (!hal || !ptr) return;
    hal->memory.free(ptr);
}

// Error handling helper function implementations
mcp_result_t hal_safe_get(const mcp_platform_hal_t **hal_out) {
    if (!hal_out) return MCP_ERROR_NULL_POINTER;

    const mcp_platform_hal_t *hal = mcp_platform_get_hal();
    if (!hal) return MCP_ERROR_PLATFORM_NOT_AVAILABLE;

    *hal_out = hal;
    return MCP_OK;
}

mcp_result_t hal_safe_alloc(const mcp_platform_hal_t *hal, size_t size, void **ptr_out) {
    if (!hal || !ptr_out) return MCP_ERROR_NULL_POINTER;
    if (size == 0) return MCP_ERROR_INVALID_PARAMETER;

    void *ptr = hal->memory.alloc(size);
    if (!ptr) return MCP_ERROR_MEMORY_ALLOCATION;

    *ptr_out = ptr;
    return MCP_OK;
}

mcp_result_t hal_safe_strdup(const mcp_platform_hal_t *hal, const char *str, char **str_out) {
    if (!hal || !str || !str_out) return MCP_ERROR_NULL_POINTER;

    char *copy = hal_strdup(hal, str);
    if (!copy) return MCP_ERROR_MEMORY_ALLOCATION;

    *str_out = copy;
    return MCP_OK;
}

// Capability query common implementation
bool hal_has_capability_generic(const mcp_platform_capabilities_t *capabilities, const char* capability) {
    if (!capabilities || !capability) return false;
    
    if (strcmp(capability, "dynamic_memory") == 0) return capabilities->has_dynamic_memory;
    if (strcmp(capability, "threading") == 0) return capabilities->has_threading;
    if (strcmp(capability, "networking") == 0) return capabilities->has_networking;
    
    return false;
}

// Common wrapper for platform initialization/cleanup
int hal_platform_init_wrapper(platform_init_func_t init_func) {
    if (!init_func) return 0; // If no init function, consider success
    return init_func();
}

void hal_platform_cleanup_wrapper(platform_cleanup_func_t cleanup_func) {
    if (cleanup_func) {
        cleanup_func();
    }
}

// Error message retrieval - now uses unified mcp_error_to_string()
// This function is kept for backward compatibility, but mcp_error_to_string() is recommended
const char* hal_get_error_string(mcp_result_t result) {
    return mcp_error_to_string(result);
}
