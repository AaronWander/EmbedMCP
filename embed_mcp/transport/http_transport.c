#include "transport/http_transport.h"
#include "../platform/platform_http_interface.h"
#include "hal/platform_hal.h"
#include "utils/logging.h"
#include "protocol/message.h"
#include "protocol/jsonrpc.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// MCP 协议处理函数
static int handle_mcp_request(const mcp_platform_http_request_t* request,
                             mcp_platform_http_response_t* response,
                             void* user_data) {
    mcp_http_transport_data_t* data = (mcp_http_transport_data_t*)user_data;
    if (!data || !data->transport) {
        mcp_log_error("HTTP Transport: Invalid user data in request handler");
        return -1;
    }

    mcp_log_debug("HTTP Transport: Handling %s request to %s", request->method, request->url);

    // 处理 notifications/initialized
    if (strcmp(request->method, "POST") == 0 && request->body) {
        // 检查是否为通知
        if (strstr(request->body, "notifications/initialized")) {
            mcp_log_debug("HTTP Transport: Received notifications/initialized");
            response->status_code = 202;  // HTTP 202 Accepted
            response->content_type = "application/json";
            response->body = "";
            response->body_length = 0;
            response->close_connection = true;
            return 0;
        }

        // 检查是否为 initialize 或其他 MCP 请求
        if (strstr(request->body, "\"method\"")) {
            // 创建连接对象
            mcp_connection_t* connection = calloc(1, sizeof(mcp_connection_t));
            if (connection) {
                connection->transport = data->transport;
                connection->is_active = true;
                connection->created_time = time(NULL);
                connection->last_activity = time(NULL);
                connection->private_data = (void*)request->platform_connection;

                // 调用消息接收回调
                if (data->transport->on_message) {
                    data->transport->on_message(request->body, request->body_length, connection, data->transport->user_data);
                }

                // 不在这里设置响应，让 send 函数处理
                response->status_code = 0;  // 标记为延迟响应
                response->content_type = NULL;
                response->body = NULL;
                response->body_length = 0;
                response->close_connection = false;

                free(connection);
                return 0;
            }
        }
    }

    // 默认响应
    response->status_code = 404;
    response->content_type = "text/plain";
    response->body = "Not Found";
    response->body_length = strlen(response->body);
    response->close_connection = true;
    return 0;
}

// HTTP transport interface implementation
const mcp_transport_interface_t mcp_http_transport_interface = {
    .init = mcp_http_transport_init_impl,
    .start = mcp_http_transport_start_impl,
    .stop = mcp_http_transport_stop_impl,
    .send = mcp_http_transport_send_impl,
    .close_connection = mcp_http_transport_close_connection_impl,
    .get_stats = mcp_http_transport_get_stats_impl,
    .cleanup = mcp_http_transport_cleanup_impl
};

// HTTP-specific functions
int mcp_http_transport_init_impl(mcp_transport_t *transport, const mcp_transport_config_t *config) {
    if (!transport || !config || config->type != MCP_TRANSPORT_HTTP) {
        mcp_log_error("HTTP Transport: Invalid parameters for init");
        return -1;
    }

    mcp_http_transport_data_t *data = calloc(1, sizeof(mcp_http_transport_data_t));
    if (!data) {
        mcp_log_error("HTTP Transport: Failed to allocate transport data");
        return -1;
    }

    // 获取平台 HTTP 接口
    data->platform_interface = mcp_platform_get_http_interface();
    if (!data->platform_interface) {
        mcp_log_error("HTTP Transport: No platform HTTP interface available");
        free(data);
        return -1;
    }

    // Store configuration
    transport->config = calloc(1, sizeof(mcp_transport_config_t));
    if (transport->config) {
        *transport->config = *config;
        // Duplicate string fields
        if (config->config.http.bind_address) {
            transport->config->config.http.bind_address = strdup(config->config.http.bind_address);
        }
    }

    // Initialize HTTP data
    data->port = config->config.http.port;
    data->bind_address = config->config.http.bind_address ? strdup(config->config.http.bind_address) : strdup("0.0.0.0");
    data->enable_cors = config->config.http.enable_cors;
    data->max_request_size = config->config.http.max_request_size;
    data->server_running = false;
    data->transport = transport;

    // 初始化平台 HTTP 接口
    if (data->platform_interface->init && data->platform_interface->init(config, data) < 0) {
        mcp_log_error("HTTP Transport: Platform interface init failed");
        free(data->bind_address);
        free(data);
        return -1;
    }

    transport->private_data = data;
    transport->state = MCP_TRANSPORT_STATE_STOPPED;

    mcp_log_info("HTTP Transport: Initialized with platform: %s",
                 data->platform_interface->platform_name);
    return 0;
}

int mcp_http_transport_start_impl(mcp_transport_t *transport) {
    if (!transport || !transport->private_data) {
        mcp_log_error("HTTP Transport: Invalid parameters for start");
        return -1;
    }

    mcp_http_transport_data_t *data = (mcp_http_transport_data_t*)transport->private_data;

    if (data->server_running) {
        mcp_log_warn("HTTP Transport: Server already running");
        return 0;
    }

    // 设置请求处理器
    if (data->platform_interface->set_handler) {
        const char* path = data->endpoint_path ? data->endpoint_path : "/mcp";
        if (data->platform_interface->set_handler(path, handle_mcp_request, data) < 0) {
            mcp_log_error("HTTP Transport: Failed to set request handler");
            return -1;
        }
    }

    // 启动平台 HTTP 服务器
    if (data->platform_interface->start && data->platform_interface->start() < 0) {
        mcp_log_error("HTTP Transport: Failed to start platform HTTP server");
        return -1;
    }

    data->server_running = true;
    transport->state = MCP_TRANSPORT_STATE_RUNNING;

    mcp_log_info("HTTP Transport: Server started on %s:%d using %s",
                 data->bind_address, data->port, data->platform_interface->platform_name);
    return 0;
}

int mcp_http_transport_stop_impl(mcp_transport_t *transport) {
    if (!transport || !transport->private_data) {
        return -1;
    }

    mcp_http_transport_data_t *data = (mcp_http_transport_data_t*)transport->private_data;

    if (!data->server_running) {
        return 0;
    }

    // 停止平台 HTTP 服务器
    if (data->platform_interface->stop) {
        data->platform_interface->stop();
    }

    data->server_running = false;
    transport->state = MCP_TRANSPORT_STATE_STOPPED;

    mcp_log_info("HTTP Transport: Server stopped");
    return 0;
}

int mcp_http_transport_send_impl(mcp_connection_t *connection, const char *message, size_t length) {
    if (!connection || !message || length == 0) {
        return -1;
    }

    mcp_http_transport_data_t *data = (mcp_http_transport_data_t*)connection->transport->private_data;
    if (!data || !data->platform_interface) {
        return -1;
    }

    // 构造 JSON 响应（不使用SSE）
    mcp_platform_http_response_t response = {0};
    response.status_code = 200;
    response.content_type = "application/json";
    response.is_sse = false;  // 使用普通JSON响应
    response.sse_event = NULL;
    response.body = message;
    response.body_length = length;
    response.close_connection = true;  // MCP 通常是请求-响应模式

    // 发送响应
    if (data->platform_interface->send_response) {
        return data->platform_interface->send_response(connection->private_data, &response);
    }

    return -1;
}

int mcp_http_transport_close_connection_impl(mcp_connection_t *connection) {
    if (!connection) {
        return -1;
    }

    mcp_http_transport_data_t *data = (mcp_http_transport_data_t*)connection->transport->private_data;
    if (data && data->platform_interface && data->platform_interface->close_connection) {
        return data->platform_interface->close_connection(connection->private_data);
    }

    connection->is_active = false;
    return 0;
}

int mcp_http_transport_get_stats_impl(mcp_transport_t *transport, void *stats) {
    if (!transport || !transport->private_data || !stats) {
        return -1;
    }

    mcp_http_transport_data_t *data = (mcp_http_transport_data_t*)transport->private_data;

    // 简单的统计信息
    struct {
        size_t total_requests;
        size_t active_connections;
        bool server_running;
    } *http_stats = stats;

    http_stats->total_requests = data->total_requests;
    http_stats->active_connections = data->active_connections;
    http_stats->server_running = data->server_running;

    return 0;
}

void mcp_http_transport_cleanup_impl(mcp_transport_t *transport) {
    if (!transport || !transport->private_data) {
        return;
    }

    mcp_http_transport_data_t *data = (mcp_http_transport_data_t*)transport->private_data;

    // 停止服务器
    mcp_http_transport_stop_impl(transport);

    // 清理平台接口
    if (data->platform_interface && data->platform_interface->cleanup) {
        data->platform_interface->cleanup();
    }

    // 释放资源
    free(data->bind_address);
    free(data->endpoint_path);
    free(data);

    transport->private_data = NULL;

    mcp_log_info("HTTP Transport: Cleanup completed");
}

// 外部接口函数 - 用于 Linux HTTP 实现的轮询
extern int linux_http_poll(void);

// 轮询函数 - 供主循环调用
int mcp_http_transport_poll(mcp_transport_t *transport) {
    if (!transport || !transport->private_data) {
        return -1;
    }

    mcp_http_transport_data_t *data = (mcp_http_transport_data_t*)transport->private_data;

    if (!data->platform_interface) {
        return -1;
    }

    // 调用 mongoose 轮询函数
    if (strstr(data->platform_interface->platform_name, "mongoose")) {
        return linux_http_poll();
    }

    return 0;
}
