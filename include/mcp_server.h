#ifndef MCP_SERVER_H
#define MCP_SERVER_H

// Enable POSIX functions like strdup
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>
#include "cJSON.h"

// MCP Protocol Version
#define MCP_PROTOCOL_VERSION "2025-06-18"

// Server Information
#define MCP_SERVER_NAME "EmbedMCP"
#define MCP_SERVER_VERSION "1.0.0"
#define MCP_SERVER_TITLE "Embedded C MCP Server"

// JSON-RPC Error Codes
#define JSONRPC_PARSE_ERROR     -32700
#define JSONRPC_INVALID_REQUEST -32600
#define JSONRPC_METHOD_NOT_FOUND -32601
#define JSONRPC_INVALID_PARAMS  -32602
#define JSONRPC_INTERNAL_ERROR  -32603

// HTTP Configuration
#define HTTP_PORT 8080
#define MAX_CLIENTS 10
#define BUFFER_SIZE 8192
#define SESSION_ID_LENGTH 64

// Transport Types
typedef enum {
    MCP_TRANSPORT_STDIO,
    MCP_TRANSPORT_HTTP,
    MCP_TRANSPORT_SSE
} mcp_transport_type_t;

// MCP Message Types
typedef enum {
    MCP_MSG_REQUEST,
    MCP_MSG_RESPONSE,
    MCP_MSG_NOTIFICATION
} mcp_message_type_t;

// MCP Request Structure
typedef struct {
    char *jsonrpc;
    cJSON *id;
    char *method;
    cJSON *params;
    bool is_notification;  // true if this is a notification (no id), false if request
} mcp_request_t;

// MCP Response Structure
typedef struct {
    char *jsonrpc;
    cJSON *id;
    cJSON *result;
    cJSON *error;
} mcp_response_t;

// MCP Tool Definition
typedef struct {
    char *name;
    char *title;
    char *description;
    cJSON *input_schema;
} mcp_tool_t;

// HTTP Request Structure
typedef struct {
    char method[16];
    char path[256];
    char protocol[16];
    char *headers;
    char *body;
    size_t body_length;
    char session_id[SESSION_ID_LENGTH + 1];
    char protocol_version[32];
} http_request_t;

// HTTP Response Structure
typedef struct {
    int status_code;
    char *headers;
    char *body;
    size_t body_length;
    bool is_sse;
} http_response_t;

// MCP Session Structure (independent of socket connections)
typedef struct {
    char session_id[SESSION_ID_LENGTH + 1];
    bool initialized;
    time_t created_time;
    time_t last_activity;
    cJSON *client_capabilities;
} mcp_session_t;

// Client Connection Structure (for active socket connections)
typedef struct {
    int socket_fd;
    char session_id[SESSION_ID_LENGTH + 1];
    bool sse_connected;
    pthread_t thread_id;
    time_t last_activity;
} mcp_client_t;

// MCP Server Context
typedef struct {
    bool initialized;
    cJSON *client_capabilities;
    cJSON *server_capabilities;
    mcp_tool_t *tools;
    int tool_count;

    // Transport configuration
    mcp_transport_type_t transport_type;
    int http_port;
    int server_socket;

    // Session management
    mcp_session_t sessions[MAX_CLIENTS];
    int session_count;
    pthread_mutex_t sessions_mutex;

    // Client management (active connections)
    mcp_client_t clients[MAX_CLIENTS];
    int client_count;
    pthread_mutex_t clients_mutex;
} mcp_server_t;

// Global running flag
extern volatile bool g_running;

// Function prototypes
// Core server functions
int mcp_server_init(mcp_server_t *server);
void mcp_server_cleanup(mcp_server_t *server);
int mcp_server_run(mcp_server_t *server);
int mcp_server_run_http(mcp_server_t *server, int port);

// Message handling
int mcp_read_message(char **buffer, size_t *buffer_size);
int mcp_parse_request(const char *json_str, mcp_request_t *request);
int mcp_send_response(const mcp_response_t *response);
int mcp_send_error(cJSON *id, int code, const char *message);

// Protocol handlers
int mcp_handle_initialize(mcp_server_t *server, const mcp_request_t *request);
int mcp_handle_list_tools(mcp_server_t *server, const mcp_request_t *request);
int mcp_handle_call_tool(mcp_server_t *server, const mcp_request_t *request);

// Tool implementations
int mcp_tool_add(const cJSON *params, cJSON **result);

// HTTP transport functions
int http_server_init(mcp_server_t *server, int port);
void http_server_cleanup(mcp_server_t *server);
void *http_client_handler(void *arg);
int http_parse_request(const char *raw_request, http_request_t *request);
int http_send_response(int client_fd, const http_response_t *response);
int http_send_sse_message(int client_fd, const char *data);
char *http_generate_session_id(void);

// Session management
char *mcp_create_session(mcp_server_t *server);
mcp_session_t *mcp_find_session(mcp_server_t *server, const char *session_id);
void mcp_remove_session(mcp_server_t *server, const char *session_id);
int mcp_initialize_session(mcp_server_t *server, const char *session_id, const cJSON *client_capabilities);

// Client management
int mcp_add_client(mcp_server_t *server, int socket_fd);
void mcp_remove_client(mcp_server_t *server, int socket_fd);
mcp_client_t *mcp_find_client(mcp_server_t *server, const char *session_id);
mcp_client_t *mcp_find_client_by_socket(mcp_server_t *server, int socket_fd);

// HTTP request handlers
int handle_http_post(int client_fd, const http_request_t *request);
int handle_http_get(int client_fd, const http_request_t *request);
int handle_http_delete(int client_fd, const http_request_t *request);
int handle_http_initialize(int client_fd, const http_request_t *request, const mcp_request_t *mcp_req);
cJSON *handle_mcp_request(const mcp_request_t *request);

// Legacy HTTP+SSE transport handlers
int handle_legacy_sse(int client_fd, const http_request_t *request);
int handle_legacy_messages(int client_fd, const http_request_t *request);

// Utility functions
void mcp_request_cleanup(mcp_request_t *request);
void mcp_response_cleanup(mcp_response_t *response);
void http_request_cleanup(http_request_t *request);
void http_response_cleanup(http_response_t *response);
cJSON *mcp_create_server_capabilities(void);
cJSON *mcp_create_server_info(void);

// Debug functions
#ifdef DEBUG
void mcp_debug_print(const char *format, ...);
#else
#define mcp_debug_print(...)
#endif

#endif // MCP_SERVER_H
