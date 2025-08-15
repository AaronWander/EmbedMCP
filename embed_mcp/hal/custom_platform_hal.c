/**
 * 自定义平台HAL实现示例
 * 展示如何为不支持mongoose的平台实现HAL接口
 */

#include "platform_hal.h"
#include "../utils/logging.h"
#include <stdlib.h>
#include <string.h>

// 示例：使用lwIP或其他网络库的平台实现

// 自定义平台的HTTP服务器实现
static mcp_hal_server_t custom_http_server_start(const char* url, mcp_hal_http_handler_t handler, void* user_data) {
    (void)handler;    // 避免未使用参数警告
    (void)user_data;  // 避免未使用参数警告

    mcp_log_info("Custom Platform: Starting HTTP server on %s", url);

    // 这里可以使用：
    // - lwIP的HTTP服务器
    // - 自定义的TCP socket实现
    // - 其他网络库

    // 示例伪代码：
    // custom_server_t* server = custom_http_server_create(url);
    // custom_http_server_set_handler(server, handler, user_data);
    // custom_http_server_start(server);
    // return (mcp_hal_server_t)server;

    return NULL; // 示例返回
}

static int custom_http_response_send(mcp_hal_connection_t conn, const mcp_hal_http_response_t* response) {
    (void)conn;  // 避免未使用参数警告

    mcp_log_debug("Custom Platform: Sending HTTP response, status=%d", response->status_code);

    // 这里可以使用：
    // - lwIP的send函数
    // - 自定义的socket send
    // - 其他网络库的发送函数

    // 示例伪代码：
    // custom_connection_t* c = (custom_connection_t*)conn;
    // return custom_http_send_response(c, response->status_code, response->headers, response->body, response->body_len);

    return (int)response->body_len; // 示例返回
}

static int custom_network_poll(int timeout_ms) {
    // 这里可以使用：
    // - select/poll系统调用
    // - 平台特定的事件轮询
    // - RTOS的任务调度
    
    // 示例伪代码：
    // return custom_network_poll_events(timeout_ms);
    
    (void)timeout_ms;
    return 0; // 示例返回
}

static int custom_http_server_stop(mcp_hal_server_t server) {
    mcp_log_info("Custom Platform: Stopping HTTP server");
    
    // 示例伪代码：
    // custom_server_t* s = (custom_server_t*)server;
    // custom_http_server_stop(s);
    // custom_http_server_destroy(s);
    
    (void)server;
    return 0; // 示例返回
}

// 底层socket实现（用于不支持高级HTTP库的平台）
static int custom_socket_create(int domain, int type, int protocol) {
    // 使用平台的socket API或自定义实现
    // 示例：return lwip_socket(domain, type, protocol);
    (void)domain; (void)type; (void)protocol;
    return -1; // 示例返回
}

static int custom_socket_bind(int sockfd, const char* address, uint16_t port) {
    // 使用平台的bind API
    (void)sockfd; (void)address; (void)port;
    return -1; // 示例返回
}

// 自定义平台初始化
static int custom_platform_init(void) {
    mcp_log_info("Custom Platform: Initializing platform-specific resources");
    
    // 这里可以初始化：
    // - 网络栈
    // - 内存池
    // - 硬件资源
    
    return 0;
}

static void custom_platform_cleanup(void) {
    mcp_log_info("Custom Platform: Cleaning up platform-specific resources");
    
    // 清理平台资源
}

// 自定义平台能力定义
static const mcp_platform_capabilities_t custom_capabilities = {
    .has_dynamic_memory = true,     // 根据平台调整
    .has_threading = true,          // 根据平台调整
    .has_networking = true,
    .max_memory_kb = 512,           // 根据平台调整
    .max_connections = 10,          // 根据平台调整
    .tick_frequency_hz = 100        // 根据平台调整
};

// 自定义平台HAL实现
static const mcp_platform_hal_t custom_platform_hal = {
    .platform_name = "CustomPlatform",  // 自定义平台名称
    .version = "1.0.0",
    .capabilities = custom_capabilities,
    
    // 内存管理 - 根据平台实现
    .memory = {
        .alloc = malloc,            // 或自定义内存分配器
        .free = free,               // 或自定义内存释放器
        .realloc = realloc,         // 或自定义内存重分配器
        .get_free_size = NULL       // 平台特定实现
    },
    
    // 线程管理 - 根据平台实现
    .thread = {
        .create = NULL,             // 平台特定线程创建
        .join = NULL,               // 平台特定线程等待
        .yield = NULL,              // 平台特定线程让出
        .sleep_ms = NULL,           // 平台特定线程睡眠
        .get_id = NULL              // 平台特定获取线程ID
    },
    
    // 同步原语 - 根据平台实现
    .sync = {
        .mutex_create = NULL,       // 平台特定互斥锁创建
        .mutex_destroy = NULL,      // 平台特定互斥锁销毁
        .mutex_lock = NULL,         // 平台特定互斥锁加锁
        .mutex_unlock = NULL        // 平台特定互斥锁解锁
    },
    
    // 时间管理 - 根据平台实现
    .time = {
        .get_tick_ms = NULL,        // 平台特定获取时钟
        .get_time_us = NULL,        // 平台特定获取微秒时间
        .delay_ms = NULL,           // 平台特定毫秒延时
        .delay_us = NULL            // 平台特定微秒延时
    },
    
    // 网络接口 - 使用通用接口名称
    .network = {
        // HTTP服务器接口 - 使用自定义实现
        .http_server_start = custom_http_server_start,
        .http_response_send = custom_http_response_send,
        .network_poll = custom_network_poll,
        .http_server_stop = custom_http_server_stop,
        
        // 底层网络接口 - 用于不支持高级HTTP库的平台
        .socket_create = custom_socket_create,
        .socket_bind = custom_socket_bind,
        .socket_send = NULL,        // 根据需要实现
        .socket_recv = NULL,        // 根据需要实现
        .socket_close = NULL        // 根据需要实现
    },
    
    // 平台初始化和清理
    .init = custom_platform_init,
    .cleanup = custom_platform_cleanup
};

// 获取自定义平台HAL接口
const mcp_platform_hal_t* mcp_get_custom_platform_hal(void) {
    return &custom_platform_hal;
}
