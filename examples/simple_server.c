#include "embed_mcp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

// 数组求和工具
cJSON *sum_array_handler(const cJSON *args) {
    cJSON *numbers = cJSON_GetObjectItem(args, "numbers");
    if (!numbers || !cJSON_IsArray(numbers)) return NULL;

    double total = 0.0;
    int count = cJSON_GetArraySize(numbers);

    for (int i = 0; i < count; i++) {
        cJSON *item = cJSON_GetArrayItem(numbers, i);
        if (cJSON_IsNumber(item)) {
            total += item->valuedouble;
        }
    }

    cJSON *response = cJSON_CreateObject();
    cJSON *content = cJSON_CreateArray();
    cJSON *text_content = cJSON_CreateObject();
    cJSON_AddStringToObject(text_content, "type", "text");

    char result_text[512];
    snprintf(result_text, sizeof(result_text),
             "Sum of %d numbers: %.2f (Average: %.2f)",
             count, total, count > 0 ? total / count : 0.0);
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
    // 创建服务器
    embed_mcp_server_t *server = embed_mcp_create_simple("AdvancedServer", "1.0.0");
    if (!server) {
        fprintf(stderr, "Failed to create server: %s\n", embed_mcp_get_error());
        return 1;
    }

    // 示例1: 简单的数学工具（两个数字参数）
    mcp_param_desc_t add_params[] = {
        MCP_PARAM_DOUBLE_DEF("a", "First number to add", 1),
        MCP_PARAM_DOUBLE_DEF("b", "Second number to add", 1)
    };

    mcp_output_desc_t add_output = {
        "Addition result with formatted text",
        "{\"type\":\"object\",\"properties\":{\"content\":{\"type\":\"array\",\"items\":{\"type\":\"object\"}}}}"
    };

    embed_mcp_add_tool_with_params(server, "add", "Add two numbers together",
                                  add_params, 2, &add_output, add_handler);

    // 示例2: 数组参数工具（不定数量的数字）
    mcp_param_desc_t sum_params[] = {
        MCP_PARAM_ARRAY_DOUBLE_DEF("numbers", "Array of numbers to sum", "A number to include in the sum", 1)
    };

    mcp_output_desc_t sum_output = {
        "Sum result with statistics",
        "{\"type\":\"object\",\"properties\":{\"content\":{\"type\":\"array\",\"description\":\"Result with sum, count and average\"}}}"
    };

    embed_mcp_add_tool_with_params(server, "sum_array", "Calculate sum of an array of numbers",
                                  sum_params, 1, &sum_output, sum_array_handler);

    // 示例3: 使用旧的简化API
    embed_mcp_add_text_tool(server, "greet", "Greet a person by name",
                           "name", "Name of the person to greet", greet_handler);

    // 运行HTTP服务器
    printf("Advanced EmbedMCP Server starting on http://localhost:8080/mcp\n");
    printf("Available tools:\n");
    printf("  - add: Add two numbers (a, b)\n");
    printf("  - sum_array: Sum an array of numbers ([1,2,3,...])\n");
    printf("  - greet: Greet a person (name)\n");

    int result = embed_mcp_run_http(server);

    // 清理
    embed_mcp_destroy(server);
    return result;
}
