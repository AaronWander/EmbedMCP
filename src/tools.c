#include "mcp_server.h"

// Implementation of the "add" tool
int mcp_tool_add(const cJSON *params, cJSON **result) {
    if (!params || !result) return -1;
    
    mcp_debug_print("Executing add tool\n");
    
    // Extract num1 parameter
    cJSON *num1_json = cJSON_GetObjectItem(params, "num1");
    if (!num1_json || !cJSON_IsNumber(num1_json)) {
        mcp_debug_print("Invalid or missing num1 parameter\n");
        return -1;
    }
    
    // Extract num2 parameter
    cJSON *num2_json = cJSON_GetObjectItem(params, "num2");
    if (!num2_json || !cJSON_IsNumber(num2_json)) {
        mcp_debug_print("Invalid or missing num2 parameter\n");
        return -1;
    }
    
    // Get the numeric values
    double num1 = num1_json->valuedouble;
    double num2 = num2_json->valuedouble;
    
    // Calculate the sum
    double sum = num1 + num2;
    
    mcp_debug_print("Adding %.2f + %.2f = %.2f\n", num1, num2, sum);
    
    // Create result object
    *result = cJSON_CreateObject();
    if (!*result) return -1;
    
    cJSON_AddNumberToObject(*result, "num1", num1);
    cJSON_AddNumberToObject(*result, "num2", num2);
    cJSON_AddNumberToObject(*result, "sum", sum);
    
    // Also add a human-readable message
    char message[256];
    snprintf(message, sizeof(message), "%.2f + %.2f = %.2f", num1, num2, sum);
    cJSON_AddStringToObject(*result, "message", message);
    
    return 0;
}
