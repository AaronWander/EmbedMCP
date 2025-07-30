#ifndef MCP_BUILTIN_TOOLS_H
#define MCP_BUILTIN_TOOLS_H

#include "tool_interface.h"
#include "tool_registry.h"
#include "cJSON.h"

// Built-in tool registration
int mcp_builtin_tools_register_all(mcp_tool_registry_t *registry);
int mcp_builtin_tools_unregister_all(mcp_tool_registry_t *registry);

// Math tools
mcp_tool_t *mcp_builtin_tool_create_add(void);
mcp_tool_t *mcp_builtin_tool_create_subtract(void);
mcp_tool_t *mcp_builtin_tool_create_multiply(void);
mcp_tool_t *mcp_builtin_tool_create_divide(void);
mcp_tool_t *mcp_builtin_tool_create_power(void);
mcp_tool_t *mcp_builtin_tool_create_sqrt(void);
mcp_tool_t *mcp_builtin_tool_create_abs(void);
mcp_tool_t *mcp_builtin_tool_create_round(void);
mcp_tool_t *mcp_builtin_tool_create_floor(void);
mcp_tool_t *mcp_builtin_tool_create_ceil(void);

// Math tool implementations
cJSON *mcp_builtin_tool_add_execute(const cJSON *parameters, void *user_data);
cJSON *mcp_builtin_tool_subtract_execute(const cJSON *parameters, void *user_data);
cJSON *mcp_builtin_tool_multiply_execute(const cJSON *parameters, void *user_data);
cJSON *mcp_builtin_tool_divide_execute(const cJSON *parameters, void *user_data);
cJSON *mcp_builtin_tool_power_execute(const cJSON *parameters, void *user_data);
cJSON *mcp_builtin_tool_sqrt_execute(const cJSON *parameters, void *user_data);
cJSON *mcp_builtin_tool_abs_execute(const cJSON *parameters, void *user_data);
cJSON *mcp_builtin_tool_round_execute(const cJSON *parameters, void *user_data);
cJSON *mcp_builtin_tool_floor_execute(const cJSON *parameters, void *user_data);
cJSON *mcp_builtin_tool_ceil_execute(const cJSON *parameters, void *user_data);

// Text tools
mcp_tool_t *mcp_builtin_tool_create_text_length(void);
mcp_tool_t *mcp_builtin_tool_create_text_upper(void);
mcp_tool_t *mcp_builtin_tool_create_text_lower(void);
mcp_tool_t *mcp_builtin_tool_create_text_trim(void);
mcp_tool_t *mcp_builtin_tool_create_text_reverse(void);
mcp_tool_t *mcp_builtin_tool_create_text_contains(void);
mcp_tool_t *mcp_builtin_tool_create_text_replace(void);
mcp_tool_t *mcp_builtin_tool_create_text_split(void);
mcp_tool_t *mcp_builtin_tool_create_text_join(void);

// Text tool implementations
cJSON *mcp_builtin_tool_text_length_execute(const cJSON *parameters, void *user_data);
cJSON *mcp_builtin_tool_text_upper_execute(const cJSON *parameters, void *user_data);
cJSON *mcp_builtin_tool_text_lower_execute(const cJSON *parameters, void *user_data);
cJSON *mcp_builtin_tool_text_trim_execute(const cJSON *parameters, void *user_data);
cJSON *mcp_builtin_tool_text_reverse_execute(const cJSON *parameters, void *user_data);
cJSON *mcp_builtin_tool_text_contains_execute(const cJSON *parameters, void *user_data);
cJSON *mcp_builtin_tool_text_replace_execute(const cJSON *parameters, void *user_data);
cJSON *mcp_builtin_tool_text_split_execute(const cJSON *parameters, void *user_data);
cJSON *mcp_builtin_tool_text_join_execute(const cJSON *parameters, void *user_data);

// Utility tools
mcp_tool_t *mcp_builtin_tool_create_echo(void);
mcp_tool_t *mcp_builtin_tool_create_timestamp(void);
mcp_tool_t *mcp_builtin_tool_create_uuid(void);
mcp_tool_t *mcp_builtin_tool_create_random_number(void);
mcp_tool_t *mcp_builtin_tool_create_base64_encode(void);
mcp_tool_t *mcp_builtin_tool_create_base64_decode(void);
mcp_tool_t *mcp_builtin_tool_create_hash_md5(void);
mcp_tool_t *mcp_builtin_tool_create_hash_sha256(void);

// Utility tool implementations
cJSON *mcp_builtin_tool_echo_execute(const cJSON *parameters, void *user_data);
cJSON *mcp_builtin_tool_timestamp_execute(const cJSON *parameters, void *user_data);
cJSON *mcp_builtin_tool_uuid_execute(const cJSON *parameters, void *user_data);
cJSON *mcp_builtin_tool_random_number_execute(const cJSON *parameters, void *user_data);
cJSON *mcp_builtin_tool_base64_encode_execute(const cJSON *parameters, void *user_data);
cJSON *mcp_builtin_tool_base64_decode_execute(const cJSON *parameters, void *user_data);
cJSON *mcp_builtin_tool_hash_md5_execute(const cJSON *parameters, void *user_data);
cJSON *mcp_builtin_tool_hash_sha256_execute(const cJSON *parameters, void *user_data);

// System tools (optional, may require additional permissions)
mcp_tool_t *mcp_builtin_tool_create_system_info(void);
mcp_tool_t *mcp_builtin_tool_create_current_time(void);
mcp_tool_t *mcp_builtin_tool_create_environment_var(void);

// System tool implementations
cJSON *mcp_builtin_tool_system_info_execute(const cJSON *parameters, void *user_data);
cJSON *mcp_builtin_tool_current_time_execute(const cJSON *parameters, void *user_data);
cJSON *mcp_builtin_tool_environment_var_execute(const cJSON *parameters, void *user_data);

// Schema creation helpers for built-in tools
cJSON *mcp_builtin_create_two_number_schema(const char *param1_name, const char *param2_name);
cJSON *mcp_builtin_create_single_number_schema(const char *param_name);
cJSON *mcp_builtin_create_single_string_schema(const char *param_name);
cJSON *mcp_builtin_create_text_operation_schema(void);
cJSON *mcp_builtin_create_text_search_schema(void);
cJSON *mcp_builtin_create_text_replace_schema(void);
cJSON *mcp_builtin_create_array_join_schema(void);

// Validation helpers for built-in tools
bool mcp_builtin_validate_two_numbers(const cJSON *parameters, const char *param1, const char *param2);
bool mcp_builtin_validate_single_number(const cJSON *parameters, const char *param_name);
bool mcp_builtin_validate_single_string(const cJSON *parameters, const char *param_name);
bool mcp_builtin_validate_non_zero_number(const cJSON *parameters, const char *param_name);

// Result creation helpers for built-in tools
cJSON *mcp_builtin_create_number_result(double value);
cJSON *mcp_builtin_create_string_result(const char *value);
cJSON *mcp_builtin_create_boolean_result(bool value);
cJSON *mcp_builtin_create_array_result(cJSON *array);
cJSON *mcp_builtin_create_math_result(double num1, double num2, double result, 
                                     const char *operation, const char *message);

// Tool name constants
#define MCP_BUILTIN_TOOL_ADD "add"
#define MCP_BUILTIN_TOOL_SUBTRACT "subtract"
#define MCP_BUILTIN_TOOL_MULTIPLY "multiply"
#define MCP_BUILTIN_TOOL_DIVIDE "divide"
#define MCP_BUILTIN_TOOL_POWER "power"
#define MCP_BUILTIN_TOOL_SQRT "sqrt"
#define MCP_BUILTIN_TOOL_ABS "abs"
#define MCP_BUILTIN_TOOL_ROUND "round"
#define MCP_BUILTIN_TOOL_FLOOR "floor"
#define MCP_BUILTIN_TOOL_CEIL "ceil"

#define MCP_BUILTIN_TOOL_TEXT_LENGTH "text_length"
#define MCP_BUILTIN_TOOL_TEXT_UPPER "text_upper"
#define MCP_BUILTIN_TOOL_TEXT_LOWER "text_lower"
#define MCP_BUILTIN_TOOL_TEXT_TRIM "text_trim"
#define MCP_BUILTIN_TOOL_TEXT_REVERSE "text_reverse"
#define MCP_BUILTIN_TOOL_TEXT_CONTAINS "text_contains"
#define MCP_BUILTIN_TOOL_TEXT_REPLACE "text_replace"
#define MCP_BUILTIN_TOOL_TEXT_SPLIT "text_split"
#define MCP_BUILTIN_TOOL_TEXT_JOIN "text_join"

#define MCP_BUILTIN_TOOL_ECHO "echo"
#define MCP_BUILTIN_TOOL_TIMESTAMP "timestamp"
#define MCP_BUILTIN_TOOL_UUID "uuid"
#define MCP_BUILTIN_TOOL_RANDOM_NUMBER "random_number"
#define MCP_BUILTIN_TOOL_BASE64_ENCODE "base64_encode"
#define MCP_BUILTIN_TOOL_BASE64_DECODE "base64_decode"
#define MCP_BUILTIN_TOOL_HASH_MD5 "hash_md5"
#define MCP_BUILTIN_TOOL_HASH_SHA256 "hash_sha256"

#define MCP_BUILTIN_TOOL_SYSTEM_INFO "system_info"
#define MCP_BUILTIN_TOOL_CURRENT_TIME "current_time"
#define MCP_BUILTIN_TOOL_ENVIRONMENT_VAR "environment_var"

#endif // MCP_BUILTIN_TOOLS_H
