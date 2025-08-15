#include "transport/http_transport.h"
#include "hal/platform_hal.h"
#include "utils/logging.h"
#include "protocol/message.h"
#include "protocol/jsonrpc.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// HTTP请求处理函数 - 通过HAL接口
static void http_request_handler(const mcp_hal_http_request_t* request,
                                mcp_hal_http_response_t* response,
                                void* user_data) {
    mcp_http_transport_data_t* data = (mcp_http_transport_data_t*)user_data;

    mcp_log_debug("HTTP Transport: Received %s request to %s", request->method, request->uri);

    // 检查是否为POST请求到/mcp端点
    if (strcmp(request->method, "POST") == 0 && strcmp(request->uri, "/mcp") == 0) {

        // 处理notifications/initialized
        if (request->body && strstr(request->body, "notifications/initialized")) {
            mcp_log_debug("HTTP Transport: Received notifications/initialized");
            response->status_code = 202;
            response->headers = "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n";
            response->body = "";
            response->body_len = 0;
            return;
        }

        // 检查是否为MCP请求
        if (request->body && strstr(request->body, "\"method\"")) {
            // 创建连接对象
            mcp_connection_t* connection = calloc(1, sizeof(mcp_connection_t));
            if (!connection) {
                mcp_log_error("HTTP Transport: Failed to allocate connection");
                response->status_code = 500;
                response->headers = "Content-Type: application/json\r\n";
                response->body = "{\"error\":\"Internal server error\"}";
                response->body_len = strlen(response->body);
                return;
            }

            // 初始化连接对象
            connection->transport = data->transport;
            connection->is_active = true;
            connection->created_time = time(NULL);
            connection->last_activity = time(NULL);
            connection->private_data = (void*)request->connection;  // 保存HAL连接

            // 调用消息接收回调
            if (data->transport->on_message) {
                data->transport->on_message(request->body, request->body_len, connection, data->transport->user_data);
            }

            // 延迟响应 - 不设置响应内容，等待send函数调用
            response->status_code = 0;  // 特殊标记表示延迟响应
            return;
        }
    }

    // 默认404响应
    response->status_code = 404;
    response->headers = "Content-Type: text/plain\r\n";
    response->body = "Not Found";
    response->body_len = strlen(response->body);
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

    // 获取HAL接口
    data->hal = mcp_platform_get_hal();
    if (!data->hal) {
        mcp_log_error("HTTP Transport: No platform HAL available");
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

    transport->private_data = data;
    transport->state = MCP_TRANSPORT_STATE_STOPPED;

    mcp_log_info("HTTP Transport: Initialized on %s:%d", data->bind_address, data->port);
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

    // 构建监听地址
    char listen_url[512];
    snprintf(listen_url, sizeof(listen_url), "http://%s:%d", data->bind_address, data->port);

    // 通过HAL启动HTTP服务器 - 使用通用接口名称
    data->server = data->hal->network.http_server_start(listen_url, http_request_handler, data);
    if (!data->server) {
        mcp_log_error("HTTP Transport: Failed to start server on %s", listen_url);
        return -1;
    }

    data->server_running = true;
    transport->state = MCP_TRANSPORT_STATE_RUNNING;

    mcp_log_info("HTTP Transport: Server started on %s:%d", data->bind_address, data->port);
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

    // 通过HAL停止服务器 - 使用通用接口名称
    if (data->server) {
        data->hal->network.http_server_stop(data->server);
        data->server = NULL;
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
    if (!data) {
        return -1;
    }

    // 获取HAL连接
    mcp_hal_connection_t hal_conn = (mcp_hal_connection_t)connection->private_data;
    if (!hal_conn) {
        mcp_log_error("HTTP Transport: No HAL connection in send");
        return -1;
    }

    // 构造HAL响应
    mcp_hal_http_response_t response = {
        .status_code = 200,
        .headers = "Content-Type: application/json\r\n"
                  "Access-Control-Allow-Origin: *\r\n"
                  "Access-Control-Allow-Headers: Content-Type, Authorization, Mcp-Session-Id, Mcp-Protocol-Version\r\n",
        .body = message,
        .body_len = length
    };

    // 通过HAL发送响应 - 使用通用接口名称
    int result = data->hal->network.http_response_send(hal_conn, &response);
    if (result > 0) {
        mcp_log_debug("HTTP Transport: Sent response (%zu bytes)", length);
    }

    return result;
}

int mcp_http_transport_close_connection_impl(mcp_connection_t *connection) {
    if (!connection) {
        return -1;
    }

    // HTTP连接由HAL层管理，这里只标记为非活跃
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

    // 释放资源
    free(data->bind_address);
    free(data->endpoint_path);
    free(data);

    transport->private_data = NULL;

    mcp_log_info("HTTP Transport: Cleanup completed");
}

// 轮询函数 - 供主循环调用
int mcp_http_transport_poll(mcp_transport_t *transport) {
    if (!transport || !transport->private_data) {
        return -1;
    }

    mcp_http_transport_data_t *data = (mcp_http_transport_data_t*)transport->private_data;

    // 通过HAL轮询 - 使用通用接口名称
    if (data->server_running && data->hal) {
        return data->hal->network.network_poll(10); // 10ms超时
    }

    return 0;
}
