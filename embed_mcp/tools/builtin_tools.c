#include "tools/builtin_tools.h"
#include "utils/logging.h"
#include "cjson/cJSON.h"
#include "utils/base64.h"
#include "utils/uuid4.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>

// Base64编码实现
cJSON *mcp_builtin_tool_base64_encode_execute(const cJSON *parameters, void *user_data) {
    (void)user_data;

    if (!parameters) return NULL;

    cJSON *text_param = cJSON_GetObjectItem(parameters, "text");
    if (!text_param || !cJSON_IsString(text_param)) {
        return cJSON_CreateString("Error: 'text' parameter is required and must be a string");
    }

    const char *input = cJSON_GetStringValue(text_param);
    size_t input_len = strlen(input);

    size_t output_len = base64_encoded_size(input_len) + 1;
    char *output = malloc(output_len);
    if (!output) {
        return cJSON_CreateString("Error: Memory allocation failed");
    }

    size_t encoded_len = base64_encode((const unsigned char*)input, input_len, output, output_len);
    if (encoded_len == 0) {
        free(output);
        return cJSON_CreateString("Error: Base64 encoding failed");
    }

    cJSON *result = cJSON_CreateString(output);
    free(output);
    return result;
}

// Base64解码实现
cJSON *mcp_builtin_tool_base64_decode_execute(const cJSON *parameters, void *user_data) {
    (void)user_data;

    if (!parameters) return NULL;

    cJSON *text_param = cJSON_GetObjectItem(parameters, "text");
    if (!text_param || !cJSON_IsString(text_param)) {
        return cJSON_CreateString("Error: 'text' parameter is required and must be a string");
    }

    const char *input = cJSON_GetStringValue(text_param);
    size_t input_len = strlen(input);

    size_t output_len = base64_decoded_size(input, input_len) + 1;
    unsigned char *output = malloc(output_len);
    if (!output) {
        return cJSON_CreateString("Error: Memory allocation failed");
    }

    size_t decoded_len = base64_decode(input, input_len, output, output_len);
    if (decoded_len == 0) {
        free(output);
        return cJSON_CreateString("Error: Base64 decoding failed");
    }

    output[decoded_len] = '\0'; // 确保字符串结束
    cJSON *result = cJSON_CreateString((char*)output);
    free(output);
    return result;
}

// UUID生成实现
cJSON *mcp_builtin_tool_uuid_execute(const cJSON *parameters, void *user_data) {
    (void)parameters;
    (void)user_data;

    static UUID4_STATE_T state = 0;
    static int initialized = 0;

    if (!initialized) {
        uuid4_seed(&state);
        initialized = 1;
    }

    UUID4_T uuid;
    uuid4_gen(&state, &uuid);

    char uuid_str[UUID4_STR_BUFFER_SIZE];
    if (!uuid4_to_s(uuid, uuid_str, UUID4_STR_BUFFER_SIZE)) {
        return cJSON_CreateString("Error: UUID generation failed");
    }

    return cJSON_CreateString(uuid_str);
}

// 时间戳实现
cJSON *mcp_builtin_tool_timestamp_execute(const cJSON *parameters, void *user_data) {
    (void)parameters;
    (void)user_data;

    time_t timestamp = time(NULL);
    return cJSON_CreateNumber((double)timestamp);
}

// 简化的注册函数
int mcp_builtin_tools_register_all(mcp_tool_registry_t *registry) {
    (void)registry; // 暂时不实际注册，保持最小化

    mcp_log(MCP_LOG_LEVEL_DEBUG, "Built-in tool functions available (not auto-registered)");

    return 0; // Success
}


