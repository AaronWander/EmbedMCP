#include "../platform_http_interface.h"
#include "../../utils/logging.h"
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
static mcp_platform_http_handler_t request_handler = NULL;
static void* handler_user_data = NULL;
static int server_port = 9943;  // 默认端口
static char server_bind_address[256] = "0.0.0.0";  // 默认绑定地址

// 解析 JSON-RPC 请求 - 使用 cJSON
static int parse_jsonrpc_request(const char* body, char* method, int* id, char* protocol_version) {
    // 初始化输出参数
    method[0] = '\0';
    *id = 0;
    protocol_version[0] = '\0';

    // 使用 cJSON 解析
    cJSON *json = cJSON_Parse(body);
    if (!json) {
        mcp_log_error("Linux HTTP: Failed to parse JSON: %s", cJSON_GetErrorPtr());
        return -1;
    }

    // 提取 method 字段
    cJSON *method_obj = cJSON_GetObjectItem(json, "method");
    if (method_obj && cJSON_IsString(method_obj)) {
        const char *method_str = cJSON_GetStringValue(method_obj);
        if (method_str) {
            size_t len = strlen(method_str);
            if (len < 63) { // 确保不超出缓冲区
                strcpy(method, method_str);
            } else {
                mcp_log_warn("Linux HTTP: Method name too long, truncating");
                strncpy(method, method_str, 62);
                method[62] = '\0';
            }
        }
    }

    // 提取 id 字段
    cJSON *id_obj = cJSON_GetObjectItem(json, "id");
    if (id_obj) {
        if (cJSON_IsNumber(id_obj)) {
            *id = cJSON_GetNumberValue(id_obj);
        } else if (cJSON_IsString(id_obj)) {
            // 有些客户端可能发送字符串ID
            const char *id_str = cJSON_GetStringValue(id_obj);
            if (id_str) {
                *id = atoi(id_str);
            }
        }
    }

    // 提取协议版本 (从 params.protocolVersion)
    cJSON *params = cJSON_GetObjectItem(json, "params");
    if (params && cJSON_IsObject(params)) {
        cJSON *version_obj = cJSON_GetObjectItem(params, "protocolVersion");
        if (version_obj && cJSON_IsString(version_obj)) {
            const char *version_str = cJSON_GetStringValue(version_obj);
            if (version_str) {
                size_t len = strlen(version_str);
                if (len < 31) { // 确保不超出缓冲区
                    strcpy(protocol_version, version_str);
                } else {
                    strncpy(protocol_version, version_str, 30);
                    protocol_version[30] = '\0';
                }
            }
        }
    }

    cJSON_Delete(json);
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

            // 处理所有 MCP 方法 - 统一交给用户处理器
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
        } else {
            // 非 POST 请求
            mg_http_reply(c, 405, NULL, "Method Not Allowed");
        }
    }
}

// 实现平台接口函数
static int linux_init(const mcp_transport_config_t* config, void* user_data) {
    (void)user_data; // Unused parameter
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
