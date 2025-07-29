#ifndef MCP_STDIO_TRANSPORT_H
#define MCP_STDIO_TRANSPORT_H

#include "transport_interface.h"
#include <stdio.h>
#include <pthread.h>

// STDIO transport specific structures
typedef struct {
    FILE *input_stream;
    FILE *output_stream;
    FILE *error_stream;
    
    // Threading
    pthread_t reader_thread;
    pthread_mutex_t output_mutex;
    bool thread_running;
    
    // Buffering
    char *input_buffer;
    size_t input_buffer_size;
    size_t input_buffer_capacity;
    
    // Line-based processing
    bool line_buffered;
    char line_delimiter;
} mcp_stdio_transport_data_t;

// STDIO connection data
typedef struct {
    bool is_connected;
    time_t connected_time;
} mcp_stdio_connection_data_t;

// STDIO transport interface implementation
extern const mcp_transport_interface_t mcp_stdio_transport_interface;

// STDIO-specific functions
int mcp_stdio_transport_init_impl(mcp_transport_t *transport, const mcp_transport_config_t *config);
int mcp_stdio_transport_start_impl(mcp_transport_t *transport);
int mcp_stdio_transport_stop_impl(mcp_transport_t *transport);
int mcp_stdio_transport_send_impl(mcp_connection_t *connection, const char *message, size_t length);
int mcp_stdio_transport_close_connection_impl(mcp_connection_t *connection);
int mcp_stdio_transport_get_stats_impl(mcp_transport_t *transport, void *stats);
void mcp_stdio_transport_cleanup_impl(mcp_transport_t *transport);

// STDIO utility functions
int mcp_stdio_transport_setup_streams(mcp_stdio_transport_data_t *data, 
                                     FILE *input, FILE *output, FILE *error);
int mcp_stdio_transport_setup_buffering(mcp_stdio_transport_data_t *data, 
                                       size_t buffer_size, bool line_buffered);
void *mcp_stdio_transport_reader_thread(void *arg);

// STDIO connection management
mcp_connection_t *mcp_stdio_connection_create(mcp_transport_t *transport);
void mcp_stdio_connection_destroy(mcp_connection_t *connection);

// STDIO message processing
int mcp_stdio_process_input_line(mcp_transport_t *transport, const char *line);
int mcp_stdio_send_output_line(mcp_transport_t *transport, const char *line);

// STDIO error handling
void mcp_stdio_handle_error(mcp_transport_t *transport, int error_code, const char *message);

#endif // MCP_STDIO_TRANSPORT_H
