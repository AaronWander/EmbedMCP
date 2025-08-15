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

// HAL网络类型
typedef enum {
    MCP_HAL_NET_TCP,               // TCP网络
    MCP_HAL_NET_UDP,               // UDP网络
    MCP_HAL_NET_UART,              // UART串口
    MCP_HAL_NET_SPI,               // SPI总线
    MCP_HAL_NET_CAN,               // CAN总线
    MCP_HAL_NET_USB                // USB接口
} mcp_hal_network_type_t;

// 网络连接句柄
typedef void* mcp_hal_connection_handle_t;

// 网络地址结构
typedef struct {
    uint32_t ip;                   // IP地址 (网络字节序)
    uint16_t port;                 // 端口号
    char hostname[256];            // 主机名 (可选)
} mcp_hal_network_address_t;

// 网络事件类型
typedef enum {
    MCP_HAL_NET_EVENT_CONNECTED,    // 新连接建立
    MCP_HAL_NET_EVENT_DATA,         // 数据到达
    MCP_HAL_NET_EVENT_DISCONNECTED, // 连接断开
    MCP_HAL_NET_EVENT_ERROR         // 网络错误
} mcp_hal_network_event_type_t;

// 网络事件结构
typedef struct {
    mcp_hal_network_event_type_t type;
    mcp_hal_connection_handle_t connection;
    const void* data;
    size_t data_length;
    int error_code;                // 错误代码 (仅用于ERROR事件)
} mcp_hal_network_event_t;

// 网络事件回调函数
typedef void (*mcp_hal_network_event_callback_t)(const mcp_hal_network_event_t* event, void* user_data);

// 网络配置结构
typedef struct {
    mcp_hal_network_type_t type;   // 网络类型
    const char* bind_address;      // 绑定地址
    uint16_t port;                 // 端口号
    mcp_hal_network_event_callback_t callback; // 事件回调
    void* user_data;               // 用户数据
} mcp_hal_network_config_t;

// HAL网络接口 - 以mongoose为核心的统一抽象
typedef void* mcp_hal_connection_t;  // 连接句柄 (mongoose connection)
typedef void* mcp_hal_server_t;      // 服务器句柄 (mongoose server)

// HTTP请求结构 (基于mongoose)
typedef struct {
    const char* method;
    const char* uri;
    const char* body;
    size_t body_len;
    mcp_hal_connection_t connection;
} mcp_hal_http_request_t;

// HTTP响应结构
typedef struct {
    int status_code;
    const char* headers;
    const char* body;
    size_t body_len;
} mcp_hal_http_response_t;

// HTTP事件回调
typedef void (*mcp_hal_http_handler_t)(const mcp_hal_http_request_t* request,
                                      mcp_hal_http_response_t* response,
                                      void* user_data);

// HAL网络接口 - 通用的网络抽象接口
// 注意：使用通用名称，底层可以是mongoose、lwIP、或其他网络库
typedef struct {
    // HTTP服务器接口 - 通用接口名称
    mcp_hal_server_t (*http_server_start)(const char* url, mcp_hal_http_handler_t handler, void* user_data);
    int (*http_response_send)(mcp_hal_connection_t conn, const mcp_hal_http_response_t* response);

    // 网络事件轮询 - 通用接口名称
    int (*network_poll)(int timeout_ms);

    // 服务器管理 - 通用接口名称
    int (*http_server_stop)(mcp_hal_server_t server);

    // 底层网络接口 (用于不支持高级HTTP库的平台)
    int (*socket_create)(int domain, int type, int protocol);
    int (*socket_bind)(int sockfd, const char* address, uint16_t port);
    int (*socket_send)(int sockfd, const void* data, size_t len);
    int (*socket_recv)(int sockfd, void* buffer, size_t max_len);
    int (*socket_close)(int sockfd);
} mcp_platform_network_t;

// 完整的平台HAL
typedef struct {
    const char* platform_name;
    const char* version;
    mcp_platform_capabilities_t capabilities;

    mcp_platform_memory_t memory;
    mcp_platform_thread_t thread;
    mcp_platform_sync_t sync;
    mcp_platform_time_t time;
    mcp_platform_network_t network;  // 使用网络接口而不是传输接口

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
