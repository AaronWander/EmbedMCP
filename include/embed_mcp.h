#ifndef EMBED_MCP_H
#define EMBED_MCP_H

#include <stddef.h>
#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
typedef struct embed_mcp_server embed_mcp_server_t;

// Tool handler function type
// Parameters: args (JSON object with tool arguments)
// Returns: JSON object with tool result (caller must free)
typedef cJSON* (*embed_mcp_tool_handler_t)(const cJSON *args);

// Transport types
typedef enum {
    EMBED_MCP_TRANSPORT_STDIO,
    EMBED_MCP_TRANSPORT_HTTP
} embed_mcp_transport_t;

// Server configuration
typedef struct {
    const char *name;           // Server name
    const char *version;        // Server version
    const char *host;           // HTTP host (default: "0.0.0.0")
    int port;                   // HTTP port (default: 8080)
    const char *path;           // HTTP endpoint path (default: "/mcp")
    int max_tools;              // Maximum number of tools (default: 100)
    int debug;                  // Enable debug logging (default: 0)
} embed_mcp_config_t;

// =============================================================================
// Core API Functions
// =============================================================================

/**
 * Create a new MCP server instance
 * @param config Server configuration
 * @return Server instance or NULL on error
 */
embed_mcp_server_t *embed_mcp_create(const embed_mcp_config_t *config);

/**
 * Destroy MCP server instance
 * @param server Server instance
 */
void embed_mcp_destroy(embed_mcp_server_t *server);

/**
 * Add a tool to the server with automatic schema generation
 * @param server Server instance
 * @param name Tool name (must be unique)
 * @param description Tool description
 * @param handler Tool handler function
 * @return 0 on success, -1 on error
 */
int embed_mcp_add_tool(embed_mcp_server_t *server,
                       const char *name,
                       const char *description,
                       embed_mcp_tool_handler_t handler);

/**
 * Add a tool with JSON schema
 * @param server Server instance
 * @param name Tool name
 * @param description Tool description
 * @param schema JSON schema for input validation (can be NULL)
 * @param handler Tool handler function
 * @return 0 on success, -1 on error
 */
int embed_mcp_add_tool_with_schema(embed_mcp_server_t *server,
                                   const char *name,
                                   const char *description,
                                   const cJSON *schema,
                                   embed_mcp_tool_handler_t handler);

// Convenience functions for common tool types

/**
 * Add a math tool (automatically generates schema for two number parameters)
 * @param server Server instance
 * @param name Tool name
 * @param description Tool description
 * @param handler Tool handler function
 * @return 0 on success, -1 on error
 */
int embed_mcp_add_math_tool(embed_mcp_server_t *server,
                            const char *name,
                            const char *description,
                            embed_mcp_tool_handler_t handler);

/**
 * Add a text tool (automatically generates schema for text input)
 * @param server Server instance
 * @param name Tool name
 * @param description Tool description
 * @param param_name Name of the text parameter
 * @param param_description Description of the text parameter
 * @param handler Tool handler function
 * @return 0 on success, -1 on error
 */
int embed_mcp_add_text_tool(embed_mcp_server_t *server,
                            const char *name,
                            const char *description,
                            const char *param_name,
                            const char *param_description,
                            embed_mcp_tool_handler_t handler);

/**
 * Run server with STDIO transport
 * This function blocks until the server is stopped
 * @param server Server instance
 * @return 0 on success, -1 on error
 */
int embed_mcp_run_stdio(embed_mcp_server_t *server);

/**
 * Run server with HTTP transport
 * This function blocks until the server is stopped
 * @param server Server instance
 * @return 0 on success, -1 on error
 */
int embed_mcp_run_http(embed_mcp_server_t *server);

/**
 * Run server with specified transport
 * This function blocks until the server is stopped
 * @param server Server instance
 * @param transport Transport type
 * @return 0 on success, -1 on error
 */
int embed_mcp_run(embed_mcp_server_t *server, embed_mcp_transport_t transport);

/**
 * Stop the running server
 * @param server Server instance
 */
void embed_mcp_stop(embed_mcp_server_t *server);

// =============================================================================
// Convenience Functions
// =============================================================================

/**
 * Create server with default configuration
 * @param name Server name
 * @param version Server version
 * @return Server instance or NULL on error
 */
embed_mcp_server_t *embed_mcp_create_simple(const char *name, const char *version);

/**
 * Quick start: create server, add tools, and run
 * @param name Server name
 * @param version Server version
 * @param transport Transport type
 * @param port Port for HTTP (ignored for STDIO)
 * @return 0 on success, -1 on error
 */
int embed_mcp_quick_start(const char *name, const char *version, 
                          embed_mcp_transport_t transport, int port);

// =============================================================================
// Utility Functions
// =============================================================================

/**
 * Create a default configuration
 * @param name Server name
 * @param version Server version
 * @return Configuration structure
 */
embed_mcp_config_t embed_mcp_config_default(const char *name, const char *version);

/**
 * Get last error message
 * @return Error message string (do not free)
 */
const char *embed_mcp_get_error(void);

#ifdef __cplusplus
}
#endif

#endif // EMBED_MCP_H
