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

// Linux传输层（简化实现）
static int linux_transport_init(mcp_hal_transport_type_t type, void* config) {
    // 传输层初始化由现有的transport模块处理
    // 这里只是HAL接口的占位符
    (void)type;
    (void)config;
    return 0;
}

static int linux_transport_send(const void* data, size_t len) {
    // 实际发送由transport模块处理
    (void)data;
    (void)len;
    return (int)len;
}

static int linux_transport_recv(void* buffer, size_t max_len) {
    // 实际接收由transport模块处理
    (void)buffer;
    (void)max_len;
    return 0;
}

static int linux_transport_poll(void) {
    return 0;
}

static int linux_transport_close(void) {
    return 0;
}

static bool linux_transport_is_connected(void) {
    return true;
}

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
    
    .transport = {
        .init = linux_transport_init,
        .send = linux_transport_send,
        .recv = linux_transport_recv,
        .poll = linux_transport_poll,
        .close = linux_transport_close,
        .is_connected = linux_transport_is_connected
    },
    
    .init = linux_platform_init,
    .cleanup = linux_platform_cleanup
};

// 使用通用宏实现导出函数
HAL_IMPLEMENT_EXPORTS(linux_hal, linux_capabilities, linux_platform_init, linux_platform_cleanup)

#endif // MCP_PLATFORM_LINUX
