#include "tools/builtin_tools.h"
#include "utils/logging.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

// Built-in tool registration
int mcp_builtin_tools_register_all(mcp_tool_registry_t *registry) {
    if (!registry) return -1;
    
    int registered = 0;
    
    // Math tools
    mcp_tool_t *tools[] = {
        mcp_builtin_tool_create_add(),
        mcp_builtin_tool_create_subtract(),
        mcp_builtin_tool_create_multiply(),
        mcp_builtin_tool_create_divide(),
        mcp_builtin_tool_create_power(),
        mcp_builtin_tool_create_sqrt(),
        mcp_builtin_tool_create_abs(),
        mcp_builtin_tool_create_round(),
        mcp_builtin_tool_create_floor(),
        mcp_builtin_tool_create_ceil(),
        
        // Text tools
        mcp_builtin_tool_create_text_length(),
        mcp_builtin_tool_create_text_upper(),
        mcp_builtin_tool_create_text_lower(),
        mcp_builtin_tool_create_text_trim(),
        mcp_builtin_tool_create_text_reverse(),
        
        // Utility tools
        mcp_builtin_tool_create_echo(),
        mcp_builtin_tool_create_timestamp(),
        mcp_builtin_tool_create_current_time(),
        
        NULL
    };
    
    for (int i = 0; tools[i]; i++) {
        if (mcp_tool_registry_register_tool(registry, tools[i]) == 0) {
            registered++;
        } else {
            mcp_tool_unref(tools[i]);
        }
    }
    
    mcp_log_info("Registered %d built-in tools", registered);
    return registered;
}

// Math tool implementations
cJSON *mcp_builtin_tool_add_execute(const cJSON *parameters, void *user_data) {
    (void)user_data;
    
    if (!mcp_builtin_validate_two_numbers(parameters, "a", "b")) {
        return mcp_tool_create_validation_error("Parameters 'a' and 'b' must be numbers");
    }
    
    double a = cJSON_GetObjectItem(parameters, "a")->valuedouble;
    double b = cJSON_GetObjectItem(parameters, "b")->valuedouble;
    double result = a + b;
    
    return mcp_builtin_create_math_result(a, b, result, "addition", "Successfully added two numbers");
}

cJSON *mcp_builtin_tool_subtract_execute(const cJSON *parameters, void *user_data) {
    (void)user_data;
    
    if (!mcp_builtin_validate_two_numbers(parameters, "a", "b")) {
        return mcp_tool_create_validation_error("Parameters 'a' and 'b' must be numbers");
    }
    
    double a = cJSON_GetObjectItem(parameters, "a")->valuedouble;
    double b = cJSON_GetObjectItem(parameters, "b")->valuedouble;
    double result = a - b;
    
    return mcp_builtin_create_math_result(a, b, result, "subtraction", "Successfully subtracted two numbers");
}

cJSON *mcp_builtin_tool_multiply_execute(const cJSON *parameters, void *user_data) {
    (void)user_data;
    
    if (!mcp_builtin_validate_two_numbers(parameters, "a", "b")) {
        return mcp_tool_create_validation_error("Parameters 'a' and 'b' must be numbers");
    }
    
    double a = cJSON_GetObjectItem(parameters, "a")->valuedouble;
    double b = cJSON_GetObjectItem(parameters, "b")->valuedouble;
    double result = a * b;
    
    return mcp_builtin_create_math_result(a, b, result, "multiplication", "Successfully multiplied two numbers");
}

cJSON *mcp_builtin_tool_divide_execute(const cJSON *parameters, void *user_data) {
    (void)user_data;
    
    if (!mcp_builtin_validate_two_numbers(parameters, "a", "b")) {
        return mcp_tool_create_validation_error("Parameters 'a' and 'b' must be numbers");
    }
    
    double a = cJSON_GetObjectItem(parameters, "a")->valuedouble;
    double b = cJSON_GetObjectItem(parameters, "b")->valuedouble;
    
    if (b == 0.0) {
        return mcp_tool_create_validation_error("Division by zero is not allowed");
    }
    
    double result = a / b;
    
    return mcp_builtin_create_math_result(a, b, result, "division", "Successfully divided two numbers");
}

cJSON *mcp_builtin_tool_sqrt_execute(const cJSON *parameters, void *user_data) {
    (void)user_data;
    
    if (!mcp_builtin_validate_single_number(parameters, "value")) {
        return mcp_tool_create_validation_error("Parameter 'value' must be a number");
    }
    
    double value = cJSON_GetObjectItem(parameters, "value")->valuedouble;
    
    if (value < 0.0) {
        return mcp_tool_create_validation_error("Cannot calculate square root of negative number");
    }
    
    double result = sqrt(value);
    
    return mcp_builtin_create_number_result(result);
}

// Text tool implementations
cJSON *mcp_builtin_tool_text_length_execute(const cJSON *parameters, void *user_data) {
    (void)user_data;
    
    if (!mcp_builtin_validate_single_string(parameters, "text")) {
        return mcp_tool_create_validation_error("Parameter 'text' must be a string");
    }
    
    const char *text = cJSON_GetObjectItem(parameters, "text")->valuestring;
    double length = (double)strlen(text);
    
    return mcp_builtin_create_number_result(length);
}

cJSON *mcp_builtin_tool_text_upper_execute(const cJSON *parameters, void *user_data) {
    (void)user_data;
    
    if (!mcp_builtin_validate_single_string(parameters, "text")) {
        return mcp_tool_create_validation_error("Parameter 'text' must be a string");
    }
    
    const char *text = cJSON_GetObjectItem(parameters, "text")->valuestring;
    char *upper_text = strdup(text);
    if (!upper_text) {
        return mcp_tool_create_execution_error("Memory allocation failed");
    }
    
    for (char *p = upper_text; *p; p++) {
        if (*p >= 'a' && *p <= 'z') {
            *p = *p - 'a' + 'A';
        }
    }
    
    cJSON *result = mcp_builtin_create_string_result(upper_text);
    free(upper_text);
    
    return result;
}

cJSON *mcp_builtin_tool_text_lower_execute(const cJSON *parameters, void *user_data) {
    (void)user_data;
    
    if (!mcp_builtin_validate_single_string(parameters, "text")) {
        return mcp_tool_create_validation_error("Parameter 'text' must be a string");
    }
    
    const char *text = cJSON_GetObjectItem(parameters, "text")->valuestring;
    char *lower_text = strdup(text);
    if (!lower_text) {
        return mcp_tool_create_execution_error("Memory allocation failed");
    }
    
    for (char *p = lower_text; *p; p++) {
        if (*p >= 'A' && *p <= 'Z') {
            *p = *p - 'A' + 'a';
        }
    }
    
    cJSON *result = mcp_builtin_create_string_result(lower_text);
    free(lower_text);
    
    return result;
}

// Utility tool implementations
cJSON *mcp_builtin_tool_echo_execute(const cJSON *parameters, void *user_data) {
    (void)user_data;
    
    if (!mcp_builtin_validate_single_string(parameters, "message")) {
        return mcp_tool_create_validation_error("Parameter 'message' must be a string");
    }
    
    const char *message = cJSON_GetObjectItem(parameters, "message")->valuestring;
    
    return mcp_builtin_create_string_result(message);
}

cJSON *mcp_builtin_tool_timestamp_execute(const cJSON *parameters, void *user_data) {
    (void)parameters;
    (void)user_data;
    
    time_t now = time(NULL);
    double timestamp = (double)now;
    
    return mcp_builtin_create_number_result(timestamp);
}

cJSON *mcp_builtin_tool_current_time_execute(const cJSON *parameters, void *user_data) {
    (void)parameters;
    (void)user_data;
    
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
    
    return mcp_builtin_create_string_result(time_str);
}

// Validation helpers
bool mcp_builtin_validate_two_numbers(const cJSON *parameters, const char *param1, const char *param2) {
    if (!parameters || !cJSON_IsObject(parameters)) return false;
    
    cJSON *p1 = cJSON_GetObjectItem(parameters, param1);
    cJSON *p2 = cJSON_GetObjectItem(parameters, param2);
    
    return p1 && cJSON_IsNumber(p1) && p2 && cJSON_IsNumber(p2);
}

bool mcp_builtin_validate_single_number(const cJSON *parameters, const char *param_name) {
    if (!parameters || !cJSON_IsObject(parameters)) return false;
    
    cJSON *param = cJSON_GetObjectItem(parameters, param_name);
    return param && cJSON_IsNumber(param);
}

bool mcp_builtin_validate_single_string(const cJSON *parameters, const char *param_name) {
    if (!parameters || !cJSON_IsObject(parameters)) return false;
    
    cJSON *param = cJSON_GetObjectItem(parameters, param_name);
    return param && cJSON_IsString(param);
}

// Result creation helpers
cJSON *mcp_builtin_create_number_result(double value) {
    cJSON *data = cJSON_CreateNumber(value);
    if (!data) return mcp_tool_create_execution_error("Failed to create number result");
    
    cJSON *result = mcp_tool_create_success_result(data);
    cJSON_Delete(data);
    
    return result;
}

cJSON *mcp_builtin_create_string_result(const char *value) {
    cJSON *data = cJSON_CreateString(value ? value : "");
    if (!data) return mcp_tool_create_execution_error("Failed to create string result");
    
    cJSON *result = mcp_tool_create_success_result(data);
    cJSON_Delete(data);
    
    return result;
}

cJSON *mcp_builtin_create_math_result(double num1, double num2, double result, 
                                     const char *operation, const char *message) {
    cJSON *data = cJSON_CreateObject();
    if (!data) return mcp_tool_create_execution_error("Failed to create result object");
    
    cJSON_AddNumberToObject(data, "operand1", num1);
    cJSON_AddNumberToObject(data, "operand2", num2);
    cJSON_AddNumberToObject(data, "result", result);
    cJSON_AddStringToObject(data, "operation", operation);
    if (message) {
        cJSON_AddStringToObject(data, "message", message);
    }
    
    cJSON *success_result = mcp_tool_create_success_result(data);
    cJSON_Delete(data);
    
    return success_result;
}

// Tool creation functions
mcp_tool_t *mcp_builtin_tool_create_add(void) {
    cJSON *schema = mcp_builtin_create_two_number_schema("a", "b");
    mcp_tool_t *tool = mcp_tool_create(
        MCP_BUILTIN_TOOL_ADD,
        "Add Numbers",
        "Add two numbers together",
        schema,
        mcp_builtin_tool_add_execute,
        NULL
    );

    if (tool) {
        mcp_tool_set_category(tool, MCP_TOOL_CATEGORY_MATH);
    }

    cJSON_Delete(schema);
    return tool;
}

mcp_tool_t *mcp_builtin_tool_create_subtract(void) {
    cJSON *schema = mcp_builtin_create_two_number_schema("a", "b");
    mcp_tool_t *tool = mcp_tool_create(
        MCP_BUILTIN_TOOL_SUBTRACT,
        "Subtract Numbers",
        "Subtract second number from first number",
        schema,
        mcp_builtin_tool_subtract_execute,
        NULL
    );

    if (tool) {
        mcp_tool_set_category(tool, MCP_TOOL_CATEGORY_MATH);
    }

    cJSON_Delete(schema);
    return tool;
}

mcp_tool_t *mcp_builtin_tool_create_multiply(void) {
    cJSON *schema = mcp_builtin_create_two_number_schema("a", "b");
    mcp_tool_t *tool = mcp_tool_create(
        MCP_BUILTIN_TOOL_MULTIPLY,
        "Multiply Numbers",
        "Multiply two numbers together",
        schema,
        mcp_builtin_tool_multiply_execute,
        NULL
    );

    if (tool) {
        mcp_tool_set_category(tool, MCP_TOOL_CATEGORY_MATH);
    }

    cJSON_Delete(schema);
    return tool;
}

mcp_tool_t *mcp_builtin_tool_create_divide(void) {
    cJSON *schema = mcp_builtin_create_two_number_schema("a", "b");
    mcp_tool_t *tool = mcp_tool_create(
        MCP_BUILTIN_TOOL_DIVIDE,
        "Divide Numbers",
        "Divide first number by second number",
        schema,
        mcp_builtin_tool_divide_execute,
        NULL
    );

    if (tool) {
        mcp_tool_set_category(tool, MCP_TOOL_CATEGORY_MATH);
    }

    cJSON_Delete(schema);
    return tool;
}

mcp_tool_t *mcp_builtin_tool_create_sqrt(void) {
    cJSON *schema = mcp_builtin_create_single_number_schema("value");
    mcp_tool_t *tool = mcp_tool_create(
        MCP_BUILTIN_TOOL_SQRT,
        "Square Root",
        "Calculate square root of a number",
        schema,
        mcp_builtin_tool_sqrt_execute,
        NULL
    );

    if (tool) {
        mcp_tool_set_category(tool, MCP_TOOL_CATEGORY_MATH);
    }

    cJSON_Delete(schema);
    return tool;
}

mcp_tool_t *mcp_builtin_tool_create_text_length(void) {
    cJSON *schema = mcp_builtin_create_single_string_schema("text");
    mcp_tool_t *tool = mcp_tool_create(
        MCP_BUILTIN_TOOL_TEXT_LENGTH,
        "Text Length",
        "Get the length of a text string",
        schema,
        mcp_builtin_tool_text_length_execute,
        NULL
    );

    if (tool) {
        mcp_tool_set_category(tool, MCP_TOOL_CATEGORY_TEXT);
    }

    cJSON_Delete(schema);
    return tool;
}

mcp_tool_t *mcp_builtin_tool_create_text_upper(void) {
    cJSON *schema = mcp_builtin_create_single_string_schema("text");
    mcp_tool_t *tool = mcp_tool_create(
        MCP_BUILTIN_TOOL_TEXT_UPPER,
        "Text to Uppercase",
        "Convert text to uppercase",
        schema,
        mcp_builtin_tool_text_upper_execute,
        NULL
    );

    if (tool) {
        mcp_tool_set_category(tool, MCP_TOOL_CATEGORY_TEXT);
    }

    cJSON_Delete(schema);
    return tool;
}

mcp_tool_t *mcp_builtin_tool_create_text_lower(void) {
    cJSON *schema = mcp_builtin_create_single_string_schema("text");
    mcp_tool_t *tool = mcp_tool_create(
        MCP_BUILTIN_TOOL_TEXT_LOWER,
        "Text to Lowercase",
        "Convert text to lowercase",
        schema,
        mcp_builtin_tool_text_lower_execute,
        NULL
    );

    if (tool) {
        mcp_tool_set_category(tool, MCP_TOOL_CATEGORY_TEXT);
    }

    cJSON_Delete(schema);
    return tool;
}

mcp_tool_t *mcp_builtin_tool_create_echo(void) {
    cJSON *schema = mcp_builtin_create_single_string_schema("message");
    mcp_tool_t *tool = mcp_tool_create(
        MCP_BUILTIN_TOOL_ECHO,
        "Echo",
        "Echo back the input message",
        schema,
        mcp_builtin_tool_echo_execute,
        NULL
    );

    if (tool) {
        mcp_tool_set_category(tool, MCP_TOOL_CATEGORY_UTILITY);
    }

    cJSON_Delete(schema);
    return tool;
}

mcp_tool_t *mcp_builtin_tool_create_timestamp(void) {
    cJSON *schema = cJSON_CreateObject(); // No parameters needed
    mcp_tool_t *tool = mcp_tool_create(
        MCP_BUILTIN_TOOL_TIMESTAMP,
        "Current Timestamp",
        "Get current Unix timestamp",
        schema,
        mcp_builtin_tool_timestamp_execute,
        NULL
    );

    if (tool) {
        mcp_tool_set_category(tool, MCP_TOOL_CATEGORY_UTILITY);
    }

    cJSON_Delete(schema);
    return tool;
}

mcp_tool_t *mcp_builtin_tool_create_current_time(void) {
    cJSON *schema = cJSON_CreateObject(); // No parameters needed
    mcp_tool_t *tool = mcp_tool_create(
        MCP_BUILTIN_TOOL_CURRENT_TIME,
        "Current Time",
        "Get current date and time as formatted string",
        schema,
        mcp_builtin_tool_current_time_execute,
        NULL
    );

    if (tool) {
        mcp_tool_set_category(tool, MCP_TOOL_CATEGORY_UTILITY);
    }

    cJSON_Delete(schema);
    return tool;
}

// Schema creation helpers
cJSON *mcp_builtin_create_two_number_schema(const char *param1_name, const char *param2_name) {
    cJSON *properties = cJSON_CreateObject();
    cJSON *required = cJSON_CreateArray();

    cJSON *param1 = mcp_tool_create_number_schema("First number", -1e308, 1e308);
    cJSON *param2 = mcp_tool_create_number_schema("Second number", -1e308, 1e308);

    cJSON_AddItemToObject(properties, param1_name, param1);
    cJSON_AddItemToObject(properties, param2_name, param2);

    cJSON_AddItemToArray(required, cJSON_CreateString(param1_name));
    cJSON_AddItemToArray(required, cJSON_CreateString(param2_name));

    cJSON *schema = mcp_tool_create_object_schema("Two number parameters", properties, required);

    cJSON_Delete(properties);
    cJSON_Delete(required);

    return schema;
}

cJSON *mcp_builtin_create_single_number_schema(const char *param_name) {
    cJSON *properties = cJSON_CreateObject();
    cJSON *required = cJSON_CreateArray();

    cJSON *param = mcp_tool_create_number_schema("Number value", -1e308, 1e308);
    cJSON_AddItemToObject(properties, param_name, param);
    cJSON_AddItemToArray(required, cJSON_CreateString(param_name));

    cJSON *schema = mcp_tool_create_object_schema("Single number parameter", properties, required);

    cJSON_Delete(properties);
    cJSON_Delete(required);

    return schema;
}

cJSON *mcp_builtin_create_single_string_schema(const char *param_name) {
    cJSON *properties = cJSON_CreateObject();
    cJSON *required = cJSON_CreateArray();

    cJSON *param = mcp_tool_create_string_schema("String value", NULL);
    cJSON_AddItemToObject(properties, param_name, param);
    cJSON_AddItemToArray(required, cJSON_CreateString(param_name));

    cJSON *schema = mcp_tool_create_object_schema("Single string parameter", properties, required);

    cJSON_Delete(properties);
    cJSON_Delete(required);

    return schema;
}

// Stub implementations for missing tools (to be implemented later)
mcp_tool_t *mcp_builtin_tool_create_power(void) { return NULL; }
mcp_tool_t *mcp_builtin_tool_create_abs(void) { return NULL; }
mcp_tool_t *mcp_builtin_tool_create_round(void) { return NULL; }
mcp_tool_t *mcp_builtin_tool_create_floor(void) { return NULL; }
mcp_tool_t *mcp_builtin_tool_create_ceil(void) { return NULL; }
mcp_tool_t *mcp_builtin_tool_create_text_trim(void) { return NULL; }
mcp_tool_t *mcp_builtin_tool_create_text_reverse(void) { return NULL; }
