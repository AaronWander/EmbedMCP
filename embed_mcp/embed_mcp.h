#ifndef EMBED_MCP_H
#define EMBED_MCP_H

#include <stddef.h>
#include <stdint.h>

// cJSON dependency - users can either:
// 1. Install cJSON system-wide: #include <cjson/cJSON.h>
// 2. Use bundled cJSON: #include "cjson/cJSON.h"
#ifdef EMBED_MCP_USE_SYSTEM_CJSON
    #include <cjson/cJSON.h>
#else
    #include "cjson/cJSON.h"
#endif

// Resource interface for templates
#include "tools/resource_interface.h"

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
typedef struct embed_mcp_server embed_mcp_server_t;

// Tool handler function type (legacy)
// Parameters: args (JSON object with tool arguments)
// Returns: JSON object with tool result (caller must free)
typedef cJSON* (*embed_mcp_tool_handler_t)(const cJSON *args);

// Parameter types
typedef enum {
    MCP_PARAM_INT,
    MCP_PARAM_DOUBLE,
    MCP_PARAM_STRING,
    MCP_PARAM_BOOL,
    MCP_PARAM_CHAR        // Single character (transmitted as string)
} mcp_param_type_t;

// Return types for pure functions
typedef enum {
    MCP_RETURN_DOUBLE,
    MCP_RETURN_INT,
    MCP_RETURN_STRING,
    MCP_RETURN_VOID
} mcp_return_type_t;

// Universal parameter value - can hold any type
typedef struct {
    mcp_param_type_t type;
    union {
        int64_t int_val;
        double double_val;
        char* string_val;
        int bool_val;
        struct {
            void* data;
            size_t count;
            mcp_param_type_t element_type;
        } array_val;
    };
} mcp_param_value_t;

// Universal parameter accessor - provides type-safe access to parameters
typedef struct mcp_param_accessor mcp_param_accessor_t;

// Parameter accessor interface - handles all MCP parameter access patterns
struct mcp_param_accessor {
    // Type-safe getters for basic types
    int64_t (*get_int)(mcp_param_accessor_t* self, const char* name);
    double (*get_double)(mcp_param_accessor_t* self, const char* name);
    const char* (*get_string)(mcp_param_accessor_t* self, const char* name);
    int (*get_bool)(mcp_param_accessor_t* self, const char* name);

    // Array getters for common MCP patterns
    double* (*get_double_array)(mcp_param_accessor_t* self, const char* name, size_t* count);
    char** (*get_string_array)(mcp_param_accessor_t* self, const char* name, size_t* count);
    int64_t* (*get_int_array)(mcp_param_accessor_t* self, const char* name, size_t* count);

    // Utility functions
    int (*has_param)(mcp_param_accessor_t* self, const char* name);
    size_t (*get_param_count)(mcp_param_accessor_t* self);

    // For rare complex cases: direct JSON access
    const cJSON* (*get_json)(mcp_param_accessor_t* self, const char* name);

    // Internal data
    void* data;
};

// Universal function signature - all pure functions use this
typedef void* (*mcp_universal_func_t)(mcp_param_accessor_t* params);



/**
 * Parameter categories - defines how parameters are structured
 */
typedef enum {
    MCP_PARAM_SINGLE,    // Single value parameter (int, double, string, bool)
    MCP_PARAM_ARRAY,     // Array of values parameter
    MCP_PARAM_OBJECT     // Complex JSON object parameter
} mcp_param_category_t;

/**
 * Array parameter description - used for array-type parameters
 */
typedef struct {
    mcp_param_type_t element_type;      // Type of elements in the array
    const char *element_description;    // Description of what each element represents
} mcp_array_desc_t;

/**
 * Parameter description structure - describes a single tool parameter
 * Used to automatically generate JSON Schema and handle parameter validation
 */
typedef struct {
    const char *name;                   // Parameter name (used in JSON)
    const char *description;            // Human-readable parameter description
    mcp_param_category_t category;      // Parameter category (single/array/object)
    int required;                       // 1 if required, 0 if optional

    union {
        mcp_param_type_t single_type;   // For single-value parameters
        mcp_array_desc_t array_desc;    // For array parameters
        const char *object_schema;      // JSON Schema string for complex objects
    };
} mcp_param_desc_t;

/**
 * Output description structure - describes tool return value
 * Used to generate outputSchema in MCP protocol
 */
typedef struct {
    const char *description;            // Human-readable output description
    const char *json_schema;            // Complete JSON Schema for output format
} mcp_output_desc_t;

// Transport types
typedef enum {
    EMBED_MCP_TRANSPORT_STDIO,
    EMBED_MCP_TRANSPORT_HTTP
} embed_mcp_transport_t;

/**
 * Server configuration structure
 * Used to configure MCP server behavior and transport settings
 */
typedef struct {
    const char *name;           // Server name (displayed in MCP protocol)
    const char *version;        // Server version (displayed in MCP protocol)
    const char *instructions;   // Server usage instructions (optional, displayed in MCP protocol)
    const char *host;           // HTTP bind address (default: "0.0.0.0")
    int port;                   // HTTP port number (default: 8080)
    const char *path;           // HTTP endpoint path (default: "/mcp")
    int max_tools;              // Maximum number of tools allowed (default: 100)
    int debug;                  // Enable debug logging (0=off, 1=on, default: 0)

    // Multi-session support
    int max_connections;        // Maximum concurrent connections (default: 10)
    int session_timeout;        // Session timeout in seconds (default: 3600)
    int enable_sessions;        // Enable session management (0=off, 1=on, default: 1)
    int auto_cleanup;           // Auto cleanup expired sessions (0=off, 1=on, default: 1)
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
 * Register a tool function with flexible parameter specification
 *
 * This is the main API for registering C functions as MCP tools.
 * It provides automatic type conversion between JSON and C types.
 *
 * Example:
 * ```c
 * int my_func(char c, int a, int b, char d) { return c + a + b + d; }
 *
 * const char* param_names[] = {"c", "a", "b", "d"};
 * mcp_param_type_t param_types[] = {MCP_PARAM_CHAR, MCP_PARAM_INT, MCP_PARAM_INT, MCP_PARAM_CHAR};
 *
 * embed_mcp_add_tool(server, "my_func", "My custom function",
 *                    param_names, param_types, 4, MCP_RETURN_INT, my_func);
 * ```
 *
 * @param server Server instance
 * @param name Tool name
 * @param description Tool description
 * @param param_names Array of parameter names
 * @param param_types Array of parameter types
 * @param param_count Number of parameters
 * @param return_type Return value type
 * @param function_ptr Pointer to user's function
 * @return 0 on success, -1 on error
 */
int embed_mcp_add_tool(embed_mcp_server_t *server,
                       const char *name,
                       const char *description,
                       const char *param_names[],
                       mcp_param_type_t param_types[],
                       size_t param_count,
                       mcp_return_type_t return_type,
                       void *function_ptr);

// =============================================================================
// Unified Pure Function API - No JSON handling required
// =============================================================================

/*
 * COMMENTED OUT: Pure Function API
 *
 * This API is kept for reference but commented out in favor of the more
 * flexible embed_mcp_add_tool API which provides better control
 * over parameter types and names.
 */

/*
 * Add a pure function tool with universal parameter handling
 * @param server Server instance
 * @param name Tool name
 * @param description Tool description
 * @param params Array of parameter descriptions
 * @param param_count Number of parameters
 * @param return_type Return type of the function
 * @param function_ptr Pointer to the universal pure function
 * @return 0 on success, -1 on error
 *
 * Universal function signature: void* func(mcp_param_accessor_t* params)
 *
 * Example usage:
 * ```c
 * double* my_function(mcp_param_accessor_t* params) {
 *     double a = params->get_double(params, "a");
 *     double b = params->get_double(params, "b");
 *     double* result = malloc(sizeof(double));
 *     *result = a + b;
 *     return result;
 * }
 * ```
 */
/*
int embed_mcp_add_pure_function(embed_mcp_server_t *server,
                                const char *name,
                                const char *description,
                                mcp_param_desc_t *params,
                                size_t param_count,
                                mcp_return_type_t return_type,
                                mcp_universal_func_t function_ptr);
*/

/*
 * RESERVED FOR FUTURE USE: Complex parameter structures
 *
 * This API is kept in code but commented out for future use when we encounter
 * complex nested parameter structures that the pure function API cannot handle.
 *
 * Uncomment when needed for scenarios like:
 * - Deep nested JSON objects (user.profile.preferences.notifications)
 * - Dynamic schema generation based on runtime conditions
 * - Complex validation rules (regex patterns, conditional requirements)
 *
 * Add a tool with custom JSON Schema (for complex parameter structures)
 * Use this when the pure function API cannot handle your parameter structure
 * @param server Server instance
 * @param name Tool name (must be unique)
 * @param description Tool description
 * @param schema JSON Schema for input validation (can be NULL for no validation)
 * @param handler Tool handler function that receives raw cJSON
 * @return 0 on success, -1 on error
 *
 * Example: Complex nested parameters that pure function API cannot handle
 */
int embed_mcp_add_tool_with_schema(embed_mcp_server_t *server,
                                   const char *name,
                                   const char *description,
                                   const cJSON *schema,
                                   embed_mcp_tool_handler_t handler);



/**
 * Run server with specified transport
 * This function blocks until the server is stopped
 * @param server Server instance
 * @param transport Transport type (EMBED_MCP_TRANSPORT_STDIO or EMBED_MCP_TRANSPORT_HTTP)
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
 * Get last error message
 * @return Error message string (do not free)
 */
const char *embed_mcp_get_error(void);

// =============================================================================
// Convenience Macros for Parameter Definitions
// =============================================================================

// Single parameter macros
#define MCP_PARAM_INT_DEF(name, desc, req) \
    {name, desc, MCP_PARAM_SINGLE, req, .single_type = MCP_PARAM_INT}

#define MCP_PARAM_DOUBLE_DEF(name, desc, req) \
    {name, desc, MCP_PARAM_SINGLE, req, .single_type = MCP_PARAM_DOUBLE}

#define MCP_PARAM_STRING_DEF(name, desc, req) \
    {name, desc, MCP_PARAM_SINGLE, req, .single_type = MCP_PARAM_STRING}

#define MCP_PARAM_BOOL_DEF(name, desc, req) \
    {name, desc, MCP_PARAM_SINGLE, req, .single_type = MCP_PARAM_BOOL}

// Array parameter macros
#define MCP_PARAM_ARRAY_INT_DEF(name, desc, elem_desc, req) \
    {name, desc, MCP_PARAM_ARRAY, req, .array_desc = {MCP_PARAM_INT, elem_desc}}

#define MCP_PARAM_ARRAY_DOUBLE_DEF(name, desc, elem_desc, req) \
    {name, desc, MCP_PARAM_ARRAY, req, .array_desc = {MCP_PARAM_DOUBLE, elem_desc}}

#define MCP_PARAM_ARRAY_STRING_DEF(name, desc, elem_desc, req) \
    {name, desc, MCP_PARAM_ARRAY, req, .array_desc = {MCP_PARAM_STRING, elem_desc}}

// Object parameter macro
#define MCP_PARAM_OBJECT_DEF(name, desc, schema, req) \
    {name, desc, MCP_PARAM_OBJECT, req, .object_schema = schema}

// Output description macro
#define MCP_OUTPUT_DESC(desc, schema) \
    &(mcp_output_desc_t){desc, schema}

// =============================================================================
// Resource API - MCP Resources Support
// =============================================================================

/**
 * Add a text resource to the server
 * @param server Server instance
 * @param uri Resource URI (unique identifier)
 * @param name Resource name
 * @param description Resource description (optional, can be NULL)
 * @param mime_type MIME type (optional, defaults to "text/plain")
 * @param content Text content
 * @return 0 on success, -1 on error
 */
int embed_mcp_add_text_resource(embed_mcp_server_t *server,
                                const char *uri,
                                const char *name,
                                const char *description,
                                const char *mime_type,
                                const char *content);

/**
 * Add a binary resource to the server
 * @param server Server instance
 * @param uri Resource URI (unique identifier)
 * @param name Resource name
 * @param description Resource description (optional, can be NULL)
 * @param mime_type MIME type (optional, defaults to "application/octet-stream")
 * @param data Binary data
 * @param size Size of binary data
 * @return 0 on success, -1 on error
 */
int embed_mcp_add_binary_resource(embed_mcp_server_t *server,
                                  const char *uri,
                                  const char *name,
                                  const char *description,
                                  const char *mime_type,
                                  const void *data,
                                  size_t size);

/**
 * Add a file resource to the server
 * @param server Server instance
 * @param uri Resource URI (unique identifier)
 * @param name Resource name
 * @param description Resource description (optional, can be NULL)
 * @param mime_type MIME type (optional, auto-detected from file extension)
 * @param file_path Path to file
 * @return 0 on success, -1 on error
 */
int embed_mcp_add_file_resource(embed_mcp_server_t *server,
                                const char *uri,
                                const char *name,
                                const char *description,
                                const char *mime_type,
                                const char *file_path);

/**
 * Function signature for dynamic text resource generation
 * @param user_data User-provided data pointer
 * @return Allocated string content (caller must free), or NULL on error
 */
typedef char* (*embed_mcp_text_resource_function_t)(void *user_data);

/**
 * Function signature for dynamic binary resource generation
 * @param user_data User-provided data pointer
 * @param data Output pointer to allocated data (caller must free)
 * @param size Output size of allocated data
 * @return 0 on success, -1 on error
 */
typedef int (*embed_mcp_binary_resource_function_t)(void *user_data, void **data, size_t *size);

/**
 * Add a dynamic text resource generated by a function
 * @param server Server instance
 * @param uri Resource URI (unique identifier)
 * @param name Resource name
 * @param description Resource description (optional, can be NULL)
 * @param mime_type MIME type (optional, defaults to "text/plain")
 * @param function Function to generate content
 * @param user_data User data passed to function
 * @return 0 on success, -1 on error
 */
int embed_mcp_add_text_function_resource(embed_mcp_server_t *server,
                                         const char *uri,
                                         const char *name,
                                         const char *description,
                                         const char *mime_type,
                                         embed_mcp_text_resource_function_t function,
                                         void *user_data);

/**
 * Add a dynamic binary resource generated by a function
 * @param server Server instance
 * @param uri Resource URI (unique identifier)
 * @param name Resource name
 * @param description Resource description (optional, can be NULL)
 * @param mime_type MIME type (optional, defaults to "application/octet-stream")
 * @param function Function to generate content
 * @param user_data User data passed to function
 * @return 0 on success, -1 on error
 */
int embed_mcp_add_binary_function_resource(embed_mcp_server_t *server,
                                           const char *uri,
                                           const char *name,
                                           const char *description,
                                           const char *mime_type,
                                           embed_mcp_binary_resource_function_t function,
                                           void *user_data);

/**
 * Get the number of registered resources
 * @param server Server instance
 * @return Number of resources, or 0 if server is NULL
 */
size_t embed_mcp_get_resource_count(embed_mcp_server_t *server);

// =============================================================================
// Resource Templates API
// =============================================================================

/**
 * Add a resource template to the server
 * @param server Server instance
 * @param template Resource template (ownership transferred to server)
 * @return 0 on success, -1 on error
 */
int embed_mcp_add_resource_template(embed_mcp_server_t *server, mcp_resource_template_t *template);

/**
 * Get the number of registered resource templates
 * @param server Server instance
 * @return Number of templates, or 0 if server is NULL
 */
size_t embed_mcp_get_resource_template_count(embed_mcp_server_t *server);

// Forward declarations for file resource handler
void mcp_file_resource_init(void);
void mcp_file_resource_cleanup(void);
int mcp_file_resource_handler(const mcp_resource_template_context_t *context,
                              mcp_resource_content_t *content);

#ifdef __cplusplus
}
#endif

#endif // EMBED_MCP_H
