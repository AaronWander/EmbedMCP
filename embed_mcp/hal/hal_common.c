#include "hal_common.h"
#include <string.h>
#include <stdlib.h>

// 内存管理辅助函数实现
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

// 错误处理辅助函数实现
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

// 能力查询通用实现
bool hal_has_capability_generic(const mcp_platform_capabilities_t *capabilities, const char* capability) {
    if (!capabilities || !capability) return false;
    
    if (strcmp(capability, "dynamic_memory") == 0) return capabilities->has_dynamic_memory;
    if (strcmp(capability, "threading") == 0) return capabilities->has_threading;
    if (strcmp(capability, "networking") == 0) return capabilities->has_networking;
    
    return false;
}

// 平台初始化/清理的通用包装
int hal_platform_init_wrapper(platform_init_func_t init_func) {
    if (!init_func) return 0; // 如果没有初始化函数，认为成功
    return init_func();
}

void hal_platform_cleanup_wrapper(platform_cleanup_func_t cleanup_func) {
    if (cleanup_func) {
        cleanup_func();
    }
}

// 错误信息获取 - 现在使用统一的 mcp_error_to_string()
// 这个函数保留用于向后兼容，但建议使用 mcp_error_to_string()
const char* hal_get_error_string(mcp_result_t result) {
    return mcp_error_to_string(result);
}
