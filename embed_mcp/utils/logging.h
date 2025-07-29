#ifndef MCP_LOGGING_H
#define MCP_LOGGING_H

#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <stdbool.h>

// Log levels
typedef enum {
    MCP_LOG_LEVEL_DEBUG = 0,
    MCP_LOG_LEVEL_INFO = 1,
    MCP_LOG_LEVEL_WARN = 2,
    MCP_LOG_LEVEL_ERROR = 3
} mcp_log_level_t;

// Logging configuration
typedef struct {
    mcp_log_level_t min_level;
    bool enable_timestamps;
    bool enable_colors;
    FILE *output_stream;
    FILE *error_stream;
} mcp_log_config_t;

// Initialize logging system
int mcp_log_init(const mcp_log_config_t *config);
void mcp_log_cleanup(void);

// Set log level
void mcp_log_set_level(mcp_log_level_t level);
mcp_log_level_t mcp_log_get_level(void);

// Logging functions
void mcp_log_debug(const char *format, ...);
void mcp_log_info(const char *format, ...);
void mcp_log_warn(const char *format, ...);
void mcp_log_error(const char *format, ...);

// Generic logging function
void mcp_log(mcp_log_level_t level, const char *format, ...);
void mcp_vlog(mcp_log_level_t level, const char *format, va_list args);

// Debug print function (for backward compatibility)
#ifdef DEBUG
void mcp_debug_print(const char *format, ...);
#else
#define mcp_debug_print(...)
#endif

// Utility functions
const char *mcp_log_level_to_string(mcp_log_level_t level);
const char *mcp_log_level_to_color(mcp_log_level_t level);

// Default configuration
mcp_log_config_t *mcp_log_config_create_default(void);
void mcp_log_config_destroy(mcp_log_config_t *config);

#endif // MCP_LOGGING_H
