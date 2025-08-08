#ifndef PLATFORM_HAL_H
#define PLATFORM_HAL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// 平台类型检测
#if defined(FREERTOS)
    #define MCP_PLATFORM_FREERTOS
#else
    #define MCP_PLATFORM_LINUX  // 默认使用Linux HAL (适用于Linux/macOS/POSIX系统)
#endif

// 平台能力描述
typedef struct {
    bool has_dynamic_memory;    // 支持动态内存分配
    bool has_threading;         // 支持多线程
    bool has_networking;        // 支持网络栈
    uint32_t max_memory_kb;     // 最大可用内存(KB)
    uint8_t max_connections;    // 最大连接数
    uint32_t tick_frequency_hz; // 系统时钟频率
} mcp_platform_capabilities_t;

// 内存管理接口
typedef struct {
    void* (*alloc)(size_t size);
    void (*free)(void* ptr);
    void* (*realloc)(void* ptr, size_t new_size);
    size_t (*get_free_size)(void);
} mcp_platform_memory_t;

// 线程管理接口
typedef struct {
    int (*create)(void** handle, void* (*func)(void*), void* arg, uint32_t stack_size);
    int (*join)(void* handle);
    void (*yield)(void);
    void (*sleep_ms)(uint32_t ms);
    uint32_t (*get_id)(void);
} mcp_platform_thread_t;

// 同步原语接口
typedef struct {
    int (*mutex_create)(void** mutex);
    int (*mutex_lock)(void* mutex);
    int (*mutex_unlock)(void* mutex);
    int (*mutex_destroy)(void* mutex);
} mcp_platform_sync_t;

// 时间接口
typedef struct {
    uint32_t (*get_tick_ms)(void);
    uint64_t (*get_time_us)(void);
    void (*delay_ms)(uint32_t ms);
    void (*delay_us)(uint32_t us);
} mcp_platform_time_t;

// 传输类型 - 扩展现有的传输类型
// 注意：基础传输类型在transport_interface.h中定义
typedef enum {
    MCP_HAL_TRANSPORT_UART = 100,  // 避免与现有类型冲突
    MCP_HAL_TRANSPORT_SPI,
    MCP_HAL_TRANSPORT_CAN,
    MCP_HAL_TRANSPORT_USB,
    MCP_HAL_TRANSPORT_AUTO
} mcp_hal_transport_type_t;

// 传输接口
typedef struct {
    int (*init)(mcp_hal_transport_type_t type, void* config);
    int (*send)(const void* data, size_t len);
    int (*recv)(void* buffer, size_t max_len);
    int (*poll)(void);
    int (*close)(void);
    bool (*is_connected)(void);
} mcp_platform_transport_t;

// 完整的平台HAL
typedef struct {
    const char* platform_name;
    const char* version;
    mcp_platform_capabilities_t capabilities;
    
    mcp_platform_memory_t memory;
    mcp_platform_thread_t thread;
    mcp_platform_sync_t sync;
    mcp_platform_time_t time;
    mcp_platform_transport_t transport;
    
    // 平台初始化和清理
    int (*init)(void);
    void (*cleanup)(void);
} mcp_platform_hal_t;

// 获取当前平台的HAL实现
const mcp_platform_hal_t* mcp_platform_get_hal(void);

// 平台能力查询
const mcp_platform_capabilities_t* mcp_platform_get_capabilities(void);
bool mcp_platform_has_capability(const char* capability);

// 平台初始化
int mcp_platform_init(void);
void mcp_platform_cleanup(void);

#endif // PLATFORM_HAL_H
