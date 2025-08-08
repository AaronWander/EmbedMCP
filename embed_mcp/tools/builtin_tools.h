#ifndef MCP_BUILTIN_TOOLS_H
#define MCP_BUILTIN_TOOLS_H

#include "tool_interface.h"
#include "tool_registry.h"
#include "cJSON.h"

// Built-in tool registration
int mcp_builtin_tools_register_all(mcp_tool_registry_t *registry);

// Currently implemented utility tool functions
cJSON *mcp_builtin_tool_timestamp_execute(const cJSON *parameters, void *user_data);
cJSON *mcp_builtin_tool_uuid_execute(const cJSON *parameters, void *user_data);
cJSON *mcp_builtin_tool_base64_encode_execute(const cJSON *parameters, void *user_data);
cJSON *mcp_builtin_tool_base64_decode_execute(const cJSON *parameters, void *user_data);

// Tool name constants for implemented tools
#define MCP_BUILTIN_TOOL_TIMESTAMP "timestamp"
#define MCP_BUILTIN_TOOL_UUID "uuid"
#define MCP_BUILTIN_TOOL_BASE64_ENCODE "base64_encode"
#define MCP_BUILTIN_TOOL_BASE64_DECODE "base64_decode"

#endif // MCP_BUILTIN_TOOLS_H
