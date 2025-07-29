#include "embed_mcp.h"
#include <stdio.h>

// 数学工具处理函数
cJSON *add_handler(const cJSON *args) {
    cJSON *a = cJSON_GetObjectItem(args, "a");
    cJSON *b = cJSON_GetObjectItem(args, "b");
    if (!a || !b || !cJSON_IsNumber(a) || !cJSON_IsNumber(b)) return NULL;
    
    double result = a->valuedouble + b->valuedouble;
    
    cJSON *response = cJSON_CreateObject();
    cJSON *content = cJSON_CreateArray();
    cJSON *text_content = cJSON_CreateObject();
    cJSON_AddStringToObject(text_content, "type", "text");
    
    char result_text[256];
    snprintf(result_text, sizeof(result_text), "%.1f + %.1f = %.1f", 
             a->valuedouble, b->valuedouble, result);
    cJSON_AddStringToObject(text_content, "text", result_text);
    
    cJSON_AddItemToArray(content, text_content);
    cJSON_AddItemToObject(response, "content", content);
    return response;
}

// 文本工具处理函数
cJSON *greet_handler(const cJSON *args) {
    cJSON *name = cJSON_GetObjectItem(args, "name");
    if (!name || !cJSON_IsString(name)) return NULL;
    
    cJSON *response = cJSON_CreateObject();
    cJSON *content = cJSON_CreateArray();
    cJSON *text_content = cJSON_CreateObject();
    cJSON_AddStringToObject(text_content, "type", "text");
    
    char greeting[256];
    snprintf(greeting, sizeof(greeting), "Hello, %s! Welcome to EmbedMCP!", name->valuestring);
    cJSON_AddStringToObject(text_content, "text", greeting);
    
    cJSON_AddItemToArray(content, text_content);
    cJSON_AddItemToObject(response, "content", content);
    return response;
}

int main() {
    // 创建服务器 - 使用简化API
    embed_mcp_server_t *server = embed_mcp_create_simple("SimpleServer", "1.0.0");
    if (!server) {
        fprintf(stderr, "Failed to create server: %s\n", embed_mcp_get_error());
        return 1;
    }
    
    // 添加数学工具 - 自动生成数字参数schema
    embed_mcp_add_math_tool(server, "add", "Add two numbers", add_handler);
    
    // 添加文本工具 - 自动生成字符串参数schema
    embed_mcp_add_text_tool(server, "greet", "Greet a person by name", 
                           "name", "Name of the person to greet", greet_handler);
    
    // 运行HTTP服务器
    printf("Simple EmbedMCP Server starting on http://localhost:8080/mcp\n");
    printf("Try these tools: add, greet\n");
    
    int result = embed_mcp_run_http(server);
    
    // 清理
    embed_mcp_destroy(server);
    return result;
}
