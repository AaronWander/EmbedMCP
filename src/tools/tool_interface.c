#include "tools/tool_interface.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Tool creation and destruction
mcp_tool_t *mcp_tool_create(const char *name,
                           const char *title,
                           const char *description,
                           const cJSON *input_schema,
                           mcp_tool_execute_func_t execute_func,
                           void *user_data) {
    return mcp_tool_create_full(name, title, description, input_schema, NULL,
                               execute_func, NULL, NULL, user_data);
}

mcp_tool_t *mcp_tool_create_full(const char *name,
                                const char *title,
                                const char *description,
                                const cJSON *input_schema,
                                const cJSON *output_schema,
                                mcp_tool_execute_func_t execute_func,
                                mcp_tool_validate_func_t validate_func,
                                mcp_tool_cleanup_func_t cleanup_func,
                                void *user_data) {
    if (!name || !execute_func) return NULL;
    
    mcp_tool_t *tool = calloc(1, sizeof(mcp_tool_t));
    if (!tool) return NULL;
    
    tool->name = strdup(name);
    tool->title = title ? strdup(title) : strdup(name);
    tool->description = description ? strdup(description) : strdup("");
    
    tool->input_schema = input_schema ? cJSON_Duplicate(input_schema, 1) : NULL;
    tool->output_schema = output_schema ? cJSON_Duplicate(output_schema, 1) : NULL;
    
    tool->execute = execute_func;
    tool->validate = validate_func;
    tool->cleanup = cleanup_func;
    tool->user_data = user_data;
    
    tool->version = NULL;
    tool->author = NULL;
    tool->category = strdup(MCP_TOOL_CATEGORY_GENERAL);
    tool->is_async = false;
    tool->is_dangerous = false;
    
    tool->max_execution_time_ms = 30000; // 30 seconds default
    tool->max_memory_usage_bytes = 1024 * 1024; // 1MB default
    
    tool->ref_count = 1;
    
    if (!tool->name || !tool->title || !tool->description || !tool->category) {
        mcp_tool_destroy(tool);
        return NULL;
    }
    
    return tool;
}

void mcp_tool_destroy(mcp_tool_t *tool) {
    if (!tool) return;
    
    // Call cleanup function if provided
    if (tool->cleanup) {
        tool->cleanup(tool->user_data);
    }
    
    free(tool->name);
    free(tool->title);
    free(tool->description);
    free(tool->version);
    free(tool->author);
    free(tool->category);
    
    if (tool->input_schema) cJSON_Delete(tool->input_schema);
    if (tool->output_schema) cJSON_Delete(tool->output_schema);
    
    free(tool);
}

// Tool reference counting
mcp_tool_t *mcp_tool_ref(mcp_tool_t *tool) {
    if (!tool) return NULL;
    
    tool->ref_count++;
    return tool;
}

void mcp_tool_unref(mcp_tool_t *tool) {
    if (!tool) return;
    
    tool->ref_count--;
    if (tool->ref_count <= 0) {
        mcp_tool_destroy(tool);
    }
}

// Tool information
const char *mcp_tool_get_name(const mcp_tool_t *tool) {
    return tool ? tool->name : NULL;
}

const char *mcp_tool_get_title(const mcp_tool_t *tool) {
    return tool ? tool->title : NULL;
}

const char *mcp_tool_get_description(const mcp_tool_t *tool) {
    return tool ? tool->description : NULL;
}

const cJSON *mcp_tool_get_input_schema(const mcp_tool_t *tool) {
    return tool ? tool->input_schema : NULL;
}

const cJSON *mcp_tool_get_output_schema(const mcp_tool_t *tool) {
    return tool ? tool->output_schema : NULL;
}

// Tool metadata
int mcp_tool_set_version(mcp_tool_t *tool, const char *version) {
    if (!tool) return -1;
    
    free(tool->version);
    tool->version = version ? strdup(version) : NULL;
    
    return 0;
}

int mcp_tool_set_author(mcp_tool_t *tool, const char *author) {
    if (!tool) return -1;
    
    free(tool->author);
    tool->author = author ? strdup(author) : NULL;
    
    return 0;
}

int mcp_tool_set_category(mcp_tool_t *tool, const char *category) {
    if (!tool) return -1;
    
    free(tool->category);
    tool->category = category ? strdup(category) : strdup(MCP_TOOL_CATEGORY_GENERAL);
    
    return 0;
}

int mcp_tool_set_async(mcp_tool_t *tool, bool is_async) {
    if (!tool) return -1;
    
    tool->is_async = is_async;
    return 0;
}

int mcp_tool_set_dangerous(mcp_tool_t *tool, bool is_dangerous) {
    if (!tool) return -1;
    
    tool->is_dangerous = is_dangerous;
    return 0;
}

int mcp_tool_set_execution_constraints(mcp_tool_t *tool,
                                      size_t max_execution_time_ms,
                                      size_t max_memory_usage_bytes) {
    if (!tool) return -1;
    
    tool->max_execution_time_ms = max_execution_time_ms;
    tool->max_memory_usage_bytes = max_memory_usage_bytes;
    
    return 0;
}

const char *mcp_tool_get_version(const mcp_tool_t *tool) {
    return tool ? tool->version : NULL;
}

const char *mcp_tool_get_author(const mcp_tool_t *tool) {
    return tool ? tool->author : NULL;
}

const char *mcp_tool_get_category(const mcp_tool_t *tool) {
    return tool ? tool->category : NULL;
}

bool mcp_tool_is_async(const mcp_tool_t *tool) {
    return tool ? tool->is_async : false;
}

bool mcp_tool_is_dangerous(const mcp_tool_t *tool) {
    return tool ? tool->is_dangerous : false;
}

// Tool execution
cJSON *mcp_tool_execute(const mcp_tool_t *tool, const cJSON *parameters) {
    if (!tool || !tool->execute) {
        return mcp_tool_create_error_result(MCP_TOOL_ERROR_INTERNAL, "Tool or execute function is null", NULL);
    }
    
    // Validate parameters if validation function is provided
    if (tool->validate && !tool->validate(parameters, tool->user_data)) {
        return mcp_tool_create_validation_error("Parameter validation failed");
    }
    
    // Validate against input schema if provided
    if (tool->input_schema && !mcp_tool_validate_parameter_against_schema(parameters, tool->input_schema)) {
        char *error_msg = mcp_tool_get_validation_error_message(parameters, tool->input_schema);
        cJSON *result = mcp_tool_create_validation_error(error_msg ? error_msg : "Schema validation failed");
        free(error_msg);
        return result;
    }
    
    // Execute the tool
    cJSON *result = tool->execute(parameters, tool->user_data);
    
    // If no result returned, create an error
    if (!result) {
        return mcp_tool_create_execution_error("Tool execution returned null result");
    }
    
    return result;
}

bool mcp_tool_validate_parameters(const mcp_tool_t *tool, const cJSON *parameters) {
    if (!tool) return false;
    
    // Use custom validation function if provided
    if (tool->validate) {
        return tool->validate(parameters, tool->user_data);
    }
    
    // Use schema validation if available
    if (tool->input_schema) {
        return mcp_tool_validate_parameter_against_schema(parameters, tool->input_schema);
    }
    
    // No validation available, assume valid
    return true;
}

// Tool serialization
cJSON *mcp_tool_to_json(const mcp_tool_t *tool) {
    if (!tool) return NULL;
    
    cJSON *json = cJSON_CreateObject();
    if (!json) return NULL;
    
    cJSON_AddStringToObject(json, "name", tool->name);
    cJSON_AddStringToObject(json, "title", tool->title);
    cJSON_AddStringToObject(json, "description", tool->description);
    
    if (tool->input_schema) {
        cJSON_AddItemToObject(json, "inputSchema", cJSON_Duplicate(tool->input_schema, 1));
    }
    
    if (tool->output_schema) {
        cJSON_AddItemToObject(json, "outputSchema", cJSON_Duplicate(tool->output_schema, 1));
    }
    
    if (tool->version) {
        cJSON_AddStringToObject(json, "version", tool->version);
    }
    
    if (tool->author) {
        cJSON_AddStringToObject(json, "author", tool->author);
    }
    
    cJSON_AddStringToObject(json, "category", tool->category);
    cJSON_AddBoolToObject(json, "isAsync", tool->is_async);
    cJSON_AddBoolToObject(json, "isDangerous", tool->is_dangerous);
    
    cJSON_AddNumberToObject(json, "maxExecutionTimeMs", (double)tool->max_execution_time_ms);
    cJSON_AddNumberToObject(json, "maxMemoryUsageBytes", (double)tool->max_memory_usage_bytes);
    
    return json;
}

cJSON *mcp_tool_to_mcp_tool_definition(const mcp_tool_t *tool) {
    if (!tool) return NULL;
    
    cJSON *json = cJSON_CreateObject();
    if (!json) return NULL;
    
    cJSON_AddStringToObject(json, "name", tool->name);
    
    if (tool->title && strcmp(tool->title, tool->name) != 0) {
        cJSON_AddStringToObject(json, "title", tool->title);
    }
    
    cJSON_AddStringToObject(json, "description", tool->description);
    
    if (tool->input_schema) {
        cJSON_AddItemToObject(json, "inputSchema", cJSON_Duplicate(tool->input_schema, 1));
    }
    
    return json;
}

// Tool validation
bool mcp_tool_validate(const mcp_tool_t *tool) {
    if (!tool) return false;
    
    // Must have name and execute function
    if (!tool->name || !tool->execute) return false;
    
    // Validate name
    if (!mcp_tool_validate_name(tool->name)) return false;
    
    // Validate schemas if present
    if (tool->input_schema && !mcp_tool_validate_schema(tool->input_schema)) return false;
    if (tool->output_schema && !mcp_tool_validate_schema(tool->output_schema)) return false;
    
    return true;
}

bool mcp_tool_validate_name(const char *name) {
    if (!name || strlen(name) == 0) return false;
    
    // Name should not be too long
    if (strlen(name) > 255) return false;
    
    // Name should contain only alphanumeric characters, underscores, and hyphens
    for (const char *p = name; *p; p++) {
        if (!(*p >= 'a' && *p <= 'z') &&
            !(*p >= 'A' && *p <= 'Z') &&
            !(*p >= '0' && *p <= '9') &&
            *p != '_' && *p != '-') {
            return false;
        }
    }
    
    return true;
}

bool mcp_tool_validate_schema(const cJSON *schema) {
    if (!schema) return true; // NULL schema is valid (no validation)
    
    if (!cJSON_IsObject(schema)) return false;
    
    // Basic JSON Schema validation - check for required fields
    cJSON *type = cJSON_GetObjectItem(schema, "type");
    if (type && !cJSON_IsString(type)) return false;
    
    return true;
}

// Schema utilities
cJSON *mcp_tool_create_simple_schema(const char *type, const char *description) {
    cJSON *schema = cJSON_CreateObject();
    if (!schema) return NULL;

    cJSON_AddStringToObject(schema, "type", type);
    if (description) {
        cJSON_AddStringToObject(schema, "description", description);
    }

    return schema;
}

cJSON *mcp_tool_create_object_schema(const char *description, const cJSON *properties,
                                    const cJSON *required) {
    cJSON *schema = cJSON_CreateObject();
    if (!schema) return NULL;

    cJSON_AddStringToObject(schema, "type", "object");
    if (description) {
        cJSON_AddStringToObject(schema, "description", description);
    }

    if (properties) {
        cJSON_AddItemToObject(schema, "properties", cJSON_Duplicate(properties, 1));
    }

    if (required) {
        cJSON_AddItemToObject(schema, "required", cJSON_Duplicate(required, 1));
    }

    return schema;
}

cJSON *mcp_tool_create_array_schema(const char *description, const cJSON *items) {
    cJSON *schema = cJSON_CreateObject();
    if (!schema) return NULL;

    cJSON_AddStringToObject(schema, "type", "array");
    if (description) {
        cJSON_AddStringToObject(schema, "description", description);
    }

    if (items) {
        cJSON_AddItemToObject(schema, "items", cJSON_Duplicate(items, 1));
    }

    return schema;
}

cJSON *mcp_tool_create_string_schema(const char *description, const char *pattern) {
    cJSON *schema = cJSON_CreateObject();
    if (!schema) return NULL;

    cJSON_AddStringToObject(schema, "type", "string");
    if (description) {
        cJSON_AddStringToObject(schema, "description", description);
    }

    if (pattern) {
        cJSON_AddStringToObject(schema, "pattern", pattern);
    }

    return schema;
}

cJSON *mcp_tool_create_number_schema(const char *description, double minimum, double maximum) {
    cJSON *schema = cJSON_CreateObject();
    if (!schema) return NULL;

    cJSON_AddStringToObject(schema, "type", "number");
    if (description) {
        cJSON_AddStringToObject(schema, "description", description);
    }

    if (minimum > -1e308) { // Use a large negative number instead of -INFINITY
        cJSON_AddNumberToObject(schema, "minimum", minimum);
    }

    if (maximum < 1e308) { // Use a large positive number instead of INFINITY
        cJSON_AddNumberToObject(schema, "maximum", maximum);
    }

    return schema;
}

cJSON *mcp_tool_create_boolean_schema(const char *description) {
    cJSON *schema = cJSON_CreateObject();
    if (!schema) return NULL;

    cJSON_AddStringToObject(schema, "type", "boolean");
    if (description) {
        cJSON_AddStringToObject(schema, "description", description);
    }

    return schema;
}

// Parameter validation utilities
bool mcp_tool_validate_parameter_type(const cJSON *value, const char *expected_type) {
    if (!value || !expected_type) return false;

    if (strcmp(expected_type, "string") == 0) {
        return cJSON_IsString(value);
    } else if (strcmp(expected_type, "number") == 0) {
        return cJSON_IsNumber(value);
    } else if (strcmp(expected_type, "boolean") == 0) {
        return cJSON_IsBool(value);
    } else if (strcmp(expected_type, "array") == 0) {
        return cJSON_IsArray(value);
    } else if (strcmp(expected_type, "object") == 0) {
        return cJSON_IsObject(value);
    } else if (strcmp(expected_type, "null") == 0) {
        return cJSON_IsNull(value);
    }

    return false;
}

bool mcp_tool_validate_parameter_against_schema(const cJSON *value, const cJSON *schema) {
    if (!schema) return true; // No schema means no validation
    if (!value) return false;

    // Get the type from schema
    cJSON *type = cJSON_GetObjectItem(schema, "type");
    if (type && cJSON_IsString(type)) {
        if (!mcp_tool_validate_parameter_type(value, type->valuestring)) {
            return false;
        }
    }

    // Additional validations can be added here (minimum, maximum, pattern, etc.)

    return true;
}

char *mcp_tool_get_validation_error_message(const cJSON *value, const cJSON *schema) {
    if (!schema) return strdup("No schema provided");
    if (!value) return strdup("No value provided");

    cJSON *type = cJSON_GetObjectItem(schema, "type");
    if (type && cJSON_IsString(type)) {
        if (!mcp_tool_validate_parameter_type(value, type->valuestring)) {
            char *msg = malloc(256);
            if (msg) {
                snprintf(msg, 256, "Expected type '%s' but got different type", type->valuestring);
            }
            return msg;
        }
    }

    return strdup("Validation failed");
}

// Error result creation
cJSON *mcp_tool_create_error_result(const char *error_type, const char *message, cJSON *details) {
    cJSON *result = cJSON_CreateObject();
    if (!result) return NULL;

    // Create content array for error according to MCP spec
    cJSON *content = cJSON_CreateArray();
    if (!content) {
        cJSON_Delete(result);
        return NULL;
    }

    // Create text content block with error message
    cJSON *text_block = cJSON_CreateObject();
    cJSON_AddStringToObject(text_block, "type", "text");

    // Format error message
    char error_msg[512];
    snprintf(error_msg, sizeof(error_msg), "Error (%s): %s",
             error_type ? error_type : MCP_TOOL_ERROR_INTERNAL,
             message ? message : "Unknown error");
    cJSON_AddStringToObject(text_block, "text", error_msg);

    cJSON_AddItemToArray(content, text_block);
    cJSON_AddItemToObject(result, "content", content);

    // Add structured content for error details
    if (details) {
        cJSON_AddItemToObject(result, "structuredContent", cJSON_Duplicate(details, 1));
    }

    cJSON_AddBoolToObject(result, "isError", true);

    return result;
}

cJSON *mcp_tool_create_validation_error(const char *message) {
    return mcp_tool_create_error_result(MCP_TOOL_ERROR_VALIDATION, message, NULL);
}

cJSON *mcp_tool_create_execution_error(const char *message) {
    return mcp_tool_create_error_result(MCP_TOOL_ERROR_EXECUTION, message, NULL);
}

cJSON *mcp_tool_create_timeout_error(void) {
    return mcp_tool_create_error_result(MCP_TOOL_ERROR_TIMEOUT, "Tool execution timed out", NULL);
}

cJSON *mcp_tool_create_memory_error(void) {
    return mcp_tool_create_error_result(MCP_TOOL_ERROR_MEMORY, "Tool execution exceeded memory limit", NULL);
}

// Success result creation
cJSON *mcp_tool_create_success_result(cJSON *data) {
    cJSON *result = cJSON_CreateObject();
    if (!result) return NULL;

    // Create content array according to MCP spec
    cJSON *content = cJSON_CreateArray();
    if (!content) {
        cJSON_Delete(result);
        return NULL;
    }

    // Create text content block
    cJSON *text_block = cJSON_CreateObject();
    cJSON_AddStringToObject(text_block, "type", "text");

    if (data) {
        // Convert data to string representation
        char *data_str = cJSON_Print(data);
        cJSON_AddStringToObject(text_block, "text", data_str ? data_str : "{}");
        if (data_str) free(data_str);
    } else {
        cJSON_AddStringToObject(text_block, "text", "Success");
    }

    cJSON_AddItemToArray(content, text_block);
    cJSON_AddItemToObject(result, "content", content);

    // Add structured content if data provided
    if (data) {
        cJSON_AddItemToObject(result, "structuredContent", cJSON_Duplicate(data, 1));
    }

    cJSON_AddBoolToObject(result, "isError", false);

    return result;
}


