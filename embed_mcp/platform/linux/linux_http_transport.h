#ifndef LINUX_HTTP_TRANSPORT_H
#define LINUX_HTTP_TRANSPORT_H

#include "../../hal/platform_hal.h"
#include <stddef.h>
#include <stdbool.h>

// Linux HTTP传输接口 - 供HAL层调用

// 注意：传输层现在直接使用mongoose，这些函数已废弃
// int linux_http_init(const mcp_hal_http_config_t* config);

/**
 * 启动HTTP服务器
 * @return 0成功，-1失败
 */
int linux_http_start(void);

/**
 * 停止HTTP服务器
 * @return 0成功，-1失败
 */
int linux_http_stop(void);

/**
 * 发送HTTP响应数据
 * @param data 数据指针
 * @param len 数据长度
 * @return 发送的字节数，-1失败
 */
int linux_http_send(const void* data, size_t len);

/**
 * 发送HTTP响应到指定连接
 * @param platform_connection 平台连接对象
 * @param response HTTP响应
 * @return 发送的字节数，-1失败
 */
// int linux_http_send_response(void* platform_connection, const mcp_hal_http_response_t* response);

/**
 * 接收HTTP请求数据
 * @param buffer 接收缓冲区
 * @param max_len 缓冲区最大长度
 * @return 接收的字节数，-1失败
 */
int linux_http_recv(void* buffer, size_t max_len);

/**
 * 轮询HTTP事件
 * @return 0成功，-1失败
 */
int linux_http_poll(void);

/**
 * 关闭HTTP连接
 * @return 0成功，-1失败
 */
int linux_http_close(void);

/**
 * 检查HTTP连接状态
 * @return true连接，false断开
 */
bool linux_http_is_connected(void);

/**
 * 清理HTTP资源
 */
void linux_http_cleanup(void);

#endif // LINUX_HTTP_TRANSPORT_H
