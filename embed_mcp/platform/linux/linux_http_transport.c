#include "../platform_http_interface.h"
#include "../../utils/logging.h"
#include "mongoose.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Linux 平台 HTTP 传输实现 - 基于 mongoose
// 这是我们已经验证工作的实现

static struct mg_mgr mgr;
static struct mg_connection *server_conn = NULL;
static bool server_running = false;
static mcp_platform_http_handler_t request_handler = NULL;
static void* handler_user_data = NULL;
static int server_port = 9943;  // 默认端口
static char server_bind_address[256] = "0.0.0.0";  // 默认绑定地址

// MCP 协议响应模板 (基于我们调试成功的格式)
static const char* mcp_initialize_response =
"{\"jsonrpc\":\"2.0\",\"id\":%d,\"result\":{\"protocolVersion\":\"%s\",\"capabilities\":{\"experimental\":{},\"prompts\":{\"listChanged\":true},\"resources\":{\"subscribe\":false,\"listChanged\":true},\"tools\":{\"listChanged\":true}},\"serverInfo\":{\"name\":\"EmbedMCP Server\",\"version\":\"1.0.0\"}}}";

static const char* mcp_tools_response =
"{\"jsonrpc\":\"2.0\",\"id\":%d,\"result\":{\"tools\":[{\"name\":\"add\",\"description\":\"Add two numbers together\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"a\":{\"type\":\"number\",\"description\":\"First number\"},\"b\":{\"type\":\"number\",\"description\":\"Second number\"}},\"required\":[\"a\",\"b\"]}},{\"name\":\"weather\",\"description\":\"Get weather information for a city\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"city\":{\"type\":\"string\",\"description\":\"City name (supports Jinan/济南)\"}},\"required\":[\"city\"]}},{\"name\":\"calculate_score\",\"description\":\"Calculate a score based on parameters\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"base\":{\"type\":\"integer\",\"description\":\"Base score\"},\"grade\":{\"type\":\"string\",\"description\":\"Grade letter\"},\"multiplier\":{\"type\":\"number\",\"description\":\"Score multiplier\"}},\"required\":[\"base\",\"grade\",\"multiplier\"]}}]}}";

// 解析 JSON-RPC 请求
static int parse_jsonrpc_request(const char* body, char* method, int* id, char* protocol_version) {
    // 初始化输出参数
    method[0] = '\0';
    *id = 0;

    // 简化的 JSON 解析 - 提取 method 和 id
    const char* method_start = strstr(body, "\"method\":");
    const char* id_start = strstr(body, "\"id\":");

    if (method_start) {
        // 跳过 "method":
        method_start += 9;
        // 跳过空格和引号
        while (*method_start == ' ' || *method_start == '\t') method_start++;
        if (*method_start == '"') {
            method_start++;
            const char* method_end = strchr(method_start, '"');
            if (method_end) {
                size_t len = method_end - method_start;
                if (len < 63) { // 确保不超出缓冲区
                    strncpy(method, method_start, len);
                    method[len] = '\0';
                }
            }
        }
    }

    if (id_start) {
        // 跳过 "id":
        id_start += 5;
        while (*id_start == ' ' || *id_start == '\t') id_start++;
        *id = atoi(id_start);
    }

    // 提取协议版本 (如果有)
    const char* version_start = strstr(body, "\"protocolVersion\":");
    if (version_start) {
        version_start += 18; // 跳过 "protocolVersion":
        while (*version_start == ' ' || *version_start == '\t') version_start++;
        if (*version_start == '"') {
            version_start++;
            const char* version_end = strchr(version_start, '"');
            if (version_end) {
                size_t len = version_end - version_start;
                if (len < 31) { // 确保不超出缓冲区
                    strncpy(protocol_version, version_start, len);
                    protocol_version[len] = '\0';
                }
            }
        }
    }

    return 0;
}

// mongoose 事件处理器
static void mongoose_event_handler(struct mg_connection *c, int ev, void *ev_data) {
    if (ev == MG_EV_HTTP_MSG) {
        struct mg_http_message *hm = (struct mg_http_message *) ev_data;

        mcp_log_debug("Linux HTTP: Received %.*s request to %.*s",
                     (int)hm->method.len, hm->method.buf,
                     (int)hm->uri.len, hm->uri.buf);

        // 处理 POST 请求
        if (mg_strcmp(hm->method, mg_str("POST")) == 0) {
            char method[64] = {0};
            char protocol_version[32] = "2025-03-26"; // 默认版本
            int id = 1;

            // 检查请求体
            if (hm->body.len == 0) {
                mg_http_reply(c, 400, NULL, "Bad Request: Empty body");
                return;
            }

            if (hm->body.len >= 4096) {
                mg_http_reply(c, 413, NULL, "Request Entity Too Large");
                return;
            }

            // 解析 JSON-RPC 请求
            char body[4096];
            size_t body_len = hm->body.len;
            strncpy(body, hm->body.buf, body_len);
            body[body_len] = '\0';

            parse_jsonrpc_request(body, method, &id, protocol_version);

            mcp_log_debug("Linux HTTP: Parsed method='%s', id=%d", method, id);

            // 处理不同的 MCP 方法
            if (strstr(method, "notifications/initialized")) {
                // 通知响应 - HTTP 202 Accepted
                mg_http_reply(c, 202,
                             "Content-Type: application/json\r\n"
                             "Access-Control-Allow-Origin: *\r\n"
                             "Access-Control-Allow-Headers: Content-Type, Authorization\r\n",
                             "");
                mcp_log_debug("Linux HTTP: Sent 202 Accepted for notifications/initialized");

            } else if (strcmp(method, "initialize") == 0) {
                // 初始化响应
                char response[2048];
                snprintf(response, sizeof(response), mcp_initialize_response, id, protocol_version);

                mg_http_reply(c, 200,
                             "Content-Type: text/event-stream\r\n"
                             "Cache-Control: no-cache, no-transform\r\n"
                             "Connection: keep-alive\r\n"
                             "Access-Control-Allow-Origin: *\r\n"
                             "Access-Control-Allow-Headers: Content-Type, Authorization\r\n",
                             "event: message\ndata: %s\n\n", response);
                mcp_log_debug("Linux HTTP: Sent initialize response (%zu bytes)", strlen(response));

            } else if (strcmp(method, "tools/list") == 0) {
                // 工具列表响应
                char response[4096];
                snprintf(response, sizeof(response), mcp_tools_response, id);

                mg_http_reply(c, 200,
                             "Content-Type: text/event-stream\r\n"
                             "Cache-Control: no-cache, no-transform\r\n"
                             "Connection: keep-alive\r\n"
                             "Access-Control-Allow-Origin: *\r\n"
                             "Access-Control-Allow-Headers: Content-Type, Authorization\r\n",
                             "event: message\ndata: %s\n\n", response);
                mcp_log_debug("Linux HTTP: Sent tools/list response (%zu bytes)", strlen(response));

            } else {
                // 其他请求 - 调用用户处理器
                if (request_handler) {
                    mcp_platform_http_request_t request = {0};
                    request.method = "POST";
                    request.url = "/mcp";
                    request.body = body;
                    request.body_length = body_len;
                    request.content_type = "application/json";
                    request.platform_connection = c;

                    mcp_platform_http_response_t response = {0};
                    if (request_handler(&request, &response, handler_user_data) == 0) {
                        // 如果状态码为0，表示延迟响应，不在这里发送
                        if (response.status_code == 0) {
                            return; // 等待 send_response 调用
                        }

                        if (response.is_sse) {
                            mg_http_reply(c, response.status_code,
                                         "Content-Type: text/event-stream\r\n"
                                         "Cache-Control: no-cache, no-transform\r\n"
                                         "Connection: keep-alive\r\n"
                                         "Access-Control-Allow-Origin: *\r\n"
                                         "Access-Control-Allow-Headers: Content-Type, Authorization\r\n",
                                         "event: %s\ndata: %s\n\n",
                                         response.sse_event ? response.sse_event : "message",
                                         response.body ? response.body : "");
                        } else {
                            mg_http_reply(c, response.status_code,
                                         "Content-Type: %s\r\n"
                                         "Access-Control-Allow-Origin: *\r\n"
                                         "Access-Control-Allow-Headers: Content-Type, Authorization\r\n",
                                         response.content_type ? response.content_type : "application/json",
                                         "%s", response.body ? response.body : "");
                        }
                    } else {
                        mcp_log_error("Linux HTTP: Request handler failed");
                        mg_http_reply(c, 500, NULL, "Internal Server Error");
                    }
                } else {
                    // 默认响应
                    mg_http_reply(c, 404, NULL, "Not Found");
                }
            }
        } else {
            // 非 POST 请求
            mg_http_reply(c, 405, NULL, "Method Not Allowed");
        }
    }
}

// 实现平台接口函数
static int linux_init(const mcp_transport_config_t* config, void* user_data) {
    mg_mgr_init(&mgr);

    // 保存配置信息
    if (config && config->type == MCP_TRANSPORT_HTTP) {
        server_port = config->config.http.port;
        if (config->config.http.bind_address) {
            strncpy(server_bind_address, config->config.http.bind_address, sizeof(server_bind_address) - 1);
            server_bind_address[sizeof(server_bind_address) - 1] = '\0';
        }
    }

    mcp_log_info("Linux HTTP: Initialized mongoose manager (port: %d, bind: %s)",
                 server_port, server_bind_address);
    return 0;
}

static int linux_start(void) {
    if (server_running) {
        return 0;
    }

    // 构建监听地址
    char listen_url[512];
    snprintf(listen_url, sizeof(listen_url), "http://%s:%d", server_bind_address, server_port);

    // 启动 HTTP 服务器
    server_conn = mg_http_listen(&mgr, listen_url, mongoose_event_handler, NULL);
    if (!server_conn) {
        mcp_log_error("Linux HTTP: Failed to start mongoose server on %s", listen_url);
        return -1;
    }

    server_running = true;
    mcp_log_info("Linux HTTP: Server started on port %d", server_port);
    return 0;
}

static int linux_stop(void) {
    if (!server_running) {
        return 0;
    }

    server_running = false;
    if (server_conn) {
        server_conn->is_closing = 1;
        server_conn = NULL;
    }

    mcp_log_info("Linux HTTP: Server stopped");
    return 0;
}

static void linux_cleanup(void) {
    linux_stop();
    mg_mgr_free(&mgr);
    mcp_log_info("Linux HTTP: Cleanup completed");
}

static int linux_set_handler(const char* path, mcp_platform_http_handler_t handler, void* user_data) {
    request_handler = handler;
    handler_user_data = user_data;
    mcp_log_debug("Linux HTTP: Handler set for path: %s", path);
    return 0;
}

static int linux_send_response(mcp_platform_http_connection_t connection,
                              const mcp_platform_http_response_t* response) {
    struct mg_connection* c = (struct mg_connection*)connection;
    if (!c || !response) {
        return -1;
    }

    if (response->is_sse) {
        mg_http_reply(c, response->status_code,
                     "Content-Type: text/event-stream\r\n"
                     "Cache-Control: no-cache, no-transform\r\n"
                     "Connection: keep-alive\r\n"
                     "Access-Control-Allow-Origin: *\r\n"
                     "Access-Control-Allow-Headers: Content-Type, Authorization\r\n",
                     "event: %s\ndata: %s\n\n",
                     response->sse_event ? response->sse_event : "message",
                     response->body ? response->body : "");
    } else {
        mg_http_reply(c, response->status_code,
                     "Content-Type: application/json\r\n"
                     "Access-Control-Allow-Origin: *\r\n"
                     "Access-Control-Allow-Headers: Content-Type, Authorization\r\n",
                     "%s", response->body ? response->body : "");
    }

    return 0;
}

// mongoose 轮询函数
int linux_http_poll(void) {
    if (server_running) {
        mg_mgr_poll(&mgr, 10); // 10ms 超时
    }
    return 0;
}

// 导出接口
const mcp_platform_http_interface_t linux_http_interface = {
    .platform_name = "Linux HTTP (mongoose)",
    .init = linux_init,
    .start = linux_start,
    .stop = linux_stop,
    .cleanup = linux_cleanup,
    .set_handler = linux_set_handler,
    .send_response = linux_send_response,
    .close_connection = NULL,  // mongoose 自动管理
    .is_connection_active = NULL,
    .get_stats = NULL,
    .set_option = NULL
};
