#include "embed_mcp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

// 简洁的工具处理函数
cJSON *add_handler(const cJSON *args) {
    cJSON *a = cJSON_GetObjectItem(args, "a");
    cJSON *b = cJSON_GetObjectItem(args, "b");
    if (!a || !b || !cJSON_IsNumber(a) || !cJSON_IsNumber(b)) return NULL;

    double result_value = a->valuedouble + b->valuedouble;

    // 创建符合MCP规范的响应格式
    cJSON *response = cJSON_CreateObject();
    cJSON *content = cJSON_CreateArray();

    cJSON *text_content = cJSON_CreateObject();
    cJSON_AddStringToObject(text_content, "type", "text");

    char result_text[256];
    snprintf(result_text, sizeof(result_text), "%.1f + %.1f = %.1f",
             a->valuedouble, b->valuedouble, result_value);
    cJSON_AddStringToObject(text_content, "text", result_text);

    cJSON_AddItemToArray(content, text_content);
    cJSON_AddItemToObject(response, "content", content);

    return response;
}

cJSON *subtract_handler(const cJSON *args) {
    cJSON *a = cJSON_GetObjectItem(args, "a");
    cJSON *b = cJSON_GetObjectItem(args, "b");
    if (!a || !b || !cJSON_IsNumber(a) || !cJSON_IsNumber(b)) return NULL;

    double result_value = a->valuedouble - b->valuedouble;

    cJSON *response = cJSON_CreateObject();
    cJSON *content = cJSON_CreateArray();

    cJSON *text_content = cJSON_CreateObject();
    cJSON_AddStringToObject(text_content, "type", "text");

    char result_text[256];
    snprintf(result_text, sizeof(result_text), "%.1f - %.1f = %.1f",
             a->valuedouble, b->valuedouble, result_value);
    cJSON_AddStringToObject(text_content, "text", result_text);

    cJSON_AddItemToArray(content, text_content);
    cJSON_AddItemToObject(response, "content", content);

    return response;
}

cJSON *multiply_handler(const cJSON *args) {
    cJSON *a = cJSON_GetObjectItem(args, "a");
    cJSON *b = cJSON_GetObjectItem(args, "b");
    if (!a || !b || !cJSON_IsNumber(a) || !cJSON_IsNumber(b)) return NULL;

    double result_value = a->valuedouble * b->valuedouble;

    cJSON *response = cJSON_CreateObject();
    cJSON *content = cJSON_CreateArray();

    cJSON *text_content = cJSON_CreateObject();
    cJSON_AddStringToObject(text_content, "type", "text");

    char result_text[256];
    snprintf(result_text, sizeof(result_text), "%.1f × %.1f = %.1f",
             a->valuedouble, b->valuedouble, result_value);
    cJSON_AddStringToObject(text_content, "text", result_text);

    cJSON_AddItemToArray(content, text_content);
    cJSON_AddItemToObject(response, "content", content);

    return response;
}

cJSON *divide_handler(const cJSON *args) {
    cJSON *a = cJSON_GetObjectItem(args, "a");
    cJSON *b = cJSON_GetObjectItem(args, "b");
    if (!a || !b || !cJSON_IsNumber(a) || !cJSON_IsNumber(b)) return NULL;

    if (b->valuedouble == 0) {
        // 处理除零错误
        cJSON *response = cJSON_CreateObject();
        cJSON *content = cJSON_CreateArray();

        cJSON *text_content = cJSON_CreateObject();
        cJSON_AddStringToObject(text_content, "type", "text");
        cJSON_AddStringToObject(text_content, "text", "Error: Division by zero is not allowed");

        cJSON_AddItemToArray(content, text_content);
        cJSON_AddItemToObject(response, "content", content);

        return response;
    }

    double result_value = a->valuedouble / b->valuedouble;

    cJSON *response = cJSON_CreateObject();
    cJSON *content = cJSON_CreateArray();

    cJSON *text_content = cJSON_CreateObject();
    cJSON_AddStringToObject(text_content, "type", "text");

    char result_text[256];
    snprintf(result_text, sizeof(result_text), "%.1f ÷ %.1f = %.1f",
             a->valuedouble, b->valuedouble, result_value);
    cJSON_AddStringToObject(text_content, "text", result_text);

    cJSON_AddItemToArray(content, text_content);
    cJSON_AddItemToObject(response, "content", content);

    return response;
}

cJSON *weather_handler(const cJSON *args) {
    cJSON *city = cJSON_GetObjectItem(args, "city");
    if (!city || !cJSON_IsString(city)) return NULL;

    const char *city_name = city->valuestring;

    // 只支持济南
    if (strcmp(city_name, "济南") != 0 && strcmp(city_name, "jinan") != 0 &&
        strcmp(city_name, "Jinan") != 0 && strcmp(city_name, "JINAN") != 0) {
        cJSON *response = cJSON_CreateObject();
        cJSON *content = cJSON_CreateArray();

        cJSON *text_content = cJSON_CreateObject();
        cJSON_AddStringToObject(text_content, "type", "text");
        cJSON_AddStringToObject(text_content, "text", "抱歉，目前只支持查询济南的天气信息。");

        cJSON_AddItemToArray(content, text_content);
        cJSON_AddItemToObject(response, "content", content);

        return response;
    }

    // 返回济南天气信息
    cJSON *response = cJSON_CreateObject();
    cJSON *content = cJSON_CreateArray();

    cJSON *text_content = cJSON_CreateObject();
    cJSON_AddStringToObject(text_content, "type", "text");

    const char *weather_info =
        "🌤️ 济南天气预报\n\n"
        "Tonight:\n"
        "温度: 59°F\n"
        "风: 2 to 10 mph S\n"
        "预报: Clear, with a low around 59. South wind 2 to 10 mph, with gusts as high as 18 mph.\n\n"
        "…………………………\n\n"
        "Thursday Night:\n"
        "温度: 57°F\n"
        "风: 5 to 10 mph SSW\n"
        "预报: Clear, with a low around 57. South southwest wind 5 to 10 mph, with gusts as high as 20 mph.";

    cJSON_AddStringToObject(text_content, "text", weather_info);

    cJSON_AddItemToArray(content, text_content);
    cJSON_AddItemToObject(response, "content", content);

    return response;
}

void print_usage(const char *program_name) {
    printf("Usage: %s [OPTIONS]\n", program_name);
    printf("Options:\n");
    printf("  -t, --transport TYPE    Transport type (stdio|http) [default: stdio]\n");
    printf("  -p, --port PORT         HTTP port [default: 8080]\n");
    printf("  -b, --bind HOST         HTTP bind address [default: 0.0.0.0]\n");
    printf("  -e, --endpoint PATH     HTTP endpoint path [default: /mcp] (Note: currently fixed to /mcp)\n");
    printf("  -d, --debug             Enable debug logging\n");
    printf("  -h, --help              Show this help message\n");
    printf("\nExamples:\n");
    printf("  %s                      # STDIO transport\n", program_name);
    printf("  %s -t http -p 9943      # HTTP on port 9943\n", program_name);
    printf("  %s -t http -e /api/mcp  # HTTP with custom endpoint (planned feature)\n", program_name);
}

int main(int argc, char *argv[]) {
    // 解析命令行参数
    const char *transport_type = "stdio";
    int port = 8080;
    const char *bind_address = "0.0.0.0";
    const char *endpoint_path = "/mcp";
    int debug = 0;

    static struct option long_options[] = {
        {"transport", required_argument, 0, 't'},
        {"port", required_argument, 0, 'p'},
        {"bind", required_argument, 0, 'b'},
        {"endpoint", required_argument, 0, 'e'},
        {"debug", no_argument, 0, 'd'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    int c;
    while ((c = getopt_long(argc, argv, "t:p:b:e:dh", long_options, NULL)) != -1) {
        switch (c) {
            case 't': transport_type = optarg; break;
            case 'p': port = atoi(optarg); break;
            case 'b': bind_address = optarg; break;
            case 'e': endpoint_path = optarg; break;
            case 'd': debug = 1; break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    // 创建服务器配置
    embed_mcp_config_t config = embed_mcp_config_default("EmbedMCP", "1.0.0");
    config.host = bind_address;
    config.port = port;
    config.path = endpoint_path;
    config.debug = debug;

    // 创建服务器
    embed_mcp_server_t *server = embed_mcp_create(&config);
    if (!server) {
        fprintf(stderr, "Failed to create server: %s\n", embed_mcp_get_error());
        return 1;
    }

    // 添加工具 - 使用便捷函数自动生成Schema
    embed_mcp_add_math_tool(server, "add", "Add two numbers", add_handler);
    embed_mcp_add_math_tool(server, "subtract", "Subtract two numbers", subtract_handler);
    embed_mcp_add_math_tool(server, "multiply", "Multiply two numbers", multiply_handler);
    embed_mcp_add_math_tool(server, "divide", "Divide two numbers", divide_handler);
    embed_mcp_add_text_tool(server, "weather", "Get weather information for a city",
                           "city", "Name of the city to get weather for (currently supports: 济南)",
                           weather_handler);

    // 运行服务器
    printf("EmbedMCP Server starting with %s transport...\n", transport_type);
    int result;
    if (strcmp(transport_type, "http") == 0) {
        result = embed_mcp_run_http(server);
    } else {
        result = embed_mcp_run_stdio(server);
    }

    // 清理
    embed_mcp_destroy(server);
    return result;
}


