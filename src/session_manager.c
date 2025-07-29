#include "mcp_server.h"

// Create a new session and return session ID
char *mcp_create_session(mcp_server_t *server) {
    if (!server) return NULL;
    
    pthread_mutex_lock(&server->sessions_mutex);
    
    if (server->session_count >= MAX_CLIENTS) {
        pthread_mutex_unlock(&server->sessions_mutex);
        return NULL;
    }
    
    // Generate session ID
    char *session_id = http_generate_session_id();
    if (!session_id) {
        pthread_mutex_unlock(&server->sessions_mutex);
        return NULL;
    }
    
    // Initialize session
    mcp_session_t *session = &server->sessions[server->session_count];
    strncpy(session->session_id, session_id, SESSION_ID_LENGTH);
    session->session_id[SESSION_ID_LENGTH] = '\0';
    session->initialized = false;
    session->created_time = time(NULL);
    session->last_activity = time(NULL);
    session->client_capabilities = NULL;
    
    server->session_count++;
    
    pthread_mutex_unlock(&server->sessions_mutex);
    
    mcp_debug_print("Created session: %s\n", session_id);
    return session_id; // Caller must free this
}

// Find session by ID
mcp_session_t *mcp_find_session(mcp_server_t *server, const char *session_id) {
    if (!server || !session_id || strlen(session_id) == 0) {
        mcp_debug_print("mcp_find_session: Invalid parameters\n");
        return NULL;
    }

    pthread_mutex_lock(&server->sessions_mutex);

    for (int i = 0; i < server->session_count; i++) {
        if (server->sessions[i].session_id[0] != '\0' &&
            strcmp(server->sessions[i].session_id, session_id) == 0) {
            // Update last activity
            server->sessions[i].last_activity = time(NULL);
            pthread_mutex_unlock(&server->sessions_mutex);
            return &server->sessions[i];
        }
    }
    
    pthread_mutex_unlock(&server->sessions_mutex);
    return NULL;
}

// Remove session
void mcp_remove_session(mcp_server_t *server, const char *session_id) {
    if (!server || !session_id) return;
    
    pthread_mutex_lock(&server->sessions_mutex);
    
    for (int i = 0; i < server->session_count; i++) {
        if (strcmp(server->sessions[i].session_id, session_id) == 0) {
            mcp_debug_print("Removing session: %s\n", session_id);
            
            // Cleanup session data
            if (server->sessions[i].client_capabilities) {
                cJSON_Delete(server->sessions[i].client_capabilities);
            }
            
            // Shift remaining sessions
            for (int j = i; j < server->session_count - 1; j++) {
                server->sessions[j] = server->sessions[j + 1];
            }
            server->session_count--;
            break;
        }
    }
    
    pthread_mutex_unlock(&server->sessions_mutex);
}

// Initialize session with client capabilities (but don't mark as fully initialized)
int mcp_initialize_session(mcp_server_t *server, const char *session_id, const cJSON *client_capabilities) {
    if (!server || !session_id) return -1;

    mcp_session_t *session = mcp_find_session(server, session_id);
    if (!session) return -1;

    pthread_mutex_lock(&server->sessions_mutex);

    // Store client capabilities
    if (client_capabilities) {
        session->client_capabilities = cJSON_Duplicate(client_capabilities, 1);
    }

    // Don't mark as initialized yet - wait for initialized notification
    session->initialized = false;
    session->last_activity = time(NULL);

    pthread_mutex_unlock(&server->sessions_mutex);

    mcp_debug_print("Session prepared (not yet initialized): %s\n", session_id);
    return 0;
}
