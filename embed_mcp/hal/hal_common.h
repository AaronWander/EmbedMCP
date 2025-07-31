#ifndef HAL_COMMON_H
#define HAL_COMMON_H

#include "platform_hal.h"
#include <stddef.h>
#include <stdbool.h>

// HAL通用辅助函数

// 内存管理辅助函数
char* hal_strdup(const mcp_platform_hal_t *hal, const char *str);
void* hal_calloc(const mcp_platform_hal_t *hal, size_t count, size_t size);
void* hal_realloc(const mcp_platform_hal_t *hal, void *ptr, size_t new_size);
void hal_free(const mcp_platform_hal_t *hal, void *ptr);

// 错误处理辅助函数
typedef enum {
    HAL_OK = 0,
    HAL_ERROR_NULL_POINTER = -1,
    HAL_ERROR_MEMORY_ALLOCATION = -2,
    HAL_ERROR_INVALID_PARAMETER = -3,
    HAL_ERROR_PLATFORM_NOT_AVAILABLE = -4
} hal_result_t;

hal_result_t hal_safe_get(const mcp_platform_hal_t **hal_out);
hal_result_t hal_safe_alloc(const mcp_platform_hal_t *hal, size_t size, void **ptr_out);
hal_result_t hal_safe_strdup(const mcp_platform_hal_t *hal, const char *str, char **str_out);

// 能力查询通用实现
bool hal_has_capability_generic(const mcp_platform_capabilities_t *capabilities, const char* capability);

// 平台初始化/清理的通用包装
typedef int (*platform_init_func_t)(void);
typedef void (*platform_cleanup_func_t)(void);

int hal_platform_init_wrapper(platform_init_func_t init_func);
void hal_platform_cleanup_wrapper(platform_cleanup_func_t cleanup_func);

// HAL导出函数的通用实现宏
#define HAL_IMPLEMENT_EXPORTS(hal_instance, capabilities_instance, init_func, cleanup_func) \
    const mcp_platform_hal_t* mcp_platform_get_hal(void) { \
        return &hal_instance; \
    } \
    \
    const mcp_platform_capabilities_t* mcp_platform_get_capabilities(void) { \
        return &capabilities_instance; \
    } \
    \
    bool mcp_platform_has_capability(const char* capability) { \
        return hal_has_capability_generic(&capabilities_instance, capability); \
    } \
    \
    int mcp_platform_init(void) { \
        return hal_platform_init_wrapper(init_func); \
    } \
    \
    void mcp_platform_cleanup(void) { \
        hal_platform_cleanup_wrapper(cleanup_func); \
    }

// 错误信息获取
const char* hal_get_error_string(hal_result_t result);

#endif // HAL_COMMON_H
