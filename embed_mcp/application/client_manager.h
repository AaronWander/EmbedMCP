#ifndef MCP_CLIENT_MANAGER_H
#define MCP_CLIENT_MANAGER_H

#include "transport/transport_interface.h"
#include <stdbool.h>
#include <time.h>
#include <pthread.h>
#include "cjson/cJSON.h"

// Forward declarations
typedef struct mcp_client_manager mcp_client_manager_t;
typedef struct mcp_client mcp_client_t;

// Client states
typedef enum {
    MCP_CLIENT_STATE_CONNECTED,
    MCP_CLIENT_STATE_AUTHENTICATED,
    MCP_CLIENT_STATE_ACTIVE,
    MCP_CLIENT_STATE_INACTIVE,
    MCP_CLIENT_STATE_DISCONNECTED
} mcp_client_state_t;

// Client structure
struct mcp_client {
    char *client_id;
    mcp_client_state_t state;
    
    // Connection information
    mcp_connection_t *connection;
    char *session_id;
    
    // Client information
    time_t connected_time;
    time_t last_activity;
    time_t last_heartbeat;
    
    // Transport information
    mcp_transport_type_t transport_type;
    char *remote_address;
    int remote_port;
    
    // Statistics
    size_t messages_sent;
    size_t messages_received;
    size_t bytes_sent;
    size_t bytes_received;
    size_t requests_handled;
    size_t notifications_sent;
    
    // User data
    void *user_data;
    
    // Internal
    pthread_mutex_t mutex;
    int ref_count;
};

// Client manager configuration
typedef struct {
    size_t max_clients;
    time_t client_timeout;
    time_t heartbeat_interval;
    time_t cleanup_interval;
    bool auto_cleanup;
    bool require_heartbeat;
} mcp_client_manager_config_t;

// Client manager structure
struct mcp_client_manager {
    // Configuration
    mcp_client_manager_config_t config;
    
    // Client storage
    mcp_client_t **clients;
    size_t client_count;
    size_t client_capacity;
    
    // Thread safety
    pthread_rwlock_t clients_lock;
    pthread_mutex_t manager_mutex;
    
    // Cleanup thread
    pthread_t cleanup_thread;
    bool cleanup_running;
    
    // Statistics
    size_t total_clients_connected;
    size_t clients_disconnected;
    size_t clients_timed_out;
};

// Client manager lifecycle
mcp_client_manager_t *mcp_client_manager_create(const mcp_client_manager_config_t *config);
void mcp_client_manager_destroy(mcp_client_manager_t *manager);

int mcp_client_manager_start(mcp_client_manager_t *manager);
int mcp_client_manager_stop(mcp_client_manager_t *manager);

// Client management
mcp_client_t *mcp_client_manager_add_client(mcp_client_manager_t *manager,
                                           mcp_connection_t *connection);
mcp_client_t *mcp_client_manager_find_client(mcp_client_manager_t *manager,
                                            const char *client_id);
mcp_client_t *mcp_client_manager_find_client_by_connection(mcp_client_manager_t *manager,
                                                          mcp_connection_t *connection);
mcp_client_t *mcp_client_manager_find_client_by_session(mcp_client_manager_t *manager,
                                                       const char *session_id);
int mcp_client_manager_remove_client(mcp_client_manager_t *manager,
                                    const char *client_id);

// Client lifecycle
int mcp_client_authenticate(mcp_client_t *client, const char *session_id);
int mcp_client_activate(mcp_client_t *client);
int mcp_client_deactivate(mcp_client_t *client);
int mcp_client_disconnect(mcp_client_t *client);

// Client state management
mcp_client_state_t mcp_client_get_state(const mcp_client_t *client);
bool mcp_client_is_active(const mcp_client_t *client);
bool mcp_client_is_connected(const mcp_client_t *client);
int mcp_client_update_activity(mcp_client_t *client);
int mcp_client_update_heartbeat(mcp_client_t *client);

// Client information
const char *mcp_client_get_id(const mcp_client_t *client);
const char *mcp_client_get_session_id(const mcp_client_t *client);
mcp_connection_t *mcp_client_get_connection(const mcp_client_t *client);
mcp_transport_type_t mcp_client_get_transport_type(const mcp_client_t *client);
const char *mcp_client_get_remote_address(const mcp_client_t *client);
time_t mcp_client_get_connected_time(const mcp_client_t *client);
time_t mcp_client_get_last_activity(const mcp_client_t *client);

// Client communication
int mcp_client_send_message(mcp_client_t *client, const char *message, size_t length);
int mcp_client_send_json(mcp_client_t *client, const cJSON *json);
int mcp_client_send_response(mcp_client_t *client, cJSON *id, cJSON *result);
int mcp_client_send_error(mcp_client_t *client, cJSON *id, int code, 
                         const char *message, cJSON *data);
int mcp_client_send_notification(mcp_client_t *client, const char *method, cJSON *params);

// Client reference counting
mcp_client_t *mcp_client_ref(mcp_client_t *client);
void mcp_client_unref(mcp_client_t *client);

// Client utilities
char *mcp_client_generate_id(void);
bool mcp_client_validate_id(const char *client_id);
cJSON *mcp_client_to_json(const mcp_client_t *client);

// Client manager utilities
size_t mcp_client_manager_get_client_count(const mcp_client_manager_t *manager);
size_t mcp_client_manager_get_active_client_count(const mcp_client_manager_t *manager);
int mcp_client_manager_cleanup_inactive_clients(mcp_client_manager_t *manager);
int mcp_client_manager_broadcast_message(mcp_client_manager_t *manager,
                                        const char *message, size_t length);
int mcp_client_manager_broadcast_notification(mcp_client_manager_t *manager,
                                             const char *method, cJSON *params);
cJSON *mcp_client_manager_get_stats(const mcp_client_manager_t *manager);

// Client manager configuration
mcp_client_manager_config_t *mcp_client_manager_config_create_default(void);
void mcp_client_manager_config_destroy(mcp_client_manager_config_t *config);

// Client callbacks
typedef void (*mcp_client_state_change_callback_t)(mcp_client_t *client,
                                                  mcp_client_state_t old_state,
                                                  mcp_client_state_t new_state,
                                                  void *user_data);
typedef void (*mcp_client_disconnected_callback_t)(mcp_client_t *client, void *user_data);
typedef void (*mcp_client_timeout_callback_t)(mcp_client_t *client, void *user_data);

void mcp_client_set_state_change_callback(mcp_client_t *client,
                                         mcp_client_state_change_callback_t callback,
                                         void *user_data);
void mcp_client_manager_set_disconnected_callback(mcp_client_manager_t *manager,
                                                 mcp_client_disconnected_callback_t callback,
                                                 void *user_data);
void mcp_client_manager_set_timeout_callback(mcp_client_manager_t *manager,
                                            mcp_client_timeout_callback_t callback,
                                            void *user_data);

// Utility functions
const char *mcp_client_state_to_string(mcp_client_state_t state);

#endif // MCP_CLIENT_MANAGER_H
