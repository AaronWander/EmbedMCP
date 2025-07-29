#include "utils/logging.h"
#include <stdlib.h>
#include <string.h>

// Global logging configuration
static mcp_log_config_t *g_log_config = NULL;

// Initialize logging system
int mcp_log_init(const mcp_log_config_t *config) {
    if (g_log_config) {
        mcp_log_cleanup();
    }
    
    g_log_config = malloc(sizeof(mcp_log_config_t));
    if (!g_log_config) return -1;
    
    if (config) {
        *g_log_config = *config;
    } else {
        // Default configuration
        g_log_config->min_level = MCP_LOG_LEVEL_INFO;
        g_log_config->enable_timestamps = true;
        g_log_config->enable_colors = true;
        g_log_config->output_stream = stdout;
        g_log_config->error_stream = stderr;
    }
    
    return 0;
}

void mcp_log_cleanup(void) {
    if (g_log_config) {
        free(g_log_config);
        g_log_config = NULL;
    }
}

// Set log level
void mcp_log_set_level(mcp_log_level_t level) {
    if (g_log_config) {
        g_log_config->min_level = level;
    }
}

mcp_log_level_t mcp_log_get_level(void) {
    return g_log_config ? g_log_config->min_level : MCP_LOG_LEVEL_INFO;
}

// Generic logging function
void mcp_vlog(mcp_log_level_t level, const char *format, va_list args) {
    if (!g_log_config || level < g_log_config->min_level) {
        return;
    }
    
    FILE *stream = (level >= MCP_LOG_LEVEL_ERROR) ? g_log_config->error_stream : g_log_config->output_stream;
    
    // Add timestamp if enabled
    if (g_log_config->enable_timestamps) {
        time_t now = time(NULL);
        struct tm *tm_info = localtime(&now);
        fprintf(stream, "[%04d-%02d-%02d %02d:%02d:%02d] ",
                tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
                tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
    }
    
    // Add color if enabled
    if (g_log_config->enable_colors) {
        fprintf(stream, "%s", mcp_log_level_to_color(level));
    }
    
    // Add log level
    fprintf(stream, "[%s] ", mcp_log_level_to_string(level));
    
    // Add the actual message
    vfprintf(stream, format, args);
    
    // Reset color if enabled
    if (g_log_config->enable_colors) {
        fprintf(stream, "\033[0m");
    }
    
    // Add newline if not present
    if (format[strlen(format) - 1] != '\n') {
        fprintf(stream, "\n");
    }
    
    fflush(stream);
}

void mcp_log(mcp_log_level_t level, const char *format, ...) {
    va_list args;
    va_start(args, format);
    mcp_vlog(level, format, args);
    va_end(args);
}

// Convenience logging functions
void mcp_log_debug(const char *format, ...) {
    va_list args;
    va_start(args, format);
    mcp_vlog(MCP_LOG_LEVEL_DEBUG, format, args);
    va_end(args);
}

void mcp_log_info(const char *format, ...) {
    va_list args;
    va_start(args, format);
    mcp_vlog(MCP_LOG_LEVEL_INFO, format, args);
    va_end(args);
}

void mcp_log_warn(const char *format, ...) {
    va_list args;
    va_start(args, format);
    mcp_vlog(MCP_LOG_LEVEL_WARN, format, args);
    va_end(args);
}

void mcp_log_error(const char *format, ...) {
    va_list args;
    va_start(args, format);
    mcp_vlog(MCP_LOG_LEVEL_ERROR, format, args);
    va_end(args);
}

// Debug print function (for backward compatibility)
#ifdef DEBUG
void mcp_debug_print(const char *format, ...) {
    va_list args;
    va_start(args, format);
    mcp_vlog(MCP_LOG_LEVEL_DEBUG, format, args);
    va_end(args);
}
#endif

// Utility functions
const char *mcp_log_level_to_string(mcp_log_level_t level) {
    switch (level) {
        case MCP_LOG_LEVEL_DEBUG: return "DEBUG";
        case MCP_LOG_LEVEL_INFO:  return "INFO";
        case MCP_LOG_LEVEL_WARN:  return "WARN";
        case MCP_LOG_LEVEL_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

const char *mcp_log_level_to_color(mcp_log_level_t level) {
    switch (level) {
        case MCP_LOG_LEVEL_DEBUG: return "\033[36m"; // Cyan
        case MCP_LOG_LEVEL_INFO:  return "\033[32m"; // Green
        case MCP_LOG_LEVEL_WARN:  return "\033[33m"; // Yellow
        case MCP_LOG_LEVEL_ERROR: return "\033[31m"; // Red
        default: return "\033[0m"; // Reset
    }
}

// Configuration helpers
mcp_log_config_t *mcp_log_config_create_default(void) {
    mcp_log_config_t *config = malloc(sizeof(mcp_log_config_t));
    if (!config) return NULL;
    
    config->min_level = MCP_LOG_LEVEL_INFO;
    config->enable_timestamps = true;
    config->enable_colors = true;
    config->output_stream = stdout;
    config->error_stream = stderr;
    
    return config;
}

void mcp_log_config_destroy(mcp_log_config_t *config) {
    if (config) {
        free(config);
    }
}
