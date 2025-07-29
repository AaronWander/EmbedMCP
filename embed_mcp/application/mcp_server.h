#ifndef MCP_APPLICATION_SERVER_H
#define MCP_APPLICATION_SERVER_H

#include "protocol/mcp_protocol.h"
#include "transport/transport_interface.h"
#include "session_manager.h"
#include "client_manager.h"
#include "request_router.h"
#include <stdbool.h>
#include <time.h>
#include <pthread.h>

// Forward declarations
typedef struct mcp_server mcp_server_t;

// Server states
typedef enum {
    MCP_SERVER_STATE_STOPPED,
    MCP_SERVER_STATE_STARTING,
    MCP_SERVER_STATE_RUNNING,
    MCP_SERVER_STATE_STOPPING,
    MCP_SERVER_STATE_ERROR
} mcp_server_state_t;

// Server configuration
typedef struct {
    // Server information
    char *server_name;
    char *server_version;
    char *server_title;
    
    // Transport configuration
    mcp_transport_type_t transport_type;
    mcp_transport_config_t *transport_config;
    
    // Protocol configuration
    mcp_protocol_config_t *protocol_config;
    
    // Application settings
    bool enable_logging;
    size_t max_sessions;
    size_t max_clients;
    time_t session_timeout;
    time_t client_timeout;
    
    // Threading
    size_t worker_threads;
    bool use_thread_pool;
} mcp_server_config_t;

// Server statistics
typedef struct {
    time_t started_time;
    time_t uptime;
    size_t total_sessions;
    size_t active_sessions;
    size_t total_clients;
    size_t active_clients;
    size_t messages_processed;
    size_t requests_handled;
    size_t errors_encountered;
} mcp_server_stats_t;

// Server structure
struct mcp_server {
    // Configuration
    mcp_server_config_t *config;
    
    // State
    mcp_server_state_t state;
    pthread_mutex_t state_mutex;
    
    // Core components
    mcp_protocol_t *protocol;
    mcp_transport_t *transport;
    mcp_session_manager_t *session_manager;
    mcp_client_manager_t *client_manager;
    mcp_request_router_t *request_router;
    
    // Statistics
    mcp_server_stats_t stats;
    pthread_mutex_t stats_mutex;
    
    // Threading
    pthread_t *worker_threads;
    size_t worker_thread_count;
    bool shutdown_requested;
};

// Server lifecycle
mcp_server_t *mcp_server_create(const mcp_server_config_t *config);
void mcp_server_destroy(mcp_server_t *server);

int mcp_server_start(mcp_server_t *server);
int mcp_server_stop(mcp_server_t *server);
int mcp_server_restart(mcp_server_t *server);

// Server state management
mcp_server_state_t mcp_server_get_state(const mcp_server_t *server);
bool mcp_server_is_running(const mcp_server_t *server);
bool mcp_server_is_ready(const mcp_server_t *server);

// Server configuration
mcp_server_config_t *mcp_server_config_create_default(void);
mcp_server_config_t *mcp_server_config_create_stdio(void);
mcp_server_config_t *mcp_server_config_create_http(int port, const char *bind_address);
mcp_server_config_t *mcp_server_config_create_sse(int port, const char *bind_address);
void mcp_server_config_destroy(mcp_server_config_t *config);

int mcp_server_config_set_server_info(mcp_server_config_t *config,
                                     const char *name, const char *version, const char *title);
int mcp_server_config_set_transport(mcp_server_config_t *config,
                                   mcp_transport_type_t type, const mcp_transport_config_t *transport_config);
int mcp_server_config_set_protocol(mcp_server_config_t *config,
                                  const mcp_protocol_config_t *protocol_config);

// Server statistics
int mcp_server_get_stats(const mcp_server_t *server, mcp_server_stats_t *stats);
void mcp_server_reset_stats(mcp_server_t *server);

// Server event callbacks
typedef void (*mcp_server_state_change_callback_t)(mcp_server_t *server, 
                                                  mcp_server_state_t old_state,
                                                  mcp_server_state_t new_state,
                                                  void *user_data);
typedef void (*mcp_server_error_callback_t)(mcp_server_t *server,
                                           int error_code,
                                           const char *error_message,
                                           void *user_data);

void mcp_server_set_state_change_callback(mcp_server_t *server,
                                         mcp_server_state_change_callback_t callback,
                                         void *user_data);
void mcp_server_set_error_callback(mcp_server_t *server,
                                  mcp_server_error_callback_t callback,
                                  void *user_data);

// Server utility functions
const char *mcp_server_state_to_string(mcp_server_state_t state);
int mcp_server_send_notification(mcp_server_t *server, const char *session_id,
                                const char *method, cJSON *params);
int mcp_server_broadcast_notification(mcp_server_t *server,
                                     const char *method, cJSON *params);

// Server maintenance
int mcp_server_cleanup_expired_sessions(mcp_server_t *server);
int mcp_server_cleanup_inactive_clients(mcp_server_t *server);
int mcp_server_perform_maintenance(mcp_server_t *server);

// Server information
cJSON *mcp_server_get_info(const mcp_server_t *server);
cJSON *mcp_server_get_capabilities(const mcp_server_t *server);
cJSON *mcp_server_get_status(const mcp_server_t *server);

#endif // MCP_APPLICATION_SERVER_H
