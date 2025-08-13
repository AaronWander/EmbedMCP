#include "resource_registry.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Helper function to detect MIME type from file extension
static const char *detect_mime_type(const char *file_path) {
    if (!file_path) return "application/octet-stream";
    
    const char *ext = strrchr(file_path, '.');
    if (!ext) return "application/octet-stream";
    
    ext++; // Skip the dot
    
    // Common text types
    if (strcmp(ext, "txt") == 0) return "text/plain";
    if (strcmp(ext, "json") == 0) return "application/json";
    if (strcmp(ext, "xml") == 0) return "application/xml";
    if (strcmp(ext, "html") == 0 || strcmp(ext, "htm") == 0) return "text/html";
    if (strcmp(ext, "css") == 0) return "text/css";
    if (strcmp(ext, "js") == 0) return "application/javascript";
    if (strcmp(ext, "md") == 0) return "text/markdown";
    if (strcmp(ext, "csv") == 0) return "text/csv";
    
    // Programming languages
    if (strcmp(ext, "c") == 0 || strcmp(ext, "h") == 0) return "text/x-c";
    if (strcmp(ext, "cpp") == 0 || strcmp(ext, "hpp") == 0) return "text/x-c++";
    if (strcmp(ext, "py") == 0) return "text/x-python";
    if (strcmp(ext, "rs") == 0) return "text/x-rust";
    if (strcmp(ext, "go") == 0) return "text/x-go";
    
    // Binary types
    if (strcmp(ext, "png") == 0) return "image/png";
    if (strcmp(ext, "jpg") == 0 || strcmp(ext, "jpeg") == 0) return "image/jpeg";
    if (strcmp(ext, "gif") == 0) return "image/gif";
    if (strcmp(ext, "pdf") == 0) return "application/pdf";
    if (strcmp(ext, "zip") == 0) return "application/zip";
    
    return "application/octet-stream";
}

// Create a new resource registry
mcp_resource_registry_t *mcp_resource_registry_create(void) {
    mcp_resource_registry_t *registry = calloc(1, sizeof(mcp_resource_registry_t));
    if (!registry) return NULL;

    registry->resources = NULL;
    registry->count = 0;
    registry->enable_logging = 0;

    // Initialize templates
    registry->templates = NULL;
    registry->template_count = 0;

    return registry;
}

// Destroy a resource registry
void mcp_resource_registry_destroy(mcp_resource_registry_t *registry) {
    if (!registry) return;

    // Free all resources
    mcp_resource_desc_t *current = registry->resources;
    while (current) {
        mcp_resource_desc_t *next = current->next;
        mcp_resource_desc_destroy(current);
        current = next;
    }

    // Free all templates
    mcp_resource_template_t *template_current = registry->templates;
    while (template_current) {
        mcp_resource_template_t *template_next = template_current->next;
        mcp_resource_template_destroy(template_current);
        template_current = template_next;
    }

    free(registry);
}

// Helper function to add a resource to the registry
static int add_resource_to_registry(mcp_resource_registry_t *registry, mcp_resource_desc_t *resource) {
    if (!registry || !resource) return -1;
    
    // Check for duplicate URI
    if (mcp_resource_registry_find(registry, resource->uri)) {
        if (registry->enable_logging) {
            fprintf(stderr, "[RESOURCE] Warning: Resource with URI '%s' already exists\n", resource->uri);
        }
        mcp_resource_desc_destroy(resource);
        return -1;
    }
    
    // Add to front of list
    resource->next = registry->resources;
    registry->resources = resource;
    registry->count++;
    
    if (registry->enable_logging) {
        fprintf(stderr, "[RESOURCE] Registered resource: %s (%s)\n", resource->name, resource->uri);
    }
    
    return 0;
}

// Register a text resource
int mcp_resource_registry_add_text(mcp_resource_registry_t *registry,
                                   const char *uri,
                                   const char *name,
                                   const char *description,
                                   const char *mime_type,
                                   const char *content) {
    if (!registry || !uri || !name || !content) return -1;
    
    mcp_resource_desc_t *resource = mcp_resource_desc_create(uri, name, description, 
                                                            mime_type ? mime_type : "text/plain", 
                                                            MCP_RESOURCE_TEXT);
    if (!resource) return -1;
    
    // Copy content
    resource->data.text.content = strdup(content);
    if (!resource->data.text.content) {
        mcp_resource_desc_destroy(resource);
        return -1;
    }
    
    return add_resource_to_registry(registry, resource);
}

// Register a binary resource
int mcp_resource_registry_add_binary(mcp_resource_registry_t *registry,
                                     const char *uri,
                                     const char *name,
                                     const char *description,
                                     const char *mime_type,
                                     const void *data,
                                     size_t size) {
    if (!registry || !uri || !name || !data || size == 0) return -1;
    
    mcp_resource_desc_t *resource = mcp_resource_desc_create(uri, name, description,
                                                            mime_type ? mime_type : "application/octet-stream",
                                                            MCP_RESOURCE_BINARY);
    if (!resource) return -1;
    
    // Copy data
    resource->data.binary.data = malloc(size);
    if (!resource->data.binary.data) {
        mcp_resource_desc_destroy(resource);
        return -1;
    }
    
    memcpy(resource->data.binary.data, data, size);
    resource->data.binary.size = size;

    return add_resource_to_registry(registry, resource);
}

// Register a text function resource
int mcp_resource_registry_add_text_function(mcp_resource_registry_t *registry,
                                            const char *uri,
                                            const char *name,
                                            const char *description,
                                            const char *mime_type,
                                            mcp_resource_text_function_t function,
                                            void *user_data) {
    if (!registry || !uri || !name || !function) return -1;

    mcp_resource_desc_t *resource = mcp_resource_desc_create(uri, name, description,
                                                            mime_type ? mime_type : "text/plain",
                                                            MCP_RESOURCE_FUNCTION);
    if (!resource) return -1;

    resource->data.function.text_fn = function;
    resource->data.function.binary_fn = NULL;
    resource->data.function.user_data = user_data;
    resource->data.function.is_binary = 0;

    return add_resource_to_registry(registry, resource);
}

// Register a binary function resource
int mcp_resource_registry_add_binary_function(mcp_resource_registry_t *registry,
                                              const char *uri,
                                              const char *name,
                                              const char *description,
                                              const char *mime_type,
                                              mcp_resource_binary_function_t function,
                                              void *user_data) {
    if (!registry || !uri || !name || !function) return -1;

    mcp_resource_desc_t *resource = mcp_resource_desc_create(uri, name, description,
                                                            mime_type ? mime_type : "application/octet-stream",
                                                            MCP_RESOURCE_FUNCTION);
    if (!resource) return -1;

    resource->data.function.text_fn = NULL;
    resource->data.function.binary_fn = function;
    resource->data.function.user_data = user_data;
    resource->data.function.is_binary = 1;

    return add_resource_to_registry(registry, resource);
}

// Register a file resource
int mcp_resource_registry_add_file(mcp_resource_registry_t *registry,
                                   const char *uri,
                                   const char *name,
                                   const char *description,
                                   const char *mime_type,
                                   const char *file_path) {
    if (!registry || !uri || !name || !file_path) return -1;

    // Auto-detect MIME type if not provided
    const char *detected_mime = mime_type ? mime_type : detect_mime_type(file_path);

    mcp_resource_desc_t *resource = mcp_resource_desc_create(uri, name, description,
                                                            detected_mime,
                                                            MCP_RESOURCE_FILE);
    if (!resource) return -1;

    // Copy file path
    resource->data.file.path = strdup(file_path);
    if (!resource->data.file.path) {
        mcp_resource_desc_destroy(resource);
        return -1;
    }

    return add_resource_to_registry(registry, resource);
}

// Find a resource by URI
mcp_resource_desc_t *mcp_resource_registry_find(mcp_resource_registry_t *registry, const char *uri) {
    if (!registry || !uri) return NULL;

    mcp_resource_desc_t *current = registry->resources;
    while (current) {
        if (strcmp(current->uri, uri) == 0) {
            return current;
        }
        current = current->next;
    }

    return NULL;
}

// Get the number of registered resources
size_t mcp_resource_registry_count(mcp_resource_registry_t *registry) {
    return registry ? registry->count : 0;
}

// Generate JSON list of all resources
cJSON *mcp_resource_registry_list_resources(mcp_resource_registry_t *registry) {
    if (!registry) return NULL;

    cJSON *resources_array = cJSON_CreateArray();
    if (!resources_array) return NULL;

    mcp_resource_desc_t *current = registry->resources;
    while (current) {
        cJSON *resource_obj = cJSON_CreateObject();
        if (!resource_obj) {
            cJSON_Delete(resources_array);
            return NULL;
        }

        cJSON_AddStringToObject(resource_obj, "uri", current->uri);
        cJSON_AddStringToObject(resource_obj, "name", current->name);
        if (current->description) {
            cJSON_AddStringToObject(resource_obj, "description", current->description);
        }
        cJSON_AddStringToObject(resource_obj, "mimeType", current->mime_type);

        cJSON_AddItemToArray(resources_array, resource_obj);
        current = current->next;
    }

    return resources_array;
}

// Read resource content by URI
int mcp_resource_registry_read_resource(mcp_resource_registry_t *registry,
                                        const char *uri,
                                        mcp_resource_content_t *content) {
    if (!registry || !uri || !content) return -1;

    mcp_resource_desc_t *resource = mcp_resource_registry_find(registry, uri);
    if (!resource) return -1;

    return mcp_resource_read_content(resource, content);
}

// Enable or disable logging
void mcp_resource_registry_set_logging(mcp_resource_registry_t *registry, int enable) {
    if (registry) {
        registry->enable_logging = enable;
    }
}

// =============================================================================
// Resource Templates Implementation
// =============================================================================

int mcp_resource_registry_add_template(mcp_resource_registry_t *registry,
                                       mcp_resource_template_t *template) {
    if (!registry || !template) {
        return -1;
    }

    // Check for duplicate template name
    mcp_resource_template_t *current = registry->templates;
    while (current) {
        if (strcmp(current->name, template->name) == 0) {
            if (registry->enable_logging) {
                fprintf(stderr, "[RESOURCE] Warning: Template with name '%s' already exists\n", template->name);
            }
            return -1;
        }
        current = current->next;
    }

    // Add to linked list
    template->next = registry->templates;
    registry->templates = template;
    registry->template_count++;

    if (registry->enable_logging) {
        printf("✅ Registered %s template (%s)\n", template->name, template->uri_template);
    }

    return 0;
}

size_t mcp_resource_registry_template_count(mcp_resource_registry_t *registry) {
    return registry ? registry->template_count : 0;
}

cJSON *mcp_resource_registry_list_templates(mcp_resource_registry_t *registry) {
    if (!registry) return NULL;

    cJSON *templates_array = cJSON_CreateArray();
    if (!templates_array) return NULL;

    mcp_resource_template_t *current = registry->templates;
    while (current) {
        cJSON *template_obj = cJSON_CreateObject();
        if (!template_obj) {
            cJSON_Delete(templates_array);
            return NULL;
        }

        cJSON_AddStringToObject(template_obj, "uriTemplate", current->uri_template);
        cJSON_AddStringToObject(template_obj, "name", current->name);
        if (current->title) {
            cJSON_AddStringToObject(template_obj, "title", current->title);
        }
        if (current->description) {
            cJSON_AddStringToObject(template_obj, "description", current->description);
        }
        if (current->mime_type) {
            cJSON_AddStringToObject(template_obj, "mimeType", current->mime_type);
        }

        cJSON_AddItemToArray(templates_array, template_obj);
        current = current->next;
    }

    return templates_array;
}

mcp_resource_template_t *mcp_resource_registry_find_template(mcp_resource_registry_t *registry,
                                                             const char *uri) {
    if (!registry || !uri) return NULL;

    mcp_resource_template_t *current = registry->templates;
    while (current) {
        if (mcp_resource_template_matches_uri(current->uri_template, uri)) {
            return current;
        }
        current = current->next;
    }

    return NULL;
}

int mcp_resource_registry_read_template(mcp_resource_registry_t *registry,
                                        const char *uri,
                                        mcp_resource_content_t *content) {
    if (!registry || !uri || !content) {
        return -1;
    }

    mcp_resource_template_t *template = mcp_resource_registry_find_template(registry, uri);
    if (!template || !template->handler) {
        return -1;
    }

    // Parse URI parameters
    char **param_names = NULL;
    char **param_values = NULL;
    size_t param_count = 0;

    int parse_result = mcp_resource_template_parse_uri(template->uri_template, uri,
                                                       &param_names, &param_values, &param_count);
    if (parse_result != 0) {
        return -1;
    }

    // Create context
    mcp_resource_template_context_t context = {
        .resolved_uri = uri,
        .param_names = param_names,
        .param_values = param_values,
        .param_count = param_count,
        .user_data = template->user_data
    };

    // Call handler
    int result = template->handler(&context, content);

    // Clean up parameters
    if (param_names) {
        for (size_t i = 0; i < param_count; i++) {
            free(param_names[i]);
        }
        free(param_names);
    }
    if (param_values) {
        for (size_t i = 0; i < param_count; i++) {
            free(param_values[i]);
        }
        free(param_values);
    }

    return result;
}
