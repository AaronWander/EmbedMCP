#include "tools/builtin_tools.h"
#include "utils/logging.h"

// Simplified built-in tool registration for embed_mcp library
// This is a minimal implementation that doesn't register any built-in tools
// Users should use embed_mcp_add_tool() to register their own tools
int mcp_builtin_tools_register_all(mcp_tool_registry_t *registry) {
    (void)registry; // Suppress unused parameter warning
    
    // No built-in tools registered in the embed_mcp library
    // This keeps the library minimal and focused
    mcp_log(MCP_LOG_LEVEL_DEBUG, "No built-in tools registered (embed_mcp library)");
    
    return 0; // Success - no tools to register
}


