#ifndef LINUX_HTTP_TRANSPORT_H
#define LINUX_HTTP_TRANSPORT_H

#include "../../hal/platform_hal.h"
#include <stddef.h>
#include <stdbool.h>

// Linux HTTP transport interface - called by HAL layer

// Note: Transport layer now uses mongoose directly, these functions are deprecated
// int linux_http_init(const mcp_hal_http_config_t* config);

/**
 * Start HTTP server
 * @return 0 on success, -1 on failure
 */
int linux_http_start(void);

/**
 * Stop HTTP server
 * @return 0 on success, -1 on failure
 */
int linux_http_stop(void);

/**
 * Send HTTP response data
 * @param data Data pointer
 * @param len Data length
 * @return Number of bytes sent, -1 on failure
 */
int linux_http_send(const void* data, size_t len);

/**
 * Send HTTP response to specific connection
 * @param platform_connection Platform connection object
 * @param response HTTP response
 * @return Number of bytes sent, -1 on failure
 */
// int linux_http_send_response(void* platform_connection, const mcp_hal_http_response_t* response);

/**
 * Receive HTTP request data
 * @param buffer Receive buffer
 * @param max_len Maximum buffer length
 * @return Number of bytes received, -1 on failure
 */
int linux_http_recv(void* buffer, size_t max_len);

/**
 * Poll HTTP events
 * @return 0 on success, -1 on failure
 */
int linux_http_poll(void);

/**
 * Close HTTP connection
 * @return 0 on success, -1 on failure
 */
int linux_http_close(void);

/**
 * Check HTTP connection status
 * @return true if connected, false if disconnected
 */
bool linux_http_is_connected(void);

/**
 * Cleanup HTTP resources
 */
void linux_http_cleanup(void);

#endif // LINUX_HTTP_TRANSPORT_H
