#ifndef PLATFORM_HTTP_INTERFACE_H
#define PLATFORM_HTTP_INTERFACE_H

#include <stddef.h>
#include <stdbool.h>
#include "../transport/transport_interface.h"

// HTTP 连接句柄 (平台特定)
typedef void* mcp_platform_http_connection_t;

// HTTP 请求信息
typedef struct {
    const char* method;           // GET, POST, etc.
    const char* url;              // 请求 URL
    const char* version;          // HTTP 版本
    const char* body;             // 请求体
    size_t body_length;           // 请求体长度
    
    // 头部信息 (简化版本)
    const char* content_type;
    const char* user_agent;
    const char* session_id;
    
    // 平台特定的连接信息
    mcp_platform_http_connection_t platform_connection;
} mcp_platform_http_request_t;

// HTTP 响应信息
typedef struct {
    int status_code;              // HTTP 状态码
    const char* content_type;     // 响应内容类型
    const char* body;             // 响应体
    size_t body_length;           // 响应体长度
    bool close_connection;        // 是否关闭连接
    
    // SSE 支持
    bool is_sse;                  // 是否为 SSE 响应
    const char* sse_event;        // SSE 事件类型
} mcp_platform_http_response_t;

// HTTP 请求处理回调
typedef int (*mcp_platform_http_handler_t)(
    const mcp_platform_http_request_t* request,
    mcp_platform_http_response_t* response,
    void* user_data
);

// 平台 HTTP 传输接口
typedef struct {
    const char* platform_name;
    
    // 服务器生命周期
    int (*init)(const mcp_transport_config_t* config, void* user_data);
    int (*start)(void);
    int (*stop)(void);
    void (*cleanup)(void);
    
    // 请求处理
    int (*set_handler)(const char* path, mcp_platform_http_handler_t handler, void* user_data);
    int (*send_response)(mcp_platform_http_connection_t connection, 
                        const mcp_platform_http_response_t* response);
    
    // 连接管理
    int (*close_connection)(mcp_platform_http_connection_t connection);
    bool (*is_connection_active)(mcp_platform_http_connection_t connection);
    
    // 平台特定功能
    int (*get_stats)(void* stats);
    int (*set_option)(const char* option, const void* value);
    
} mcp_platform_http_interface_t;

// 平台工厂函数
const mcp_platform_http_interface_t* mcp_platform_get_http_interface(void);

#endif // PLATFORM_HTTP_INTERFACE_H
