#include "platform_hal.h"
#include "hal_common.h"

#ifdef MCP_PLATFORM_FREERTOS

// FreeRTOS头文件（在实际项目中需要包含）
// #include "FreeRTOS.h"
// #include "task.h"
// #include "semphr.h"
// #include "queue.h"

#include <stdio.h>
#include <string.h>

// 为了编译通过，这里使用模拟的FreeRTOS类型和函数
// 在实际RTOS项目中，这些会被真正的FreeRTOS API替代

typedef void* TaskHandle_t;
typedef void* SemaphoreHandle_t;
typedef int BaseType_t;
typedef unsigned int TickType_t;

#define pdPASS 1
#define pdFAIL 0
#define portMAX_DELAY 0xFFFFFFFF
#define configMINIMAL_STACK_SIZE 128
#define tskIDLE_PRIORITY 0

// 模拟的FreeRTOS函数（实际项目中会使用真正的API）
static BaseType_t xTaskCreate(void* func, const char* name, uint32_t stack, void* param, int priority, TaskHandle_t* handle) {
    (void)func; (void)name; (void)stack; (void)param; (void)priority; (void)handle;
    return pdPASS; // 模拟成功
}

static void* pvPortMalloc(size_t size) { return malloc(size); }
static void vPortFree(void* ptr) { free(ptr); }
static size_t xPortGetFreeHeapSize(void) { return 32 * 1024; } // 32KB
static void vTaskDelay(TickType_t ticks) { (void)ticks; }
static void taskYIELD(void) { }
static TaskHandle_t xTaskGetCurrentTaskHandle(void) { return (TaskHandle_t)0x1234; }
static TickType_t xTaskGetTickCount(void) { return 1000; }

static SemaphoreHandle_t xSemaphoreCreateMutex(void) { return (SemaphoreHandle_t)malloc(4); }
static BaseType_t xSemaphoreTake(SemaphoreHandle_t sem, TickType_t timeout) { (void)sem; (void)timeout; return pdPASS; }
static BaseType_t xSemaphoreGive(SemaphoreHandle_t sem) { (void)sem; return pdPASS; }
static void vSemaphoreDelete(SemaphoreHandle_t sem) { free(sem); }

#define portTICK_PERIOD_MS 1

// FreeRTOS内存管理
static void* freertos_mem_alloc(size_t size) {
    return pvPortMalloc(size);
}

static void freertos_mem_free(void* ptr) {
    vPortFree(ptr);
}

static void* freertos_mem_realloc(void* ptr, size_t new_size) {
    // FreeRTOS没有realloc，手动实现
    if (!ptr) return pvPortMalloc(new_size);
    
    void* new_ptr = pvPortMalloc(new_size);
    if (new_ptr && ptr) {
        // 简化：假设新大小总是更大，实际需要记录原大小
        memcpy(new_ptr, ptr, new_size);
        vPortFree(ptr);
    }
    return new_ptr;
}

static size_t freertos_mem_get_free_size(void) {
    return xPortGetFreeHeapSize();
}

// FreeRTOS任务管理
typedef struct {
    TaskHandle_t handle;
    void* (*func)(void*);
    void* arg;
} freertos_thread_wrapper_t;

static void freertos_thread_wrapper_func(void* param) {
    freertos_thread_wrapper_t* wrapper = (freertos_thread_wrapper_t*)param;
    wrapper->func(wrapper->arg);
    vPortFree(wrapper);
    // vTaskDelete(NULL); // 删除自己
}

static int freertos_thread_create(void** handle, void* (*func)(void*), void* arg, uint32_t stack_size) {
    freertos_thread_wrapper_t* wrapper = pvPortMalloc(sizeof(freertos_thread_wrapper_t));
    if (!wrapper) return -1;
    
    wrapper->func = func;
    wrapper->arg = arg;
    
    if (stack_size == 0) {
        stack_size = configMINIMAL_STACK_SIZE * 2;
    }
    
    BaseType_t result = xTaskCreate(
        freertos_thread_wrapper_func,
        "MCP_Task",
        stack_size,
        wrapper,
        tskIDLE_PRIORITY + 1,
        &wrapper->handle
    );
    
    if (result == pdPASS) {
        *handle = wrapper;
        return 0;
    }
    
    vPortFree(wrapper);
    return -1;
}

static int freertos_thread_join(void* handle) {
    // FreeRTOS没有join概念，任务会自动清理
    (void)handle;
    return 0;
}

static void freertos_thread_yield(void) {
    taskYIELD();
}

static void freertos_thread_sleep_ms(uint32_t ms) {
    vTaskDelay(ms / portTICK_PERIOD_MS);
}

static uint32_t freertos_thread_get_id(void) {
    return (uint32_t)xTaskGetCurrentTaskHandle();
}

// FreeRTOS同步原语
static int freertos_mutex_create(void** mutex) {
    SemaphoreHandle_t sem = xSemaphoreCreateMutex();
    if (sem) {
        *mutex = sem;
        return 0;
    }
    return -1;
}

static int freertos_mutex_lock(void* mutex) {
    return (xSemaphoreTake((SemaphoreHandle_t)mutex, portMAX_DELAY) == pdPASS) ? 0 : -1;
}

static int freertos_mutex_unlock(void* mutex) {
    return (xSemaphoreGive((SemaphoreHandle_t)mutex) == pdPASS) ? 0 : -1;
}

static int freertos_mutex_destroy(void* mutex) {
    if (mutex) {
        vSemaphoreDelete((SemaphoreHandle_t)mutex);
        return 0;
    }
    return -1;
}

// FreeRTOS时间函数
static uint32_t freertos_get_tick_ms(void) {
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

static uint64_t freertos_get_time_us(void) {
    return (uint64_t)xTaskGetTickCount() * portTICK_PERIOD_MS * 1000;
}

static void freertos_delay_ms(uint32_t ms) {
    vTaskDelay(ms / portTICK_PERIOD_MS);
}

static void freertos_delay_us(uint32_t us) {
    // FreeRTOS通常不支持微秒级延时
    if (us >= 1000) {
        vTaskDelay((us / 1000) / portTICK_PERIOD_MS);
    }
    // 对于小于1ms的延时，可能需要忙等待或使用高精度定时器
}

// FreeRTOS传输层（UART示例）
static int freertos_transport_init(mcp_hal_transport_type_t type, void* config) {
    if (type == MCP_HAL_TRANSPORT_UART) {
        // 初始化UART硬件
        // uart_init(config);
        return 0;
    }
    return -1;
}

static int freertos_transport_send(const void* data, size_t len) {
    // 通过UART发送数据
    // return uart_send(data, len);
    (void)data;
    return (int)len; // 模拟成功
}

static int freertos_transport_recv(void* buffer, size_t max_len) {
    // 从UART接收数据
    // return uart_recv(buffer, max_len);
    (void)buffer;
    (void)max_len;
    return 0; // 模拟无数据
}

static int freertos_transport_poll(void) {
    // 检查是否有数据可读
    return 0;
}

static int freertos_transport_close(void) {
    return 0;
}

static bool freertos_transport_is_connected(void) {
    return true; // UART总是"连接"的
}

// FreeRTOS平台初始化
static int freertos_platform_init(void) {
    return 0;
}

static void freertos_platform_cleanup(void) {
    // FreeRTOS清理
}

// FreeRTOS平台能力
static const mcp_platform_capabilities_t freertos_capabilities = {
    .has_dynamic_memory = true,
    .has_threading = true,
    .has_networking = false,  // 取决于硬件和网络栈
    .max_memory_kb = 64,      // 典型的MCU内存
    .max_connections = 4,     // 资源受限
    .tick_frequency_hz = 1000
};

// FreeRTOS HAL实现
static const mcp_platform_hal_t freertos_hal = {
    .platform_name = "FreeRTOS",
    .version = "10.4.0",
    .capabilities = freertos_capabilities,
    
    .memory = {
        .alloc = freertos_mem_alloc,
        .free = freertos_mem_free,
        .realloc = freertos_mem_realloc,
        .get_free_size = freertos_mem_get_free_size
    },
    
    .thread = {
        .create = freertos_thread_create,
        .join = freertos_thread_join,
        .yield = freertos_thread_yield,
        .sleep_ms = freertos_thread_sleep_ms,
        .get_id = freertos_thread_get_id
    },
    
    .sync = {
        .mutex_create = freertos_mutex_create,
        .mutex_lock = freertos_mutex_lock,
        .mutex_unlock = freertos_mutex_unlock,
        .mutex_destroy = freertos_mutex_destroy
    },
    
    .time = {
        .get_tick_ms = freertos_get_tick_ms,
        .get_time_us = freertos_get_time_us,
        .delay_ms = freertos_delay_ms,
        .delay_us = freertos_delay_us
    },
    
    .transport = {
        .init = freertos_transport_init,
        .send = freertos_transport_send,
        .recv = freertos_transport_recv,
        .poll = freertos_transport_poll,
        .close = freertos_transport_close,
        .is_connected = freertos_transport_is_connected
    },
    
    .init = freertos_platform_init,
    .cleanup = freertos_platform_cleanup
};

// 使用通用宏实现导出函数
HAL_IMPLEMENT_EXPORTS(freertos_hal, freertos_capabilities, freertos_platform_init, freertos_platform_cleanup)

#endif // MCP_PLATFORM_FREERTOS
