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
    MCP_PARAM_BOOL
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
typedef void* (*mcp_universal_func_t)(mcp_param_accessor_t* params, void* user_data);



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
 * Register a tool using universal function wrapper
 *
 * This method allows registering any function signature by providing a wrapper
 * function that handles parameter extraction and type conversion.
 *
 * Example:
 * ```c
 * // Original function
 * int add(int a, int b) { return a + b; }
 *
 * // Wrapper function
 * void* add_wrapper(mcp_param_accessor_t* params, void* user_data) {
 *     int a = (int)params->get_int(params, "a");
 *     int b = (int)params->get_int(params, "b");
 *     int result = add(a, b);
 *     int* ret = malloc(sizeof(int));
 *     *ret = result;
 *     return ret;
 * }
 *
 * // Register the tool
 * const char* param_names[] = {"a", "b"};
 * const char* param_descriptions[] = {"First number", "Second number"};
 * mcp_param_type_t param_types[] = {MCP_PARAM_INT, MCP_PARAM_INT};
 *
 * embed_mcp_add_tool(server, "add", "Add two numbers",
 *                    param_names, param_descriptions, param_types, 2,
 *                    MCP_RETURN_INT, add_wrapper, NULL);
 * ```
 *
 * @param server Server instance
 * @param name Tool name
 * @param description Tool description
 * @param param_names Array of parameter names
 * @param param_descriptions Array of parameter descriptions
 * @param param_types Array of parameter types
 * @param param_count Number of parameters
 * @param return_type Return value type
 * @param wrapper_func Universal wrapper function
 * @param user_data Optional user data passed to wrapper function
 * @return 0 on success, -1 on error
 */
int embed_mcp_add_tool(embed_mcp_server_t *server,
                       const char *name,
                       const char *description,
                       const char *param_names[],
                       const char *param_descriptions[],
                       mcp_param_type_t param_types[],
                       size_t param_count,
                       mcp_return_type_t return_type,
                       mcp_universal_func_t wrapper_func,
                       void *user_data);



// =============================================================================
// Universal Macro System for Wrapper Function Generation
// =============================================================================

/**
 * Universal macro system that supports ANY function signature with variable arguments.
 *
 * Usage:
 * ```c
 * // Define your business functions
 * int add(int a, int b) { return a + b; }
 * double calc(int x, double y, const char* mode) { return x * y; }
 * void print_msg(const char* msg, int level, bool urgent) { printf(...); }
 *
 * // Generate wrapper functions - ONE LINE each, ANY signature!
 * EMBED_MCP_WRAPPER(add_wrapper, add, INT, INT(a), INT(b))
 * EMBED_MCP_WRAPPER(calc_wrapper, calc, DOUBLE, INT(x), DOUBLE(y), STRING(mode))
 * EMBED_MCP_WRAPPER(print_wrapper, print_msg, VOID, STRING(msg), INT(level), BOOL(urgent))
 *
 * // Register normally
 * const char* names[] = {"a", "b"};
 * const char* descs[] = {"First", "Second"};
 * mcp_param_type_t types[] = {MCP_PARAM_INT, MCP_PARAM_INT};
 * embed_mcp_add_tool(server, "add", "Add", names, descs, types, 2, MCP_RETURN_INT, add_wrapper, NULL);
 * ```
 */

/**
 * Truly Universal Wrapper Macro System
 *
 * This system uses recursive macro expansion to support ANY number of parameters.
 * It uses the X-Macro pattern and recursive processing to handle variable arguments.
 */

// =============================================================================
// Core Macro Infrastructure
// =============================================================================

// Argument counting - supports up to 64 arguments
#define GET_ARG_COUNT(...) GET_ARG_COUNT_IMPL(__VA_ARGS__, \
    64,63,62,61,60,59,58,57,56,55,54,53,52,51,50,49,48,47,46,45,44,43,42,41,40, \
    39,38,37,36,35,34,33,32,31,30,29,28,27,26,25,24,23,22,21,20,19,18,17,16,15, \
    14,13,12,11,10,9,8,7,6,5,4,3,2,1,0)

#define GET_ARG_COUNT_IMPL( \
    _1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_20, \
    _21,_22,_23,_24,_25,_26,_27,_28,_29,_30,_31,_32,_33,_34,_35,_36,_37,_38,_39,_40, \
    _41,_42,_43,_44,_45,_46,_47,_48,_49,_50,_51,_52,_53,_54,_55,_56,_57,_58,_59,_60, \
    _61,_62,_63,_64,N,...) N

// Macro concatenation helpers
#define CONCAT(a, b) CONCAT_IMPL(a, b)
#define CONCAT_IMPL(a, b) a##b

// =============================================================================
// Parameter Processing System
// =============================================================================

// Parameter pair processing (type, name) -> extraction code
#define PROCESS_PARAM_PAIR(type, name) type##_EXTRACT(name, params);

// Recursive parameter processing using FOR_EACH pattern
#define FOR_EACH_PAIR(macro, ...) FOR_EACH_PAIR_IMPL(GET_ARG_COUNT(__VA_ARGS__), macro, __VA_ARGS__)
#define FOR_EACH_PAIR_IMPL(count, macro, ...) CONCAT(FOR_EACH_PAIR_, count)(macro, __VA_ARGS__)

// FOR_EACH implementations for different argument counts
#define FOR_EACH_PAIR_0(macro)
#define FOR_EACH_PAIR_1(macro, t1) // Invalid - single argument without pair
#define FOR_EACH_PAIR_2(macro, t1, n1) macro(t1, n1)
#define FOR_EACH_PAIR_3(macro, t1, n1, t2) // Invalid - odd number
#define FOR_EACH_PAIR_4(macro, t1, n1, t2, n2) macro(t1, n1) macro(t2, n2)
#define FOR_EACH_PAIR_5(macro, t1, n1, t2, n2, t3) // Invalid - odd number
#define FOR_EACH_PAIR_6(macro, t1, n1, t2, n2, t3, n3) macro(t1, n1) macro(t2, n2) macro(t3, n3)
#define FOR_EACH_PAIR_7(macro, t1, n1, t2, n2, t3, n3, t4) // Invalid - odd number
#define FOR_EACH_PAIR_8(macro, t1, n1, t2, n2, t3, n3, t4, n4) macro(t1, n1) macro(t2, n2) macro(t3, n3) macro(t4, n4)
#define FOR_EACH_PAIR_9(macro, t1, n1, t2, n2, t3, n3, t4, n4, t5) // Invalid - odd number
#define FOR_EACH_PAIR_10(macro, t1, n1, t2, n2, t3, n3, t4, n4, t5, n5) macro(t1, n1) macro(t2, n2) macro(t3, n3) macro(t4, n4) macro(t5, n5)
#define FOR_EACH_PAIR_11(macro, t1, n1, t2, n2, t3, n3, t4, n4, t5, n5, t6) // Invalid - odd number
#define FOR_EACH_PAIR_12(macro, t1, n1, t2, n2, t3, n3, t4, n4, t5, n5, t6, n6) macro(t1, n1) macro(t2, n2) macro(t3, n3) macro(t4, n4) macro(t5, n5) macro(t6, n6)
#define FOR_EACH_PAIR_13(macro, t1, n1, t2, n2, t3, n3, t4, n4, t5, n5, t6, n6, t7) // Invalid - odd number
#define FOR_EACH_PAIR_14(macro, t1, n1, t2, n2, t3, n3, t4, n4, t5, n5, t6, n6, t7, n7) macro(t1, n1) macro(t2, n2) macro(t3, n3) macro(t4, n4) macro(t5, n5) macro(t6, n6) macro(t7, n7)
#define FOR_EACH_PAIR_15(macro, t1, n1, t2, n2, t3, n3, t4, n4, t5, n5, t6, n6, t7, n7, t8) // Invalid - odd number
#define FOR_EACH_PAIR_16(macro, t1, n1, t2, n2, t3, n3, t4, n4, t5, n5, t6, n6, t7, n7, t8, n8) macro(t1, n1) macro(t2, n2) macro(t3, n3) macro(t4, n4) macro(t5, n5) macro(t6, n6) macro(t7, n7) macro(t8, n8)
#define FOR_EACH_PAIR_17(macro, t1, n1, t2, n2, t3, n3, t4, n4, t5, n5, t6, n6, t7, n7, t8, n8, t9) // Invalid - odd number
#define FOR_EACH_PAIR_18(macro, t1, n1, t2, n2, t3, n3, t4, n4, t5, n5, t6, n6, t7, n7, t8, n8, t9, n9) macro(t1, n1) macro(t2, n2) macro(t3, n3) macro(t4, n4) macro(t5, n5) macro(t6, n6) macro(t7, n7) macro(t8, n8) macro(t9, n9)
#define FOR_EACH_PAIR_19(macro, t1, n1, t2, n2, t3, n3, t4, n4, t5, n5, t6, n6, t7, n7, t8, n8, t9, n9, t10) // Invalid - odd number
#define FOR_EACH_PAIR_20(macro, t1, n1, t2, n2, t3, n3, t4, n4, t5, n5, t6, n6, t7, n7, t8, n8, t9, n9, t10, n10) macro(t1, n1) macro(t2, n2) macro(t3, n3) macro(t4, n4) macro(t5, n5) macro(t6, n6) macro(t7, n7) macro(t8, n8) macro(t9, n9) macro(t10, n10)

// =============================================================================
// Function Call System
// =============================================================================

// Extract parameter names for function calls
#define GET_PARAM_NAME(type, name) name,
#define GET_PARAM_NAMES_0()
#define GET_PARAM_NAMES_1() // Invalid
#define GET_PARAM_NAMES_2(t1, n1) n1
#define GET_PARAM_NAMES_3() // Invalid
#define GET_PARAM_NAMES_4(t1, n1, t2, n2) n1, n2
#define GET_PARAM_NAMES_5() // Invalid
#define GET_PARAM_NAMES_6(t1, n1, t2, n2, t3, n3) n1, n2, n3
#define GET_PARAM_NAMES_7() // Invalid
#define GET_PARAM_NAMES_8(t1, n1, t2, n2, t3, n3, t4, n4) n1, n2, n3, n4
#define GET_PARAM_NAMES_9() // Invalid
#define GET_PARAM_NAMES_10(t1, n1, t2, n2, t3, n3, t4, n4, t5, n5) n1, n2, n3, n4, n5
#define GET_PARAM_NAMES_11() // Invalid
#define GET_PARAM_NAMES_12(t1, n1, t2, n2, t3, n3, t4, n4, t5, n5, t6, n6) n1, n2, n3, n4, n5, n6
#define GET_PARAM_NAMES_13() // Invalid
#define GET_PARAM_NAMES_14(t1, n1, t2, n2, t3, n3, t4, n4, t5, n5, t6, n6, t7, n7) n1, n2, n3, n4, n5, n6, n7
#define GET_PARAM_NAMES_15() // Invalid
#define GET_PARAM_NAMES_16(t1, n1, t2, n2, t3, n3, t4, n4, t5, n5, t6, n6, t7, n7, t8, n8) n1, n2, n3, n4, n5, n6, n7, n8
#define GET_PARAM_NAMES_17() // Invalid
#define GET_PARAM_NAMES_18(t1, n1, t2, n2, t3, n3, t4, n4, t5, n5, t6, n6, t7, n7, t8, n8, t9, n9) n1, n2, n3, n4, n5, n6, n7, n8, n9
#define GET_PARAM_NAMES_19() // Invalid
#define GET_PARAM_NAMES_20(t1, n1, t2, n2, t3, n3, t4, n4, t5, n5, t6, n6, t7, n7, t8, n8, t9, n9, t10, n10) n1, n2, n3, n4, n5, n6, n7, n8, n9, n10

// Function call generation
#define CALL_FUNCTION_WITH_PARAMS(func, ...) func(CONCAT(GET_PARAM_NAMES_, GET_ARG_COUNT(__VA_ARGS__))(__VA_ARGS__))

// =============================================================================
// Type System
// =============================================================================

// Type extraction helpers
#define INT_EXTRACT(name, params) int name = (int)params->get_int(params, #name)
#define DOUBLE_EXTRACT(name, params) double name = params->get_double(params, #name)
#define STRING_EXTRACT(name, params) const char* name = params->get_string(params, #name)
#define BOOL_EXTRACT(name, params) bool name = params->get_bool(params, #name)

// Return value helpers
#define INT_RETURN(result) do { int* ret = malloc(sizeof(int)); *ret = (result); return ret; } while(0)
#define DOUBLE_RETURN(result) do { double* ret = malloc(sizeof(double)); *ret = (result); return ret; } while(0)
#define STRING_RETURN(result) return (result)
#define BOOL_RETURN(result) do { int* ret = malloc(sizeof(int)); *ret = (result); return ret; } while(0)
#define VOID_RETURN(result) do { (void)(result); return NULL; } while(0)

// =============================================================================
// Universal Wrapper Macro
// =============================================================================

/**
 * The ultimate universal wrapper macro that supports ANY number of parameters!
 *
 * Usage:
 * EMBED_MCP_WRAPPER(wrapper_name, function_name, return_type, type1, name1, type2, name2, ...)
 *
 * Examples:
 * EMBED_MCP_WRAPPER(add_wrapper, add, INT, INT, a, INT, b)
 * EMBED_MCP_WRAPPER(complex_wrapper, complex_func, DOUBLE, INT, x, DOUBLE, y, STRING, mode, BOOL, flag)
 * EMBED_MCP_WRAPPER(no_param_wrapper, no_param_func, INT)
 */
#define EMBED_MCP_WRAPPER(wrapper_name, func_name, return_type, ...) \
    void* wrapper_name(mcp_param_accessor_t* params, void* user_data) { \
        (void)user_data; \
        FOR_EACH_PAIR(PROCESS_PARAM_PAIR, __VA_ARGS__) \
        return_type##_RETURN(CALL_FUNCTION_WITH_PARAMS(func_name, __VA_ARGS__)); \
    }



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
