#include "resource_interface.h"
#include "utils/logging.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>

// =============================================================================
// File Resource Handler - Simple file access
// =============================================================================

/**
 * Simple MIME type detection based on file extension
 */
static const char* get_mime_type_from_extension(const char *filename) {
    if (!filename) return "application/octet-stream";

    const char *ext = strrchr(filename, '.');
    if (!ext) return "application/octet-stream";

    ext++; // Skip the dot

    // Text files
    if (strcmp(ext, "txt") == 0) return "text/plain";
    if (strcmp(ext, "md") == 0) return "text/markdown";
    if (strcmp(ext, "html") == 0) return "text/html";
    if (strcmp(ext, "htm") == 0) return "text/html";
    if (strcmp(ext, "css") == 0) return "text/css";
    if (strcmp(ext, "js") == 0) return "application/javascript";
    if (strcmp(ext, "json") == 0) return "application/json";
    if (strcmp(ext, "xml") == 0) return "application/xml";

    // Programming languages
    if (strcmp(ext, "c") == 0) return "text/x-c";
    if (strcmp(ext, "h") == 0) return "text/x-c";
    if (strcmp(ext, "cpp") == 0) return "text/x-c++";
    if (strcmp(ext, "cxx") == 0) return "text/x-c++";
    if (strcmp(ext, "cc") == 0) return "text/x-c++";
    if (strcmp(ext, "py") == 0) return "text/x-python";
    if (strcmp(ext, "rs") == 0) return "text/x-rust";
    if (strcmp(ext, "go") == 0) return "text/x-go";
    if (strcmp(ext, "java") == 0) return "text/x-java";

    return "application/octet-stream";
}

/**
 * Check if a path is safe to access (basic security check)
 */
static int is_path_safe(const char *path) {
    if (!path) return 0;

    printf("[DEBUG] Checking path safety: '%s'\n", path);

    // Don't allow absolute paths outside current directory
    if (path[0] == '/') {
        printf("[DEBUG] Rejected: absolute path\n");
        return 0;
    }

    // Don't allow parent directory traversal
    if (strstr(path, "..") != NULL) {
        printf("[DEBUG] Rejected: parent directory traversal\n");
        return 0;
    }

    // Don't allow hidden files, but allow ./path
    if (path[0] == '.' && path[1] != '/') {
        printf("[DEBUG] Rejected: hidden file (path[0]='%c', path[1]='%c')\n", path[0], path[1]);
        return 0;
    }

    printf("[DEBUG] Path approved\n");
    return 1;
}

/**
 * File resource handler function
 */
int mcp_file_resource_handler(const mcp_resource_template_context_t *context,
                              mcp_resource_content_t *content) {
    if (!context || !context->resolved_uri || !content) {
        return -1;
    }

    const char *resolved_uri = context->resolved_uri;

    // Extract file path from URI (remove file:// prefix)
    const char *file_path = resolved_uri;
    if (strncmp(file_path, "file://", 7) == 0) {
        file_path += 7;
    }

    // Remove leading slash if present (make it relative)
    if (file_path[0] == '/') {
        file_path++;
    }

    // Security check
    if (!is_path_safe(file_path)) {
        printf("[FILE_RESOURCE] Access denied to path: %s\n", file_path);
        return -1;
    }

    // Check if file exists and get stats
    struct stat file_stat;
    if (stat(file_path, &file_stat) != 0) {
        printf("[FILE_RESOURCE] File not found: %s\n", file_path);
        return -1;
    }

    // Check if it's a regular file
    if (!S_ISREG(file_stat.st_mode)) {
        printf("[FILE_RESOURCE] Not a regular file: %s\n", file_path);
        return -1;
    }

    // Check file size (limit to 1MB for safety)
    if (file_stat.st_size > 1024 * 1024) {
        printf("[FILE_RESOURCE] File too large: %s (%lld bytes)\n", file_path, (long long)file_stat.st_size);
        return -1;
    }

    // Open and read file
    FILE *file = fopen(file_path, "rb");
    if (!file) {
        printf("[FILE_RESOURCE] Cannot open file: %s\n", file_path);
        return -1;
    }

    // Allocate buffer
    size_t file_size = file_stat.st_size;
    void *data = malloc(file_size + 1); // +1 for null terminator for text files
    if (!data) {
        fclose(file);
        return -1;
    }

    // Read file content
    size_t bytes_read = fread(data, 1, file_size, file);
    fclose(file);

    if (bytes_read != file_size) {
        free(data);
        printf("[FILE_RESOURCE] Failed to read complete file: %s\n", file_path);
        return -1;
    }

    // Determine MIME type
    const char *mime_type = get_mime_type_from_extension(file_path);

    // Check if it's a text file (simple heuristic)
    int is_text = (strncmp(mime_type, "text/", 5) == 0) ||
                  (strcmp(mime_type, "application/json") == 0) ||
                  (strcmp(mime_type, "application/xml") == 0) ||
                  (strcmp(mime_type, "application/javascript") == 0);

    // Fill content structure
    content->data = data;
    content->size = file_size;
    content->mime_type = strdup(mime_type);
    content->is_binary = is_text ? 0 : 1;

    if (is_text) {
        // Null-terminate text content
        ((char*)data)[file_size] = '\0';
    }

    printf("[FILE_RESOURCE] Successfully read file: %s (%zu bytes, %s)\n",
           file_path, file_size, mime_type);

    return 0;
}

/**
 * Initialize file resource system
 */
void mcp_file_resource_init(void) {
    printf("✅ File resource system initialized\n");
}

/**
 * Cleanup file resource system
 */
void mcp_file_resource_cleanup(void) {
    // Nothing to cleanup for basic version
}