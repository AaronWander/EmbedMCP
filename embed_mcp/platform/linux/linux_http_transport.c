
#include "linux_http_transport.h"
#include "../../utils/logging.h"
#include "../../hal/platform_hal.h"  // 包含HAL类型定义
#include "mongoose.h"
#include "../../cjson/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Linux 平台 HTTP 传输实现 - 基于 mongoose
// 这是我们已经验证工作的实现

static struct mg_mgr mgr;
static struct mg_connection *server_conn = NULL;
static bool server_running = false;
static int server_port = 9943;  // 默认端口
static char server_bind_address[256] = "0.0.0.0";  // 默认绑定地址

// 注意：这些函数现在是存根，传输层直接使用mongoose
// 保留这些函数只是为了编译兼容性

// 注意：HAL类型定义现在在platform_hal.h中

// 存根变量
static void* g_hal_handler = NULL;
static void* g_hal_user_data = NULL;

// 辅助函数：将mg_str转换为C字符串（使用多个缓冲区避免覆盖）
static const char* mg_str_to_cstr(struct mg_str str) {
    static char buffers[4][1024];  // 4个缓冲区轮换使用
    static int current_buffer = 0;

    char* buffer = buffers[current_buffer];
    current_buffer = (current_buffer + 1) % 4;

    size_t len = str.len < sizeof(buffers[0]) - 1 ? str.len : sizeof(buffers[0]) - 1;
    strncpy(buffer, str.buf, len);
    buffer[len] = '\0';
    return buffer;
}



// mongoose 事件处理器 - HAL版本
static void mongoose_event_handler(struct mg_connection *c, int ev, void *ev_data) {
    if (ev == MG_EV_HTTP_MSG) {
        struct mg_http_message *hm = (struct mg_http_message *) ev_data;

        mcp_log_debug("Linux HTTP: Received %.*s request to %.*s",
                     (int)hm->method.len, hm->method.buf,
                     (int)hm->uri.len, hm->uri.buf);

        // 检查是否有HAL处理器
        if (!g_hal_handler) {
            mcp_log_error("Linux HTTP: No HAL handler available");
            mg_http_reply(c, 500, NULL, "Internal Server Error: No HAL handler");
            return;
        }

        // 转换mongoose请求到HAL请求
        mcp_hal_http_request_t hal_req = {
            .method = mg_str_to_cstr(hm->method),  // HTTP方法：POST, GET等
            .uri = mg_str_to_cstr(hm->uri),        // URL路径：/mcp等
            .body = hm->body.buf,
            .body_len = hm->body.len,
            .connection = (mcp_hal_connection_t)c
        };

        // 创建HAL响应
        mcp_hal_http_response_t hal_resp = {0};

        // 存根：直接返回404
        int result = -1;
        (void)hal_req;
        (void)hal_resp;
        (void)g_hal_user_data;

        // 发送响应
        if (result == 0) {
            // 检查是否为延迟响应
            if (hal_resp.status_code == 0) {
                // 延迟响应，等待send_response调用
                return;
            }

            // 立即发送响应
            mg_http_reply(c, hal_resp.status_code,
                         "Content-Type: application/json\r\n"
                         "Access-Control-Allow-Origin: *\r\n"
                         "Access-Control-Allow-Headers: Content-Type, Authorization, MCP-Session-Id, MCP-Protocol-Version\r\n",
                         "%s", hal_resp.body ? hal_resp.body : "");
        } else {
            mcp_log_error("Linux HTTP: Request handler failed with code %d", result);
            mg_http_reply(c, 500, NULL, "Internal Server Error");
        }
    }
}

// 注意：旧的平台接口函数已被移除，现在只使用HAL接口

// ============================================================================
// HAL接口实现 - 供HAL层调用
// ============================================================================



int linux_http_init(void* config) {
    // 存根函数
    (void)config;
    g_hal_handler = NULL;
    g_hal_user_data = NULL;

    // 存根：不做任何初始化
    mcp_log_info("Linux HTTP: Stub init function called");

    return 0;
}

int linux_http_start(void) {
    if (server_running) {
        mcp_log_warn("Linux HTTP HAL: Server already running");
        return 0;
    }

    // 直接启动mongoose服务器
    char bind_url[256];
    snprintf(bind_url, sizeof(bind_url), "http://%s:%d", server_bind_address, server_port);

    server_conn = mg_http_listen(&mgr, bind_url, mongoose_event_handler, g_hal_user_data);
    if (!server_conn) {
        mcp_log_error("Linux HTTP HAL: Failed to start server on %s", bind_url);
        return -1;
    }

    server_running = true;
    mcp_log_info("Linux HTTP HAL: Server started on %s", bind_url);
    return 0;
}

int linux_http_stop(void) {
    if (!server_running) {
        return 0;
    }

    server_running = false;
    if (server_conn) {
        server_conn->is_closing = 1;
        server_conn = NULL;
    }

    mcp_log_info("Linux HTTP HAL: Server stopped");
    return 0;
}

int linux_http_send(const void* data, size_t len) {
    // 这个函数现在主要用于向后兼容
    // 实际的HTTP响应发送通过 linux_http_send_response 完成
    mcp_log_warn("Linux HTTP HAL: send() called, but HTTP responses should use send_response()");
    (void)data;
    (void)len;
    return -1;
}

int linux_http_send_response(void* platform_connection, const mcp_hal_http_response_t* response) {
    if (!platform_connection || !response) {
        mcp_log_error("Linux HTTP HAL: Invalid parameters for send_response");
        return -1;
    }

    struct mg_connection* c = (struct mg_connection*)platform_connection;

    mcp_log_debug("Linux HTTP: Sending response: status=%d, body_len=%zu",
                 response->status_code, response->body_len);

    mg_http_reply(c, response->status_code,
                 "Content-Type: application/json\r\n"
                 "Access-Control-Allow-Origin: *\r\n"
                 "Access-Control-Allow-Headers: Content-Type, Authorization, MCP-Session-Id, MCP-Protocol-Version\r\n",
                 "%.*s", (int)response->body_len, response->body ? response->body : "");

    return (int)response->body_len;
}

int linux_http_recv(void* buffer, size_t max_len) {
    // 对于HTTP服务器，接收通过事件处理完成
    // 这里返回0表示没有数据
    (void)buffer;
    (void)max_len;
    return 0;
}



int linux_http_close(void) {
    // 关闭服务器
    return linux_http_stop();
}

bool linux_http_is_connected(void) {
    return server_running;
}

void linux_http_cleanup(void) {
    linux_http_stop();
    mg_mgr_free(&mgr);
    g_hal_handler = NULL;
    g_hal_user_data = NULL;
    mcp_log_info("Linux HTTP HAL: Cleanup completed");
}

int linux_http_poll(void) {
    if (server_running) {
        mg_mgr_poll(&mgr, 10); // 10ms 超时
    }
    return 0;
}
