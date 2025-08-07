#ifndef MCP_HTTP_TRANSPORT_H
#define MCP_HTTP_TRANSPORT_H

#include "transport_interface.h"
#include "../platform/platform_http_interface.h"

// HTTP transport specific structures (简化版本，使用平台抽象)
typedef struct {
    // 平台 HTTP 接口
    const mcp_platform_http_interface_t* platform_interface;

    // 传输配置
    char *bind_address;
    int port;
    char *endpoint_path;
    bool enable_cors;
    size_t max_request_size;

    // 状态
    bool server_running;

    // 统计信息
    size_t total_requests;
    size_t active_connections;

    // MCP 传输引用
    mcp_transport_t* transport;
} mcp_http_transport_data_t;

// HTTP transport interface implementation
extern const mcp_transport_interface_t mcp_http_transport_interface;

// Function declarations
int mcp_http_transport_init_impl(mcp_transport_t *transport, const mcp_transport_config_t *config);
int mcp_http_transport_start_impl(mcp_transport_t *transport);
int mcp_http_transport_stop_impl(mcp_transport_t *transport);
int mcp_http_transport_send_impl(mcp_connection_t *connection, const char *message, size_t length);
int mcp_http_transport_close_connection_impl(mcp_connection_t *connection);
int mcp_http_transport_get_stats_impl(mcp_transport_t *transport, void *stats);
void mcp_http_transport_cleanup_impl(mcp_transport_t *transport);

// 轮询函数 - 供主循环调用
int mcp_http_transport_poll(mcp_transport_t *transport);

#endif // MCP_HTTP_TRANSPORT_H
int mcp_http_add_connection(mcp_transport_t *transport, mcp_connection_t *connection);
int mcp_http_remove_connection(mcp_transport_t *transport, mcp_connection_t *connection);
