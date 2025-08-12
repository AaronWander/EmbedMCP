#ifndef RESOURCE_REGISTRY_H
#define RESOURCE_REGISTRY_H

#include "resource_interface.h"
#include "cjson/cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Resource registry structure (opaque)
 */
struct mcp_resource_registry {
    mcp_resource_desc_t *resources;  // Linked list of resources
    size_t count;                    // Number of registered resources
    int enable_logging;              // Enable debug logging
};

/**
 * Create a new resource registry
 * @return Allocated registry, or NULL on error
 */
mcp_resource_registry_t *mcp_resource_registry_create(void);

/**
 * Destroy a resource registry and all registered resources
 * @param registry Registry to destroy
 */
void mcp_resource_registry_destroy(mcp_resource_registry_t *registry);

/**
 * Register a text resource
 * @param registry Resource registry
 * @param uri Resource URI
 * @param name Resource name
 * @param description Resource description (can be NULL)
 * @param mime_type MIME type (can be NULL, defaults to "text/plain")
 * @param content Text content (will be copied)
 * @return 0 on success, -1 on error
 */
int mcp_resource_registry_add_text(mcp_resource_registry_t *registry,
                                   const char *uri,
                                   const char *name,
                                   const char *description,
                                   const char *mime_type,
                                   const char *content);

/**
 * Register a binary resource
 * @param registry Resource registry
 * @param uri Resource URI
 * @param name Resource name
 * @param description Resource description (can be NULL)
 * @param mime_type MIME type (can be NULL, defaults to "application/octet-stream")
 * @param data Binary data (will be copied)
 * @param size Size of binary data
 * @return 0 on success, -1 on error
 */
int mcp_resource_registry_add_binary(mcp_resource_registry_t *registry,
                                     const char *uri,
                                     const char *name,
                                     const char *description,
                                     const char *mime_type,
                                     const void *data,
                                     size_t size);

/**
 * Register a function resource (text)
 * @param registry Resource registry
 * @param uri Resource URI
 * @param name Resource name
 * @param description Resource description (can be NULL)
 * @param mime_type MIME type (can be NULL, defaults to "text/plain")
 * @param function Text generation function
 * @param user_data User data for function
 * @return 0 on success, -1 on error
 */
int mcp_resource_registry_add_text_function(mcp_resource_registry_t *registry,
                                            const char *uri,
                                            const char *name,
                                            const char *description,
                                            const char *mime_type,
                                            mcp_resource_text_function_t function,
                                            void *user_data);

/**
 * Register a function resource (binary)
 * @param registry Resource registry
 * @param uri Resource URI
 * @param name Resource name
 * @param description Resource description (can be NULL)
 * @param mime_type MIME type (can be NULL, defaults to "application/octet-stream")
 * @param function Binary generation function
 * @param user_data User data for function
 * @return 0 on success, -1 on error
 */
int mcp_resource_registry_add_binary_function(mcp_resource_registry_t *registry,
                                              const char *uri,
                                              const char *name,
                                              const char *description,
                                              const char *mime_type,
                                              mcp_resource_binary_function_t function,
                                              void *user_data);

/**
 * Register a file resource
 * @param registry Resource registry
 * @param uri Resource URI
 * @param name Resource name
 * @param description Resource description (can be NULL)
 * @param mime_type MIME type (can be NULL, auto-detected from extension)
 * @param file_path Path to file (will be copied)
 * @return 0 on success, -1 on error
 */
int mcp_resource_registry_add_file(mcp_resource_registry_t *registry,
                                   const char *uri,
                                   const char *name,
                                   const char *description,
                                   const char *mime_type,
                                   const char *file_path);

/**
 * Find a resource by URI
 * @param registry Resource registry
 * @param uri Resource URI to find
 * @return Resource descriptor, or NULL if not found
 */
mcp_resource_desc_t *mcp_resource_registry_find(mcp_resource_registry_t *registry, const char *uri);

/**
 * Get the number of registered resources
 * @param registry Resource registry
 * @return Number of resources
 */
size_t mcp_resource_registry_count(mcp_resource_registry_t *registry);

/**
 * Generate JSON list of all resources (for resources/list response)
 * @param registry Resource registry
 * @return JSON array of resources, or NULL on error (caller must free with cJSON_Delete)
 */
cJSON *mcp_resource_registry_list_resources(mcp_resource_registry_t *registry);

/**
 * Read resource content by URI
 * @param registry Resource registry
 * @param uri Resource URI
 * @param content Output content structure (caller must cleanup)
 * @return 0 on success, -1 on error
 */
int mcp_resource_registry_read_resource(mcp_resource_registry_t *registry,
                                        const char *uri,
                                        mcp_resource_content_t *content);

/**
 * Enable or disable logging for the registry
 * @param registry Resource registry
 * @param enable 1 to enable logging, 0 to disable
 */
void mcp_resource_registry_set_logging(mcp_resource_registry_t *registry, int enable);

#ifdef __cplusplus
}
#endif

#endif // RESOURCE_REGISTRY_H
