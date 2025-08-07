#include "platform_http_interface.h"
#include "../hal/platform_hal.h"
#include "../utils/logging.h"
#include <string.h>

// 前向声明各平台实现
extern const mcp_platform_http_interface_t linux_http_interface;

const mcp_platform_http_interface_t* mcp_platform_get_http_interface(void) {
    mcp_log_info("Platform Factory: Selecting HTTP interface for platform");

    // 统一使用 Linux HTTP 接口 (mongoose)
    // 适用于 Linux、嵌入式 Linux 和 macOS 开发环境
    mcp_log_info("Platform Factory: Selected Linux HTTP interface (mongoose)");
    return &linux_http_interface;
}

// 平台能力查询辅助函数
bool mcp_platform_supports_feature(const char* feature) {
    const mcp_platform_capabilities_t* caps = mcp_platform_get_capabilities();
    
    if (strcmp(feature, "networking") == 0) {
        return caps->has_networking;
    } else if (strcmp(feature, "threading") == 0) {
        return caps->has_threading;
    } else if (strcmp(feature, "dynamic_memory") == 0) {
        return caps->has_dynamic_memory;
    }
    
    return false;
}

// 获取推荐的配置参数
void mcp_platform_get_recommended_config(mcp_transport_config_t* config) {
    const mcp_platform_capabilities_t* caps = mcp_platform_get_capabilities();
    
    if (!config) return;
    
    // 根据平台能力调整配置
    if (caps->max_memory_kb < 512) {
        // 低内存环境
        config->max_message_size = 4096;
        config->max_connections = 2;
        config->config.http.max_request_size = 8192;
    } else if (caps->max_memory_kb < 2048) {
        // 中等内存环境
        config->max_message_size = 16384;
        config->max_connections = 5;
        config->config.http.max_request_size = 32768;
    } else {
        // 充足内存环境
        config->max_message_size = 65536;
        config->max_connections = 20;
        config->config.http.max_request_size = 131072;
    }
    
    mcp_log_debug("Platform Factory: Recommended config - max_memory: %u KB, "
                  "max_connections: %zu, max_message_size: %zu",
                  caps->max_memory_kb, config->max_connections, config->max_message_size);
}
