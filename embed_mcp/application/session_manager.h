#ifndef MCP_SESSION_MANAGER_H
#define MCP_SESSION_MANAGER_H

#include "protocol/protocol_state.h"
#include <stdbool.h>
#include <time.h>
#include <pthread.h>
#include "cjson/cJSON.h"

// Forward declarations
typedef struct mcp_session_manager mcp_session_manager_t;
typedef struct mcp_session mcp_session_t;

// Session states
typedef enum {
    MCP_SESSION_STATE_CREATED,
    MCP_SESSION_STATE_INITIALIZING,
    MCP_SESSION_STATE_ACTIVE,
    MCP_SESSION_STATE_INACTIVE,
    MCP_SESSION_STATE_EXPIRED,
    MCP_SESSION_STATE_TERMINATED
} mcp_session_state_t;

// Session structure
struct mcp_session {
    char *session_id;
    mcp_session_state_t state;
    
    // Protocol state
    mcp_protocol_state_machine_t *protocol_state;
    
    // Session information
    time_t created_time;
    time_t last_activity;
    time_t expires_at;
    
    // Client information
    char *client_name;
    char *client_version;
    char *protocol_version;
    
    // Capabilities
    mcp_capabilities_t *negotiated_capabilities;
    
    // Statistics
    size_t requests_handled;
    size_t notifications_sent;
    size_t errors_encountered;
    
    // User data
    void *user_data;
    
    // Internal
    pthread_mutex_t mutex;
    int ref_count;
};

// Session manager configuration
typedef struct {
    size_t max_sessions;
    time_t default_session_timeout;
    time_t cleanup_interval;
    bool auto_cleanup;
    bool strict_session_validation;
} mcp_session_manager_config_t;

// Session manager structure
struct mcp_session_manager {
    // Configuration
    mcp_session_manager_config_t config;
    
    // Session storage
    mcp_session_t **sessions;
    size_t session_count;
    size_t session_capacity;
    
    // Thread safety
    pthread_rwlock_t sessions_lock;
    pthread_mutex_t manager_mutex;
    
    // Cleanup thread
    pthread_t cleanup_thread;
    bool cleanup_running;
    
    // Statistics
    size_t total_sessions_created;
    size_t sessions_expired;
    size_t sessions_terminated;
};

// Session manager lifecycle
mcp_session_manager_t *mcp_session_manager_create(const mcp_session_manager_config_t *config);
void mcp_session_manager_destroy(mcp_session_manager_t *manager);

int mcp_session_manager_start(mcp_session_manager_t *manager);
int mcp_session_manager_stop(mcp_session_manager_t *manager);

// Session management
mcp_session_t *mcp_session_manager_create_session(mcp_session_manager_t *manager,
                                                 const char *session_id);
mcp_session_t *mcp_session_manager_find_session(mcp_session_manager_t *manager,
                                               const char *session_id);
int mcp_session_manager_remove_session(mcp_session_manager_t *manager,
                                      const char *session_id);

// Session lifecycle
int mcp_session_initialize(mcp_session_t *session,
                          const char *protocol_version,
                          const cJSON *client_capabilities,
                          const cJSON *client_info);
int mcp_session_activate(mcp_session_t *session);
int mcp_session_deactivate(mcp_session_t *session);
int mcp_session_terminate(mcp_session_t *session);

// Session state management
mcp_session_state_t mcp_session_get_state(const mcp_session_t *session);
bool mcp_session_is_active(const mcp_session_t *session);
bool mcp_session_is_expired(const mcp_session_t *session);
int mcp_session_update_activity(mcp_session_t *session);
int mcp_session_extend_expiry(mcp_session_t *session, time_t additional_time);

// Session information
const char *mcp_session_get_id(const mcp_session_t *session);
const char *mcp_session_get_client_name(const mcp_session_t *session);
const char *mcp_session_get_protocol_version(const mcp_session_t *session);
const mcp_capabilities_t *mcp_session_get_capabilities(const mcp_session_t *session);
time_t mcp_session_get_created_time(const mcp_session_t *session);
time_t mcp_session_get_last_activity(const mcp_session_t *session);

// Session reference counting
mcp_session_t *mcp_session_ref(mcp_session_t *session);
void mcp_session_unref(mcp_session_t *session);

// Session utilities
char *mcp_session_generate_id(void);
bool mcp_session_validate_id(const char *session_id);
cJSON *mcp_session_to_json(const mcp_session_t *session);

// Session manager utilities
size_t mcp_session_manager_get_session_count(const mcp_session_manager_t *manager);
size_t mcp_session_manager_get_active_session_count(const mcp_session_manager_t *manager);
int mcp_session_manager_cleanup_expired_sessions(mcp_session_manager_t *manager);
cJSON *mcp_session_manager_get_stats(const mcp_session_manager_t *manager);

// Session manager configuration
mcp_session_manager_config_t *mcp_session_manager_config_create_default(void);
void mcp_session_manager_config_destroy(mcp_session_manager_config_t *config);

// Session callbacks
typedef void (*mcp_session_state_change_callback_t)(mcp_session_t *session,
                                                   mcp_session_state_t old_state,
                                                   mcp_session_state_t new_state,
                                                   void *user_data);
typedef void (*mcp_session_expired_callback_t)(mcp_session_t *session, void *user_data);

void mcp_session_set_state_change_callback(mcp_session_t *session,
                                          mcp_session_state_change_callback_t callback,
                                          void *user_data);
void mcp_session_manager_set_expired_callback(mcp_session_manager_t *manager,
                                             mcp_session_expired_callback_t callback,
                                             void *user_data);

// Utility functions
const char *mcp_session_state_to_string(mcp_session_state_t state);

#endif // MCP_SESSION_MANAGER_H
