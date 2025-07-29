#include "transport/stdio_transport.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

// STDIO transport interface implementation
const mcp_transport_interface_t mcp_stdio_transport_interface = {
    .init = mcp_stdio_transport_init_impl,
    .start = mcp_stdio_transport_start_impl,
    .stop = mcp_stdio_transport_stop_impl,
    .send = mcp_stdio_transport_send_impl,
    .close_connection = mcp_stdio_transport_close_connection_impl,
    .get_stats = mcp_stdio_transport_get_stats_impl,
    .cleanup = mcp_stdio_transport_cleanup_impl
};

// STDIO-specific functions
int mcp_stdio_transport_init_impl(mcp_transport_t *transport, const mcp_transport_config_t *config) {
    if (!transport) return -1;
    
    mcp_stdio_transport_data_t *data = calloc(1, sizeof(mcp_stdio_transport_data_t));
    if (!data) return -1;
    
    // Store configuration
    if (config) {
        transport->config = calloc(1, sizeof(mcp_transport_config_t));
        if (transport->config) {
            *transport->config = *config;
        }
    }
    
    // Setup default streams
    if (mcp_stdio_transport_setup_streams(data, stdin, stdout, stderr) != 0) {
        free(data);
        return -1;
    }
    
    // Setup buffering
    size_t buffer_size = config ? config->max_message_size : 8192;
    if (mcp_stdio_transport_setup_buffering(data, buffer_size, true) != 0) {
        free(data);
        return -1;
    }
    
    // Initialize mutex
    if (pthread_mutex_init(&data->output_mutex, NULL) != 0) {
        free(data->input_buffer);
        free(data);
        return -1;
    }
    
    data->thread_running = false;
    data->line_delimiter = '\n';
    
    transport->private_data = data;
    
    return 0;
}

int mcp_stdio_transport_start_impl(mcp_transport_t *transport) {
    if (!transport || !transport->private_data) return -1;
    
    mcp_stdio_transport_data_t *data = (mcp_stdio_transport_data_t*)transport->private_data;
    
    // Create the single STDIO connection
    mcp_connection_t *connection = mcp_stdio_connection_create(transport);
    if (!connection) return -1;
    
    // Start reader thread
    data->thread_running = true;
    if (pthread_create(&data->reader_thread, NULL, mcp_stdio_transport_reader_thread, transport) != 0) {
        data->thread_running = false;
        mcp_stdio_connection_destroy(connection);
        return -1;
    }
    
    // Notify connection opened
    if (transport->on_connection_opened) {
        transport->on_connection_opened(connection, transport->user_data);
    }
    
    transport->connections_opened++;
    
    return 0;
}

int mcp_stdio_transport_stop_impl(mcp_transport_t *transport) {
    if (!transport || !transport->private_data) return -1;
    
    mcp_stdio_transport_data_t *data = (mcp_stdio_transport_data_t*)transport->private_data;
    
    // Stop reader thread
    data->thread_running = false;
    
    // Wait for thread to finish
    // Note: pthread_timedjoin_np is not available on macOS, so we use pthread_join
    if (pthread_join(data->reader_thread, NULL) != 0) {
        // If join fails, try to cancel the thread
        pthread_cancel(data->reader_thread);
    }
    
    return 0;
}

int mcp_stdio_transport_send_impl(mcp_connection_t *connection, const char *message, size_t length) {
    if (!connection || !connection->transport || !message) return -1;
    
    mcp_transport_t *transport = connection->transport;
    mcp_stdio_transport_data_t *data = (mcp_stdio_transport_data_t*)transport->private_data;
    
    if (!data) return -1;
    
    // Lock output mutex
    pthread_mutex_lock(&data->output_mutex);
    
    // Write message to output stream
    size_t written = fwrite(message, 1, length, data->output_stream);
    
    // Add newline if line buffered and message doesn't end with one
    if (data->line_buffered && length > 0 && message[length - 1] != data->line_delimiter) {
        fputc(data->line_delimiter, data->output_stream);
        written++;
    }
    
    // Flush output
    fflush(data->output_stream);
    
    pthread_mutex_unlock(&data->output_mutex);
    
    return (written == length || (data->line_buffered && written == length + 1)) ? 0 : -1;
}

int mcp_stdio_transport_close_connection_impl(mcp_connection_t *connection) {
    if (!connection) return -1;
    
    // For STDIO, we can't really close the connection, just mark it as inactive
    connection->is_active = false;
    
    // Notify connection closed
    if (connection->transport && connection->transport->on_connection_closed) {
        connection->transport->on_connection_closed(connection, connection->transport->user_data);
    }
    
    return 0;
}

int mcp_stdio_transport_get_stats_impl(mcp_transport_t *transport, void *stats) {
    // For now, just return basic transport stats
    // Could be extended to include STDIO-specific stats
    (void)transport;
    (void)stats;
    return 0;
}

void mcp_stdio_transport_cleanup_impl(mcp_transport_t *transport) {
    if (!transport || !transport->private_data) return;
    
    mcp_stdio_transport_data_t *data = (mcp_stdio_transport_data_t*)transport->private_data;
    
    // Cleanup mutex
    pthread_mutex_destroy(&data->output_mutex);
    
    // Free buffers
    free(data->input_buffer);
    
    // Free private data
    free(data);
    transport->private_data = NULL;
}

// STDIO utility functions
int mcp_stdio_transport_setup_streams(mcp_stdio_transport_data_t *data, 
                                     FILE *input, FILE *output, FILE *error) {
    if (!data) return -1;
    
    data->input_stream = input ? input : stdin;
    data->output_stream = output ? output : stdout;
    data->error_stream = error ? error : stderr;
    
    return 0;
}

int mcp_stdio_transport_setup_buffering(mcp_stdio_transport_data_t *data, 
                                       size_t buffer_size, bool line_buffered) {
    if (!data) return -1;
    
    data->input_buffer = malloc(buffer_size);
    if (!data->input_buffer) return -1;
    
    data->input_buffer_size = 0;
    data->input_buffer_capacity = buffer_size;
    data->line_buffered = line_buffered;
    
    return 0;
}

void *mcp_stdio_transport_reader_thread(void *arg) {
    mcp_transport_t *transport = (mcp_transport_t*)arg;
    mcp_stdio_transport_data_t *data = (mcp_stdio_transport_data_t*)transport->private_data;
    
    if (!data) return NULL;
    
    char line_buffer[8192];
    
    while (data->thread_running) {
        // Read line from input stream
        if (fgets(line_buffer, sizeof(line_buffer), data->input_stream) == NULL) {
            if (feof(data->input_stream)) {
                // End of input
                break;
            } else if (ferror(data->input_stream)) {
                // Error reading
                mcp_stdio_handle_error(transport, errno, "Error reading from stdin");
                break;
            }
            continue;
        }
        
        // Remove trailing newline
        size_t len = strlen(line_buffer);
        if (len > 0 && line_buffer[len - 1] == '\n') {
            line_buffer[len - 1] = '\0';
            len--;
        }
        
        // Process the line
        if (len > 0) {
            mcp_stdio_process_input_line(transport, line_buffer);
        }
    }
    
    return NULL;
}

// STDIO connection management
mcp_connection_t *mcp_stdio_connection_create(mcp_transport_t *transport) {
    if (!transport) return NULL;
    
    mcp_connection_t *connection = calloc(1, sizeof(mcp_connection_t));
    if (!connection) return NULL;
    
    mcp_stdio_connection_data_t *conn_data = calloc(1, sizeof(mcp_stdio_connection_data_t));
    if (!conn_data) {
        free(connection);
        return NULL;
    }
    
    connection->transport = transport;
    connection->connection_id = strdup("stdio-0");
    connection->session_id = NULL;
    connection->is_active = true;
    connection->created_time = time(NULL);
    connection->last_activity = time(NULL);
    connection->private_data = conn_data;
    connection->messages_sent = 0;
    connection->messages_received = 0;
    connection->bytes_sent = 0;
    connection->bytes_received = 0;
    
    conn_data->is_connected = true;
    conn_data->connected_time = time(NULL);
    
    return connection;
}

void mcp_stdio_connection_destroy(mcp_connection_t *connection) {
    if (!connection) return;
    
    free(connection->connection_id);
    free(connection->session_id);
    free(connection->private_data);
    free(connection);
}

// STDIO message processing
int mcp_stdio_process_input_line(mcp_transport_t *transport, const char *line) {
    if (!transport || !line) return -1;
    
    // Find the STDIO connection (there should be only one)
    // For now, we'll create a dummy connection for the callback
    mcp_connection_t dummy_connection = {
        .transport = transport,
        .connection_id = "stdio-0",
        .session_id = NULL,
        .is_active = true,
        .created_time = time(NULL),
        .last_activity = time(NULL),
        .private_data = NULL,
        .messages_sent = 0,
        .messages_received = 1,
        .bytes_sent = 0,
        .bytes_received = strlen(line)
    };
    
    // Call message received callback
    if (transport->on_message) {
        transport->on_message(line, strlen(line), &dummy_connection, transport->user_data);
    }
    
    transport->messages_received++;
    
    return 0;
}

int mcp_stdio_send_output_line(mcp_transport_t *transport, const char *line) {
    if (!transport || !line) return -1;
    
    mcp_stdio_transport_data_t *data = (mcp_stdio_transport_data_t*)transport->private_data;
    if (!data) return -1;
    
    pthread_mutex_lock(&data->output_mutex);
    
    fputs(line, data->output_stream);
    if (data->line_buffered) {
        fputc(data->line_delimiter, data->output_stream);
    }
    fflush(data->output_stream);
    
    pthread_mutex_unlock(&data->output_mutex);
    
    return 0;
}

// STDIO error handling
void mcp_stdio_handle_error(mcp_transport_t *transport, int error_code, const char *message) {
    if (!transport) return;
    
    if (transport->on_error) {
        transport->on_error(transport, error_code, message, transport->user_data);
    }
    
    // Also log to stderr if available
    mcp_stdio_transport_data_t *data = (mcp_stdio_transport_data_t*)transport->private_data;
    if (data && data->error_stream) {
        fprintf(data->error_stream, "STDIO Transport Error %d: %s\n", error_code, message ? message : "Unknown error");
        fflush(data->error_stream);
    }
}
