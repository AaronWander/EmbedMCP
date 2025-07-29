#include "embed_mcp.h"

// 最简单的工具处理函数
cJSON *hello_handler(const cJSON *args) {
    (void)args; // 忽略参数
    
    cJSON *response = cJSON_CreateObject();
    cJSON *content = cJSON_CreateArray();
    cJSON *text_content = cJSON_CreateObject();
    
    cJSON_AddStringToObject(text_content, "type", "text");
    cJSON_AddStringToObject(text_content, "text", "Hello from EmbedMCP!");
    
    cJSON_AddItemToArray(content, text_content);
    cJSON_AddItemToObject(response, "content", content);
    
    return response;
}

int main() {
    // 10行代码创建MCP服务器
    embed_mcp_server_t *server = embed_mcp_create_simple("MinimalServer", "1.0.0");
    embed_mcp_add_tool(server, "hello", "Say hello", hello_handler);
    embed_mcp_run_http(server);
    embed_mcp_destroy(server);
    return 0;
}
