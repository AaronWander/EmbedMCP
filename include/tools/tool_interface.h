#ifndef MCP_TOOL_INTERFACE_H
#define MCP_TOOL_INTERFACE_H

#include <stdbool.h>
#include <stddef.h>
#include "cJSON.h"

// Forward declarations
typedef struct mcp_tool mcp_tool_t;

// Tool execution function type
typedef cJSON *(*mcp_tool_execute_func_t)(const cJSON *parameters, void *user_data);

// Tool validation function type
typedef bool (*mcp_tool_validate_func_t)(const cJSON *parameters, void *user_data);

// Tool cleanup function type
typedef void (*mcp_tool_cleanup_func_t)(void *user_data);

// Tool structure
struct mcp_tool {
    // Basic information
    char *name;
    char *title;
    char *description;
    
    // Schema definition
    cJSON *input_schema;
    cJSON *output_schema;
    
    // Function pointers
    mcp_tool_execute_func_t execute;
    mcp_tool_validate_func_t validate;  // Optional
    mcp_tool_cleanup_func_t cleanup;    // Optional
    
    // User data
    void *user_data;
    
    // Tool metadata
    char *version;
    char *author;
    char *category;
    bool is_async;
    bool is_dangerous;
    
    // Execution constraints
    size_t max_execution_time_ms;
    size_t max_memory_usage_bytes;
    
    // Internal reference counting
    int ref_count;
};

// Tool creation and destruction
mcp_tool_t *mcp_tool_create(const char *name,
                           const char *title,
                           const char *description,
                           const cJSON *input_schema,
                           mcp_tool_execute_func_t execute_func,
                           void *user_data);

mcp_tool_t *mcp_tool_create_full(const char *name,
                                const char *title,
                                const char *description,
                                const cJSON *input_schema,
                                const cJSON *output_schema,
                                mcp_tool_execute_func_t execute_func,
                                mcp_tool_validate_func_t validate_func,
                                mcp_tool_cleanup_func_t cleanup_func,
                                void *user_data);

void mcp_tool_destroy(mcp_tool_t *tool);

// Tool reference counting
mcp_tool_t *mcp_tool_ref(mcp_tool_t *tool);
void mcp_tool_unref(mcp_tool_t *tool);

// Tool information
const char *mcp_tool_get_name(const mcp_tool_t *tool);
const char *mcp_tool_get_title(const mcp_tool_t *tool);
const char *mcp_tool_get_description(const mcp_tool_t *tool);
const cJSON *mcp_tool_get_input_schema(const mcp_tool_t *tool);
const cJSON *mcp_tool_get_output_schema(const mcp_tool_t *tool);

// Tool metadata
int mcp_tool_set_version(mcp_tool_t *tool, const char *version);
int mcp_tool_set_author(mcp_tool_t *tool, const char *author);
int mcp_tool_set_category(mcp_tool_t *tool, const char *category);
int mcp_tool_set_async(mcp_tool_t *tool, bool is_async);
int mcp_tool_set_dangerous(mcp_tool_t *tool, bool is_dangerous);
int mcp_tool_set_execution_constraints(mcp_tool_t *tool,
                                      size_t max_execution_time_ms,
                                      size_t max_memory_usage_bytes);

const char *mcp_tool_get_version(const mcp_tool_t *tool);
const char *mcp_tool_get_author(const mcp_tool_t *tool);
const char *mcp_tool_get_category(const mcp_tool_t *tool);
bool mcp_tool_is_async(const mcp_tool_t *tool);
bool mcp_tool_is_dangerous(const mcp_tool_t *tool);

// Tool execution
cJSON *mcp_tool_execute(const mcp_tool_t *tool, const cJSON *parameters);
bool mcp_tool_validate_parameters(const mcp_tool_t *tool, const cJSON *parameters);

// Tool serialization
cJSON *mcp_tool_to_json(const mcp_tool_t *tool);
cJSON *mcp_tool_to_mcp_tool_definition(const mcp_tool_t *tool);

// Tool validation
bool mcp_tool_validate(const mcp_tool_t *tool);
bool mcp_tool_validate_name(const char *name);
bool mcp_tool_validate_schema(const cJSON *schema);

// Schema utilities
cJSON *mcp_tool_create_simple_schema(const char *type, const char *description);
cJSON *mcp_tool_create_object_schema(const char *description, const cJSON *properties, 
                                    const cJSON *required);
cJSON *mcp_tool_create_array_schema(const char *description, const cJSON *items);
cJSON *mcp_tool_create_string_schema(const char *description, const char *pattern);
cJSON *mcp_tool_create_number_schema(const char *description, double minimum, double maximum);
cJSON *mcp_tool_create_boolean_schema(const char *description);

// Parameter validation utilities
bool mcp_tool_validate_parameter_type(const cJSON *value, const char *expected_type);
bool mcp_tool_validate_parameter_against_schema(const cJSON *value, const cJSON *schema);
char *mcp_tool_get_validation_error_message(const cJSON *value, const cJSON *schema);

// Error result creation
cJSON *mcp_tool_create_error_result(const char *error_type, const char *message, cJSON *details);
cJSON *mcp_tool_create_validation_error(const char *message);
cJSON *mcp_tool_create_execution_error(const char *message);
cJSON *mcp_tool_create_timeout_error(void);
cJSON *mcp_tool_create_memory_error(void);

// Success result creation
cJSON *mcp_tool_create_success_result(cJSON *data);


// Tool categories (predefined constants)
#define MCP_TOOL_CATEGORY_GENERAL "general"
#define MCP_TOOL_CATEGORY_MATH "math"
#define MCP_TOOL_CATEGORY_TEXT "text"
#define MCP_TOOL_CATEGORY_FILE "file"
#define MCP_TOOL_CATEGORY_NETWORK "network"
#define MCP_TOOL_CATEGORY_SYSTEM "system"
#define MCP_TOOL_CATEGORY_DATABASE "database"
#define MCP_TOOL_CATEGORY_UTILITY "utility"

// Tool error types
#define MCP_TOOL_ERROR_VALIDATION "validation_error"
#define MCP_TOOL_ERROR_EXECUTION "execution_error"
#define MCP_TOOL_ERROR_TIMEOUT "timeout_error"
#define MCP_TOOL_ERROR_MEMORY "memory_error"
#define MCP_TOOL_ERROR_PERMISSION "permission_error"
#define MCP_TOOL_ERROR_NOT_FOUND "not_found_error"
#define MCP_TOOL_ERROR_INTERNAL "internal_error"

#endif // MCP_TOOL_INTERFACE_H
