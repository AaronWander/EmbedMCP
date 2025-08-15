#include "platform_hal.h"
#include "hal_common.h"

#ifdef MCP_PLATFORM_LINUX

#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <stdint.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>

// Mongoose HAL实现 - mongoose就是我们的跨平台HAL层
// mongoose内部支持Linux/FreeRTOS/ESP32等15+平台，我们只需要封装统一接口
#include "../platform/linux/mongoose.h"

// 全局mongoose管理器 - 这就是我们的HAL核心
static struct mg_mgr g_mongoose_mgr;
static bool g_mongoose_initialized = false;

// Linux内存管理
static void* linux_mem_alloc(size_t size) {
    return malloc(size);
}

static void linux_mem_free(void* ptr) {
    free(ptr);
}

static void* linux_mem_realloc(void* ptr, size_t new_size) {
    return realloc(ptr, new_size);
}

static size_t linux_mem_get_free_size(void) {
    // 简化实现，实际可以读取/proc/meminfo
    return 1024 * 1024; // 返回1MB作为示例
}

// Linux线程管理
static int linux_thread_create(void** handle, void* (*func)(void*), void* arg, uint32_t stack_size) {
    pthread_t* thread = malloc(sizeof(pthread_t));
    if (!thread) return -1;
    
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    if (stack_size > 0) {
        pthread_attr_setstacksize(&attr, stack_size);
    }
    
    int result = pthread_create(thread, &attr, func, arg);
    pthread_attr_destroy(&attr);
    
    if (result == 0) {
        *handle = thread;
        return 0;
    }
    
    free(thread);
    return -1;
}

static int linux_thread_join(void* handle) {
    if (!handle) return -1;
    pthread_t* thread = (pthread_t*)handle;
    int result = pthread_join(*thread, NULL);
    free(thread);
    return result;
}

static void linux_thread_yield(void) {
    sched_yield();
}

static void linux_thread_sleep_ms(uint32_t ms) {
    usleep(ms * 1000);
}

static uint32_t linux_thread_get_id(void) {
    // Convert pthread_t to uint32_t safely
    pthread_t tid = pthread_self();
    return (uint32_t)(uintptr_t)tid;
}

// Linux同步原语
static int linux_mutex_create(void** mutex) {
    pthread_mutex_t* m = malloc(sizeof(pthread_mutex_t));
    if (!m) return -1;
    
    if (pthread_mutex_init(m, NULL) == 0) {
        *mutex = m;
        return 0;
    }
    
    free(m);
    return -1;
}

static int linux_mutex_lock(void* mutex) {
    return pthread_mutex_lock((pthread_mutex_t*)mutex);
}

static int linux_mutex_unlock(void* mutex) {
    return pthread_mutex_unlock((pthread_mutex_t*)mutex);
}

static int linux_mutex_destroy(void* mutex) {
    if (!mutex) return -1;
    int result = pthread_mutex_destroy((pthread_mutex_t*)mutex);
    free(mutex);
    return result;
}

// Linux时间函数
static uint32_t linux_get_tick_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint32_t)(tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

static uint64_t linux_get_time_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)(tv.tv_sec * 1000000 + tv.tv_usec);
}

static void linux_delay_ms(uint32_t ms) {
    usleep(ms * 1000);
}

static void linux_delay_us(uint32_t us) {
    usleep(us);
}

// mongoose事件处理器 - 将mongoose事件转换为HAL回调
static void hal_mongoose_event_handler(struct mg_connection *c, int ev, void *ev_data) {
    if (ev == MG_EV_HTTP_MSG) {
        struct mg_http_message *hm = (struct mg_http_message *)ev_data;
        mcp_hal_http_handler_t handler = (mcp_hal_http_handler_t)c->fn_data;
        void* user_data = c->mgr->userdata;

        if (handler) {
            // 转换mongoose请求到HAL请求
            mcp_hal_http_request_t hal_req = {
                .method = "POST",  // 简化处理，假设都是POST
                .uri = "/mcp",     // 简化处理，假设都是/mcp
                .body = hm->body.buf,
                .body_len = hm->body.len,
                .connection = (mcp_hal_connection_t)c
            };

            // 创建HAL响应
            mcp_hal_http_response_t hal_resp = {0};

            // 调用HAL处理器
            handler(&hal_req, &hal_resp, user_data);

            // 发送响应
            if (hal_resp.status_code > 0) {
                mg_http_reply(c, hal_resp.status_code,
                             hal_resp.headers ? hal_resp.headers : "Content-Type: application/json\r\n",
                             "%.*s", (int)hal_resp.body_len, hal_resp.body ? hal_resp.body : "");
            }
        }
    }
}

// HAL网络接口实现 - 基于mongoose
static mcp_hal_server_t linux_hal_http_listen(const char* url, mcp_hal_http_handler_t handler, void* user_data) {
    if (!g_mongoose_initialized) {
        mg_mgr_init(&g_mongoose_mgr);
        g_mongoose_initialized = true;
    }

    // 创建HTTP监听器，使用mongoose
    struct mg_connection* conn = mg_http_listen(&g_mongoose_mgr, url, hal_mongoose_event_handler, NULL);
    if (!conn) {
        return NULL;
    }

    // 保存用户回调和数据
    conn->fn_data = handler;
    conn->mgr->userdata = user_data;

    return (mcp_hal_server_t)conn;
}

static int linux_hal_http_reply(mcp_hal_connection_t conn, const mcp_hal_http_response_t* response) {
    struct mg_connection* c = (struct mg_connection*)conn;
    if (!c || !response) {
        return -1;
    }

    mg_http_reply(c, response->status_code,
                 response->headers ? response->headers : "Content-Type: application/json\r\n",
                 "%.*s", (int)response->body_len, response->body ? response->body : "");

    return (int)response->body_len;
}

static int linux_hal_poll(int timeout_ms) {
    if (!g_mongoose_initialized) {
        return -1;
    }

    mg_mgr_poll(&g_mongoose_mgr, timeout_ms);
    return 0;
}

static int linux_hal_server_stop(mcp_hal_server_t server) {
    struct mg_connection* conn = (struct mg_connection*)server;
    if (conn) {
        conn->is_closing = 1;
    }
    return 0;
}

// 注意：传输清理现在由传输层直接处理

// Linux平台初始化
static int linux_platform_init(void) {
    // Linux平台特定的初始化
    return 0;
}

static void linux_platform_cleanup(void) {
    // Linux平台特定的清理
}

// Linux平台能力
static const mcp_platform_capabilities_t linux_capabilities = {
    .has_dynamic_memory = true,
    .has_threading = true,
    .has_networking = true,
    .max_memory_kb = 1024 * 1024, // 1GB
    .max_connections = 100,
    .tick_frequency_hz = 1000
};

// Linux HAL实现
static const mcp_platform_hal_t linux_hal = {
    .platform_name = "Linux",
    .version = "1.0.0",
    .capabilities = linux_capabilities,
    
    .memory = {
        .alloc = linux_mem_alloc,
        .free = linux_mem_free,
        .realloc = linux_mem_realloc,
        .get_free_size = linux_mem_get_free_size
    },
    
    .thread = {
        .create = linux_thread_create,
        .join = linux_thread_join,
        .yield = linux_thread_yield,
        .sleep_ms = linux_thread_sleep_ms,
        .get_id = linux_thread_get_id
    },
    
    .sync = {
        .mutex_create = linux_mutex_create,
        .mutex_lock = linux_mutex_lock,
        .mutex_unlock = linux_mutex_unlock,
        .mutex_destroy = linux_mutex_destroy
    },
    
    .time = {
        .get_tick_ms = linux_get_tick_ms,
        .get_time_us = linux_get_time_us,
        .delay_ms = linux_delay_ms,
        .delay_us = linux_delay_us
    },
    
    .network = {
        // HTTP服务器接口 - 通用接口名称，当前使用mongoose实现
        .http_server_start = linux_hal_http_listen,
        .http_response_send = linux_hal_http_reply,
        .network_poll = linux_hal_poll,
        .http_server_stop = linux_hal_server_stop,

        // 底层网络接口 - 用于不支持高级HTTP库的平台
        .socket_create = NULL,  // 当前使用mongoose，不需要直接socket操作
        .socket_bind = NULL,
        .socket_send = NULL,
        .socket_recv = NULL,
        .socket_close = NULL
    },
    
    .init = linux_platform_init,
    .cleanup = linux_platform_cleanup
};

// 使用通用宏实现导出函数
HAL_IMPLEMENT_EXPORTS(linux_hal, linux_capabilities, linux_platform_init, linux_platform_cleanup)

#endif // MCP_PLATFORM_LINUX
