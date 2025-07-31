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

void* hal_calloc(const mcp_platform_hal_t *hal, size_t count, size_t size) {
    if (!hal || count == 0 || size == 0) return NULL;
    
    size_t total_size = count * size;
    void *ptr = hal->memory.alloc(total_size);
    if (ptr) {
        memset(ptr, 0, total_size);
    }
    return ptr;
}

void* hal_realloc(const mcp_platform_hal_t *hal, void *ptr, size_t new_size) {
    if (!hal) return NULL;
    
    if (hal->memory.realloc) {
        return hal->memory.realloc(ptr, new_size);
    }
    
    // 如果HAL没有realloc，手动实现
    if (!ptr) {
        return hal->memory.alloc(new_size);
    }
    
    if (new_size == 0) {
        hal->memory.free(ptr);
        return NULL;
    }
    
    void *new_ptr = hal->memory.alloc(new_size);
    if (new_ptr && ptr) {
        // 注意：这里无法知道原始大小，这是手动realloc的限制
        // 在实际使用中应该避免这种情况
        memcpy(new_ptr, ptr, new_size); // 假设新大小不超过原大小
        hal->memory.free(ptr);
    }
    return new_ptr;
}

void hal_free(const mcp_platform_hal_t *hal, void *ptr) {
    if (!hal || !ptr) return;
    hal->memory.free(ptr);
}

// 错误处理辅助函数实现
hal_result_t hal_safe_get(const mcp_platform_hal_t **hal_out) {
    if (!hal_out) return HAL_ERROR_NULL_POINTER;
    
    const mcp_platform_hal_t *hal = mcp_platform_get_hal();
    if (!hal) return HAL_ERROR_PLATFORM_NOT_AVAILABLE;
    
    *hal_out = hal;
    return HAL_OK;
}

hal_result_t hal_safe_alloc(const mcp_platform_hal_t *hal, size_t size, void **ptr_out) {
    if (!hal || !ptr_out) return HAL_ERROR_NULL_POINTER;
    if (size == 0) return HAL_ERROR_INVALID_PARAMETER;
    
    void *ptr = hal->memory.alloc(size);
    if (!ptr) return HAL_ERROR_MEMORY_ALLOCATION;
    
    *ptr_out = ptr;
    return HAL_OK;
}

hal_result_t hal_safe_strdup(const mcp_platform_hal_t *hal, const char *str, char **str_out) {
    if (!hal || !str || !str_out) return HAL_ERROR_NULL_POINTER;
    
    char *copy = hal_strdup(hal, str);
    if (!copy) return HAL_ERROR_MEMORY_ALLOCATION;
    
    *str_out = copy;
    return HAL_OK;
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

// 错误信息获取
const char* hal_get_error_string(hal_result_t result) {
    switch (result) {
        case HAL_OK:
            return "Success";
        case HAL_ERROR_NULL_POINTER:
            return "Null pointer error";
        case HAL_ERROR_MEMORY_ALLOCATION:
            return "Memory allocation failed";
        case HAL_ERROR_INVALID_PARAMETER:
            return "Invalid parameter";
        case HAL_ERROR_PLATFORM_NOT_AVAILABLE:
            return "Platform HAL not available";
        default:
            return "Unknown error";
    }
}
