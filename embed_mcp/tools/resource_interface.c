#include "resource_interface.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Resource content cleanup
void mcp_resource_content_cleanup(mcp_resource_content_t *content) {
    if (!content) return;
    
    if (content->data) {
        free(content->data);
        content->data = NULL;
    }
    if (content->mime_type) {
        free(content->mime_type);
        content->mime_type = NULL;
    }
    content->size = 0;
    content->is_binary = 0;
}

// Create a new resource descriptor
mcp_resource_desc_t *mcp_resource_desc_create(const char *uri,
                                              const char *name,
                                              const char *description,
                                              const char *mime_type,
                                              mcp_resource_type_t type) {
    if (!uri || !name) return NULL;
    
    mcp_resource_desc_t *resource = calloc(1, sizeof(mcp_resource_desc_t));
    if (!resource) return NULL;
    
    // Copy strings
    resource->uri = strdup(uri);
    resource->name = strdup(name);
    resource->description = description ? strdup(description) : NULL;
    resource->mime_type = strdup(mime_type ? mime_type : "text/plain");
    resource->type = type;
    resource->next = NULL;
    
    // Check allocation success
    if (!resource->uri || !resource->name || !resource->mime_type) {
        mcp_resource_desc_destroy(resource);
        return NULL;
    }
    
    return resource;
}

// Destroy a resource descriptor
void mcp_resource_desc_destroy(mcp_resource_desc_t *resource) {
    if (!resource) return;
    
    // Free strings
    free(resource->uri);
    free(resource->name);
    free(resource->description);
    free(resource->mime_type);
    
    // Free type-specific data
    switch (resource->type) {
        case MCP_RESOURCE_TEXT:
            free(resource->data.text.content);
            break;
        case MCP_RESOURCE_BINARY:
            free(resource->data.binary.data);
            break;
        case MCP_RESOURCE_FUNCTION:
            // user_data is managed by caller
            break;
        case MCP_RESOURCE_FILE:
            free(resource->data.file.path);
            break;
        case MCP_RESOURCE_HTTP:
            free(resource->data.http.url);
            break;
    }
    
    free(resource);
}

// Helper function to read file content
static int read_file_content(const char *path, mcp_resource_content_t *content, const char *mime_type) {
    FILE *file = fopen(path, "rb");
    if (!file) return -1;
    
    // Get file size
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (size < 0) {
        fclose(file);
        return -1;
    }
    
    // Allocate buffer
    void *data = malloc(size + 1); // +1 for null terminator
    if (!data) {
        fclose(file);
        return -1;
    }
    
    // Read file
    size_t read_size = fread(data, 1, size, file);
    fclose(file);
    
    if (read_size != (size_t)size) {
        free(data);
        return -1;
    }
    
    // Null terminate for text files
    ((char*)data)[size] = '\0';
    
    // Determine if binary based on MIME type
    int is_binary = mime_type && !strncmp(mime_type, "text/", 5) ? 0 : 1;
    
    content->data = data;
    content->size = size;
    content->mime_type = strdup(mime_type ? mime_type : "application/octet-stream");
    content->is_binary = is_binary;
    
    return 0;
}

// Read content from a resource
int mcp_resource_read_content(const mcp_resource_desc_t *resource, mcp_resource_content_t *content) {
    if (!resource || !content) return -1;
    
    // Initialize content
    memset(content, 0, sizeof(mcp_resource_content_t));
    
    switch (resource->type) {
        case MCP_RESOURCE_TEXT: {
            if (!resource->data.text.content) return -1;
            
            size_t len = strlen(resource->data.text.content);
            content->data = malloc(len + 1);
            if (!content->data) return -1;
            
            strcpy((char*)content->data, resource->data.text.content);
            content->size = len;
            content->mime_type = strdup(resource->mime_type);
            content->is_binary = 0;
            return 0;
        }
        
        case MCP_RESOURCE_BINARY: {
            if (!resource->data.binary.data || resource->data.binary.size == 0) return -1;
            
            content->data = malloc(resource->data.binary.size);
            if (!content->data) return -1;
            
            memcpy(content->data, resource->data.binary.data, resource->data.binary.size);
            content->size = resource->data.binary.size;
            content->mime_type = strdup(resource->mime_type);
            content->is_binary = 1;
            return 0;
        }
        
        case MCP_RESOURCE_FUNCTION: {
            if (resource->data.function.is_binary) {
                // Binary function
                if (!resource->data.function.binary_fn) return -1;
                
                void *data = NULL;
                size_t size = 0;
                if (resource->data.function.binary_fn(resource->data.function.user_data, &data, &size) != 0) {
                    return -1;
                }
                
                content->data = data;
                content->size = size;
                content->mime_type = strdup(resource->mime_type);
                content->is_binary = 1;
                return 0;
            } else {
                // Text function
                if (!resource->data.function.text_fn) return -1;
                
                char *text = resource->data.function.text_fn(resource->data.function.user_data);
                if (!text) return -1;
                
                content->data = text; // Transfer ownership
                content->size = strlen(text);
                content->mime_type = strdup(resource->mime_type);
                content->is_binary = 0;
                return 0;
            }
        }
        
        case MCP_RESOURCE_FILE: {
            if (!resource->data.file.path) return -1;
            return read_file_content(resource->data.file.path, content, resource->mime_type);
        }
        
        case MCP_RESOURCE_HTTP: {
            // HTTP resources not implemented yet
            // Would need HTTP client library
            return -1;
        }
        
        default:
            return -1;
    }
}
