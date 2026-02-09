#ifndef PLATFORM_HAL_H
#define PLATFORM_HAL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Platform type detection
#if defined(FREERTOS)
    #define MCP_PLATFORM_FREERTOS
#else
    #define MCP_PLATFORM_LINUX  // Default to Linux HAL (for Linux/macOS/POSIX systems)
#endif

// Platform capabilities description
typedef struct {
    bool has_dynamic_memory;    // Support dynamic memory allocation
    bool has_threading;         // Support multi-threading
    bool has_networking;        // Support network stack
    uint32_t max_memory_kb;     // Maximum available memory (KB)
    uint8_t max_connections;    // Maximum connections
    uint32_t tick_frequency_hz; // System clock frequency
} mcp_platform_capabilities_t;

// Memory management interface
typedef struct {
    void* (*alloc)(size_t size);
    void (*free)(void* ptr);
    void* (*realloc)(void* ptr, size_t new_size);
    size_t (*get_free_size)(void);
} mcp_platform_memory_t;

// Thread management interface
typedef struct {
    int (*create)(void** handle, void* (*func)(void*), void* arg, uint32_t stack_size);
    int (*join)(void* handle);
    void (*yield)(void);
    void (*sleep_ms)(uint32_t ms);
    uint32_t (*get_id)(void);
} mcp_platform_thread_t;

// Synchronization primitives interface
typedef struct {
    int (*mutex_create)(void** mutex);
    int (*mutex_lock)(void* mutex);
    int (*mutex_unlock)(void* mutex);
    int (*mutex_destroy)(void* mutex);
} mcp_platform_sync_t;

// Time interface
typedef struct {
    uint32_t (*get_tick_ms)(void);
    uint64_t (*get_time_us)(void);
    void (*delay_ms)(uint32_t ms);
    void (*delay_us)(uint32_t us);
} mcp_platform_time_t;

// HAL network types
typedef enum {
    MCP_HAL_NET_TCP,               // TCP network
    MCP_HAL_NET_UDP,               // UDP network
    MCP_HAL_NET_UART,              // UART serial
    MCP_HAL_NET_SPI,               // SPI bus
    MCP_HAL_NET_CAN,               // CAN bus
    MCP_HAL_NET_USB                // USB interface
} mcp_hal_network_type_t;

// 网络连接句柄
typedef void* mcp_hal_connection_handle_t;

// Network address structure
typedef struct {
    uint32_t ip;                   // IP address (network byte order)
    uint16_t port;                 // Port number
    char hostname[256];            // Hostname (optional)
} mcp_hal_network_address_t;

// Network event types
typedef enum {
    MCP_HAL_NET_EVENT_CONNECTED,    // New connection established
    MCP_HAL_NET_EVENT_DATA,         // Data arrived
    MCP_HAL_NET_EVENT_DISCONNECTED, // Connection disconnected
    MCP_HAL_NET_EVENT_ERROR         // Network error
} mcp_hal_network_event_type_t;

// Network event structure
typedef struct {
    mcp_hal_network_event_type_t type;
    mcp_hal_connection_handle_t connection;
    const void* data;
    size_t data_length;
    int error_code;                // Error code (only for ERROR events)
} mcp_hal_network_event_t;

// Network event callback function
typedef void (*mcp_hal_network_event_callback_t)(const mcp_hal_network_event_t* event, void* user_data);

// Network configuration structure
typedef struct {
    mcp_hal_network_type_t type;   // Network type
    const char* bind_address;      // Bind address
    uint16_t port;                 // Port number
    mcp_hal_network_event_callback_t callback; // Event callback
    void* user_data;               // User data
} mcp_hal_network_config_t;

// HAL network interface - unified abstraction based on mongoose
typedef void* mcp_hal_connection_t;  // Connection handle (mongoose connection)
typedef void* mcp_hal_server_t;      // Server handle (mongoose server)

// HTTP request structure (based on mongoose)
typedef struct {
    const char* method;
    const char* uri;
    // Request head (request line + headers, excluding body). Useful for header parsing.
    const char* head;
    size_t head_len;
    const char* body;
    size_t body_len;
    mcp_hal_connection_t connection;
} mcp_hal_http_request_t;

// HTTP response structure
typedef struct {
    int status_code;
    const char* headers;
    const char* body;
    size_t body_len;
} mcp_hal_http_response_t;

// HTTP event callback
typedef void (*mcp_hal_http_handler_t)(const mcp_hal_http_request_t* request,
                                      mcp_hal_http_response_t* response,
                                      void* user_data);

// HAL network interface - generic network abstraction interface
// Note: Uses generic names, underlying can be mongoose, lwIP, or other network libraries
typedef struct {
    // HTTP server interface - generic interface names
    mcp_hal_server_t (*http_server_start)(const char* url, mcp_hal_http_handler_t handler, void* user_data);
    int (*http_response_send)(mcp_hal_connection_t conn, const mcp_hal_http_response_t* response);

    // Network event polling - generic interface names
    int (*network_poll)(int timeout_ms);

    // Server management - generic interface names
    int (*http_server_stop)(mcp_hal_server_t server);

    // Low-level network interface (for platforms that don't support high-level HTTP libraries)
    int (*socket_create)(int domain, int type, int protocol);
    int (*socket_bind)(int sockfd, const char* address, uint16_t port);
    int (*socket_send)(int sockfd, const void* data, size_t len);
    int (*socket_recv)(int sockfd, void* buffer, size_t max_len);
    int (*socket_close)(int sockfd);
} mcp_platform_network_t;

// Complete platform HAL
typedef struct {
    const char* platform_name;
    const char* version;
    mcp_platform_capabilities_t capabilities;

    mcp_platform_memory_t memory;
    mcp_platform_thread_t thread;
    mcp_platform_sync_t sync;
    mcp_platform_time_t time;
    mcp_platform_network_t network;  // Use network interface instead of transport interface

    // Platform initialization and cleanup
    int (*init)(void);
    void (*cleanup)(void);
} mcp_platform_hal_t;

// Get current platform HAL implementation
const mcp_platform_hal_t* mcp_platform_get_hal(void);

// Platform capability queries
const mcp_platform_capabilities_t* mcp_platform_get_capabilities(void);
bool mcp_platform_has_capability(const char* capability);

// Platform initialization
int mcp_platform_init(void);
void mcp_platform_cleanup(void);

#endif // PLATFORM_HAL_H
