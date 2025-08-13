#ifndef RESOURCE_INTERFACE_H
#define RESOURCE_INTERFACE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
typedef struct mcp_resource_registry mcp_resource_registry_t;
typedef struct mcp_resource_desc mcp_resource_desc_t;

/**
 * Resource types supported by EmbedMCP
 */
typedef enum {
    MCP_RESOURCE_TEXT,      // Static text content
    MCP_RESOURCE_BINARY,    // Static binary data
    MCP_RESOURCE_FUNCTION,  // Dynamic content from function
    MCP_RESOURCE_FILE,      // File path resource
    MCP_RESOURCE_HTTP       // HTTP URL resource
} mcp_resource_type_t;

/**
 * Resource content structure for returning data
 */
typedef struct {
    void *data;             // Content data (text or binary)
    size_t size;            // Size of data in bytes
    char *mime_type;        // MIME type of content (allocated, caller must free)
    int is_binary;          // 1 if binary data, 0 if text
} mcp_resource_content_t;

/**
 * Function signature for dynamic text resource generation
 * @param user_data User-provided data pointer
 * @return Allocated string content (caller must free), or NULL on error
 */
typedef char* (*mcp_resource_text_function_t)(void *user_data);

/**
 * Function signature for dynamic binary resource generation
 * @param user_data User-provided data pointer
 * @param data Output pointer to allocated data (caller must free)
 * @param size Output size of allocated data
 * @return 0 on success, -1 on error
 */
typedef int (*mcp_resource_binary_function_t)(void *user_data, void **data, size_t *size);

/**
 * Resource descriptor structure
 */
struct mcp_resource_desc {
    char *uri;              // Unique resource URI (allocated)
    char *name;             // Resource name (allocated)
    char *description;      // Resource description (allocated, optional)
    char *mime_type;        // MIME type (allocated)
    mcp_resource_type_t type; // Resource type
    
    // Type-specific data
    union {
        struct {
            char *content;  // Static text content (allocated)
        } text;
        
        struct {
            void *data;     // Static binary data (allocated)
            size_t size;    // Size of binary data
        } binary;
        
        struct {
            mcp_resource_text_function_t text_fn;     // Text function
            mcp_resource_binary_function_t binary_fn; // Binary function
            void *user_data;                          // User data for function
            int is_binary;                            // 1 if binary function, 0 if text
        } function;
        
        struct {
            char *path;     // File path (allocated)
        } file;
        
        struct {
            char *url;      // HTTP URL (allocated)
        } http;
    } data;
    
    // Linked list for registry
    mcp_resource_desc_t *next;
};

/**
 * Resource content cleanup function
 * @param content Resource content to cleanup
 */
void mcp_resource_content_cleanup(mcp_resource_content_t *content);

/**
 * Create a new resource descriptor
 * @param uri Resource URI (will be copied)
 * @param name Resource name (will be copied)
 * @param description Resource description (will be copied, can be NULL)
 * @param mime_type MIME type (will be copied)
 * @param type Resource type
 * @return Allocated resource descriptor, or NULL on error
 */
mcp_resource_desc_t *mcp_resource_desc_create(const char *uri,
                                              const char *name,
                                              const char *description,
                                              const char *mime_type,
                                              mcp_resource_type_t type);

/**
 * Destroy a resource descriptor and free all memory
 * @param resource Resource descriptor to destroy
 */
void mcp_resource_desc_destroy(mcp_resource_desc_t *resource);

/**
 * Read content from a resource
 * @param resource Resource descriptor
 * @param content Output content structure (caller must cleanup with mcp_resource_content_cleanup)
 * @return 0 on success, -1 on error
 */
int mcp_resource_read_content(const mcp_resource_desc_t *resource, mcp_resource_content_t *content);

// =============================================================================
// Resource Templates Support
// =============================================================================

/**
 * Resource template parameter definition
 */
typedef struct {
    char *name;           // Parameter name
    char *description;    // Parameter description (optional)
    int required;         // 1 if required, 0 if optional
} mcp_resource_template_param_t;

/**
 * Context passed to resource template handlers
 */
typedef struct {
    const char *resolved_uri;     // The resolved URI with parameters filled in
    char **param_names;           // Array of parameter names
    char **param_values;          // Array of parameter values
    size_t param_count;           // Number of parameters
    void *user_data;              // User-provided data
} mcp_resource_template_context_t;

/**
 * Resource template structure
 */
typedef struct mcp_resource_template {
    char *uri_template;           // URI template with {param} placeholders
    char *name;                   // Template name
    char *title;                  // Template title (optional)
    char *description;            // Template description (optional)
    char *mime_type;              // Default MIME type (optional)

    // Parameters
    mcp_resource_template_param_t *parameters;
    size_t parameter_count;

    // Handler function
    int (*handler)(const mcp_resource_template_context_t *context,
                   mcp_resource_content_t *content);
    void *user_data;              // User data passed to handler

    struct mcp_resource_template *next;  // Linked list
} mcp_resource_template_t;

/**
 * Create a new resource template
 */
mcp_resource_template_t *mcp_resource_template_create(const char *uri_template,
                                                      const char *name,
                                                      const char *title,
                                                      const char *description,
                                                      const char *mime_type);

/**
 * Destroy a resource template
 */
void mcp_resource_template_destroy(mcp_resource_template_t *template);

/**
 * Add a parameter to a resource template
 */
int mcp_resource_template_add_parameter(mcp_resource_template_t *template,
                                        const char *name,
                                        const char *description,
                                        int required);

/**
 * Set the handler function for a resource template
 */
void mcp_resource_template_set_handler(mcp_resource_template_t *template,
                                       int (*handler)(const mcp_resource_template_context_t *context,
                                                      mcp_resource_content_t *content),
                                       void *user_data);

/**
 * Check if a URI matches a template
 */
int mcp_resource_template_matches_uri(const char *uri_template, const char *uri);

/**
 * Parse URI against template and extract parameters
 */
int mcp_resource_template_parse_uri(const char *uri_template,
                                    const char *resolved_uri,
                                    char ***param_names,
                                    char ***param_values,
                                    size_t *param_count);

#ifdef __cplusplus
}
#endif

#endif // RESOURCE_INTERFACE_H
