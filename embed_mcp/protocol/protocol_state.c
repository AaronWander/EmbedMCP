#include "protocol/protocol_state.h"
#include "protocol/message.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// State machine creation and destruction
mcp_protocol_state_machine_t *mcp_protocol_state_create(void) {
    mcp_protocol_state_machine_t *state_machine = calloc(1, sizeof(mcp_protocol_state_machine_t));
    if (!state_machine) return NULL;
    
    state_machine->current_state = MCP_STATE_UNINITIALIZED;
    state_machine->previous_state = MCP_STATE_UNINITIALIZED;
    state_machine->state_entered_time = time(NULL);
    state_machine->transition_count = 0;
    state_machine->last_error_code = 0;
    state_machine->last_error_message = NULL;
    state_machine->strict_mode = true;
    state_machine->max_pending_requests = 100;
    state_machine->request_timeout = 30; // 30 seconds
    
    mcp_session_info_init(&state_machine->session_info);
    
    return state_machine;
}

void mcp_protocol_state_destroy(mcp_protocol_state_machine_t *state_machine) {
    if (!state_machine) return;
    
    free(state_machine->last_error_message);
    mcp_session_info_cleanup(&state_machine->session_info);
    free(state_machine);
}

// State transition logic
static bool is_valid_transition(mcp_protocol_state_t from, mcp_protocol_event_t event, mcp_protocol_state_t to) {
    switch (from) {
        case MCP_STATE_UNINITIALIZED:
            return (event == MCP_EVENT_INITIALIZE_REQUEST && to == MCP_STATE_INITIALIZING);
            
        case MCP_STATE_INITIALIZING:
            return (event == MCP_EVENT_INITIALIZE_RESPONSE && to == MCP_STATE_INITIALIZED) ||
                   (event == MCP_EVENT_ERROR && to == MCP_STATE_ERROR);
                   
        case MCP_STATE_INITIALIZED:
            return (event == MCP_EVENT_INITIALIZED_NOTIFICATION && to == MCP_STATE_READY) ||
                   (event == MCP_EVENT_ERROR && to == MCP_STATE_ERROR);
                   
        case MCP_STATE_READY:
            return (event == MCP_EVENT_REQUEST && to == MCP_STATE_READY) ||
                   (event == MCP_EVENT_RESPONSE && to == MCP_STATE_READY) ||
                   (event == MCP_EVENT_NOTIFICATION && to == MCP_STATE_READY) ||
                   (event == MCP_EVENT_ERROR && to == MCP_STATE_ERROR) ||
                   (event == MCP_EVENT_SHUTDOWN && to == MCP_STATE_SHUTDOWN);
                   
        case MCP_STATE_ERROR:
            return (event == MCP_EVENT_INITIALIZE_REQUEST && to == MCP_STATE_INITIALIZING) ||
                   (event == MCP_EVENT_SHUTDOWN && to == MCP_STATE_SHUTDOWN);
                   
        case MCP_STATE_SHUTDOWN:
            return false; // No transitions from shutdown state
            
        default:
            return false;
    }
}

static mcp_protocol_state_t get_next_state(mcp_protocol_state_t current, mcp_protocol_event_t event) {
    switch (current) {
        case MCP_STATE_UNINITIALIZED:
            if (event == MCP_EVENT_INITIALIZE_REQUEST) return MCP_STATE_INITIALIZING;
            break;
            
        case MCP_STATE_INITIALIZING:
            if (event == MCP_EVENT_INITIALIZE_RESPONSE) return MCP_STATE_INITIALIZED;
            if (event == MCP_EVENT_ERROR) return MCP_STATE_ERROR;
            break;
            
        case MCP_STATE_INITIALIZED:
            if (event == MCP_EVENT_INITIALIZED_NOTIFICATION) return MCP_STATE_READY;
            if (event == MCP_EVENT_ERROR) return MCP_STATE_ERROR;
            break;
            
        case MCP_STATE_READY:
            if (event == MCP_EVENT_ERROR) return MCP_STATE_ERROR;
            if (event == MCP_EVENT_SHUTDOWN) return MCP_STATE_SHUTDOWN;
            return MCP_STATE_READY; // Stay in ready state for normal operations
            
        case MCP_STATE_ERROR:
            if (event == MCP_EVENT_INITIALIZE_REQUEST) return MCP_STATE_INITIALIZING;
            if (event == MCP_EVENT_SHUTDOWN) return MCP_STATE_SHUTDOWN;
            break;
            
        case MCP_STATE_SHUTDOWN:
            break; // No transitions from shutdown
    }
    
    return current; // No valid transition, stay in current state
}

// State transitions
bool mcp_protocol_state_transition(mcp_protocol_state_machine_t *state_machine, mcp_protocol_event_t event) {
    if (!state_machine) return false;
    
    mcp_protocol_state_t next_state = get_next_state(state_machine->current_state, event);
    
    if (!is_valid_transition(state_machine->current_state, event, next_state)) {
        return false;
    }
    
    // Perform the transition
    state_machine->previous_state = state_machine->current_state;
    state_machine->current_state = next_state;
    state_machine->state_entered_time = time(NULL);
    state_machine->transition_count++;
    
    return true;
}

bool mcp_protocol_state_can_transition(const mcp_protocol_state_machine_t *state_machine, mcp_protocol_event_t event) {
    if (!state_machine) return false;
    
    mcp_protocol_state_t next_state = get_next_state(state_machine->current_state, event);
    return is_valid_transition(state_machine->current_state, event, next_state);
}

// State queries
mcp_protocol_state_t mcp_protocol_state_get_current(const mcp_protocol_state_machine_t *state_machine) {
    return state_machine ? state_machine->current_state : MCP_STATE_ERROR;
}

mcp_protocol_state_t mcp_protocol_state_get_previous(const mcp_protocol_state_machine_t *state_machine) {
    return state_machine ? state_machine->previous_state : MCP_STATE_ERROR;
}

bool mcp_protocol_state_is_ready(const mcp_protocol_state_machine_t *state_machine) {
    return state_machine && state_machine->current_state == MCP_STATE_READY;
}

bool mcp_protocol_state_is_initialized(const mcp_protocol_state_machine_t *state_machine) {
    return state_machine && (state_machine->current_state == MCP_STATE_INITIALIZED || 
                            state_machine->current_state == MCP_STATE_READY);
}

bool mcp_protocol_state_can_handle_requests(const mcp_protocol_state_machine_t *state_machine) {
    return mcp_protocol_state_is_ready(state_machine);
}

// Session management
int mcp_protocol_state_initialize_session(mcp_protocol_state_machine_t *state_machine,
                                         const char *protocol_version,
                                         const cJSON *client_capabilities,
                                         const cJSON *client_info) {
    if (!state_machine || state_machine->current_state != MCP_STATE_INITIALIZING) {
        return -1;
    }
    
    // Set protocol version
    free(state_machine->session_info.protocol_version);
    state_machine->session_info.protocol_version = protocol_version ? strdup(protocol_version) : NULL;
    
    // Parse client info
    if (client_info && cJSON_IsObject(client_info)) {
        cJSON *name = cJSON_GetObjectItem(client_info, "name");
        cJSON *version = cJSON_GetObjectItem(client_info, "version");
        
        if (name && cJSON_IsString(name)) {
            free(state_machine->session_info.client_info.name);
            state_machine->session_info.client_info.name = strdup(name->valuestring);
        }
        
        if (version && cJSON_IsString(version)) {
            free(state_machine->session_info.client_info.version);
            state_machine->session_info.client_info.version = strdup(version->valuestring);
        }
    }
    
    // Parse capabilities
    if (client_capabilities && cJSON_IsObject(client_capabilities)) {
        // Parse client capabilities
        cJSON *roots = cJSON_GetObjectItem(client_capabilities, "roots");
        if (roots && cJSON_IsObject(roots)) {
            cJSON *list_changed = cJSON_GetObjectItem(roots, "listChanged");
            state_machine->session_info.capabilities.client.roots = 
                list_changed && cJSON_IsBool(list_changed) && cJSON_IsTrue(list_changed);
        }
        
        cJSON *sampling = cJSON_GetObjectItem(client_capabilities, "sampling");
        if (sampling && cJSON_IsObject(sampling)) {
            state_machine->session_info.capabilities.client.sampling = true;
        }
    }
    
    state_machine->session_info.initialized_time = time(NULL);
    state_machine->session_info.last_activity = time(NULL);
    
    return 0;
}

int mcp_protocol_state_finalize_initialization(mcp_protocol_state_machine_t *state_machine) {
    if (!state_machine || state_machine->current_state != MCP_STATE_INITIALIZED) {
        return -1;
    }
    
    return mcp_protocol_state_transition(state_machine, MCP_EVENT_INITIALIZED_NOTIFICATION) ? 0 : -1;
}

void mcp_protocol_state_reset_session(mcp_protocol_state_machine_t *state_machine) {
    if (!state_machine) return;
    
    state_machine->current_state = MCP_STATE_UNINITIALIZED;
    state_machine->previous_state = MCP_STATE_UNINITIALIZED;
    state_machine->state_entered_time = time(NULL);
    state_machine->transition_count = 0;
    
    mcp_session_info_cleanup(&state_machine->session_info);
    mcp_session_info_init(&state_machine->session_info);
    
    mcp_protocol_state_clear_error(state_machine);
}

// Session info management
void mcp_session_info_init(mcp_session_info_t *info) {
    if (!info) return;
    
    memset(info, 0, sizeof(mcp_session_info_t));
    info->initialized_time = 0;
    info->last_activity = 0;
}

void mcp_session_info_cleanup(mcp_session_info_t *info) {
    if (!info) return;
    
    free(info->protocol_version);
    free(info->client_info.name);
    free(info->client_info.version);
    free(info->server_info.name);
    free(info->server_info.version);
    
    memset(info, 0, sizeof(mcp_session_info_t));
}

cJSON *mcp_session_info_to_json(const mcp_session_info_t *info) {
    if (!info) return NULL;
    
    cJSON *json = cJSON_CreateObject();
    if (!json) return NULL;
    
    if (info->protocol_version) {
        cJSON_AddStringToObject(json, "protocolVersion", info->protocol_version);
    }
    
    cJSON *client_info = cJSON_CreateObject();
    if (info->client_info.name) {
        cJSON_AddStringToObject(client_info, "name", info->client_info.name);
    }
    if (info->client_info.version) {
        cJSON_AddStringToObject(client_info, "version", info->client_info.version);
    }
    cJSON_AddItemToObject(json, "clientInfo", client_info);
    
    cJSON *server_info = cJSON_CreateObject();
    if (info->server_info.name) {
        cJSON_AddStringToObject(server_info, "name", info->server_info.name);
    }
    if (info->server_info.version) {
        cJSON_AddStringToObject(server_info, "version", info->server_info.version);
    }
    cJSON_AddItemToObject(json, "serverInfo", server_info);
    
    cJSON_AddNumberToObject(json, "initializedTime", (double)info->initialized_time);
    cJSON_AddNumberToObject(json, "lastActivity", (double)info->last_activity);
    
    return json;
}

// Capabilities management
mcp_capabilities_t *mcp_capabilities_create_default(void) {
    mcp_capabilities_t *capabilities = calloc(1, sizeof(mcp_capabilities_t));
    if (!capabilities) return NULL;

    // Default server capabilities - start with nothing, enable as features are registered
    capabilities->server.tools = false;        // Will be set to true when tools are registered
    capabilities->server.resources = false;    // Will be set to true when resources are registered
    capabilities->server.prompts = false;      // Will be set to true when prompts are registered
    capabilities->server.logging = true;       // Always enabled for debugging

    // Default client capabilities
    capabilities->client.roots = false;
    capabilities->client.sampling = false;

    return capabilities;
}

void mcp_capabilities_destroy(mcp_capabilities_t *capabilities) {
    if (!capabilities) return;
    free(capabilities);
}

bool mcp_capabilities_merge(mcp_capabilities_t *target, const mcp_capabilities_t *source) {
    if (!target || !source) return false;

    // Merge server capabilities (logical OR)
    target->server.tools = target->server.tools || source->server.tools;
    target->server.resources = target->server.resources || source->server.resources;
    target->server.prompts = target->server.prompts || source->server.prompts;
    target->server.logging = target->server.logging || source->server.logging;

    // Merge client capabilities (logical OR)
    target->client.roots = target->client.roots || source->client.roots;
    target->client.sampling = target->client.sampling || source->client.sampling;

    return true;
}

cJSON *mcp_capabilities_to_json(const mcp_capabilities_t *capabilities) {
    if (!capabilities) return NULL;

    cJSON *json = cJSON_CreateObject();
    if (!json) return NULL;

    // experimental is optional according to official SDK

    // Add prompts capability if enabled
    if (capabilities->server.prompts) {
        cJSON *prompts = cJSON_CreateObject();
        cJSON_AddBoolToObject(prompts, "listChanged", true);
        cJSON_AddItemToObject(json, "prompts", prompts);
    }

    // Add resources capability if enabled
    if (capabilities->server.resources) {
        cJSON *resources = cJSON_CreateObject();
        cJSON_AddBoolToObject(resources, "subscribe", false);
        cJSON_AddBoolToObject(resources, "listChanged", true);
        cJSON_AddItemToObject(json, "resources", resources);
    }

    // Add tools capability if enabled
    if (capabilities->server.tools) {
        cJSON *tools = cJSON_CreateObject();
        cJSON_AddBoolToObject(tools, "listChanged", true);
        cJSON_AddItemToObject(json, "tools", tools);
    }

    // Add logging capability if enabled
    if (capabilities->server.logging) {
        cJSON_AddItemToObject(json, "logging", cJSON_CreateObject());
    }

    return json;
}

mcp_capabilities_t *mcp_capabilities_from_json(const cJSON *json) {
    if (!json || !cJSON_IsObject(json)) return NULL;

    mcp_capabilities_t *capabilities = calloc(1, sizeof(mcp_capabilities_t));
    if (!capabilities) return NULL;

    // Parse server capabilities
    cJSON *server = cJSON_GetObjectItem(json, "server");
    if (server && cJSON_IsObject(server)) {
        capabilities->server.tools = cJSON_GetObjectItem(server, "tools") != NULL;
        capabilities->server.resources = cJSON_GetObjectItem(server, "resources") != NULL;
        capabilities->server.prompts = cJSON_GetObjectItem(server, "prompts") != NULL;
        capabilities->server.logging = cJSON_GetObjectItem(server, "logging") != NULL;
    }

    // Parse client capabilities
    cJSON *client = cJSON_GetObjectItem(json, "client");
    if (client && cJSON_IsObject(client)) {
        capabilities->client.roots = cJSON_GetObjectItem(client, "roots") != NULL;
        capabilities->client.sampling = cJSON_GetObjectItem(client, "sampling") != NULL;
    }

    return capabilities;
}

// Error handling
void mcp_protocol_state_set_error(mcp_protocol_state_machine_t *state_machine,
                                 int error_code, const char *error_message) {
    if (!state_machine) return;

    state_machine->last_error_code = error_code;

    free(state_machine->last_error_message);
    state_machine->last_error_message = error_message ? strdup(error_message) : NULL;

    mcp_protocol_state_transition(state_machine, MCP_EVENT_ERROR);
}

void mcp_protocol_state_clear_error(mcp_protocol_state_machine_t *state_machine) {
    if (!state_machine) return;

    state_machine->last_error_code = 0;
    free(state_machine->last_error_message);
    state_machine->last_error_message = NULL;
}

bool mcp_protocol_state_has_error(const mcp_protocol_state_machine_t *state_machine) {
    return state_machine && state_machine->last_error_code != 0;
}

// Utility functions
const char *mcp_protocol_state_to_string(mcp_protocol_state_t state) {
    switch (state) {
        case MCP_STATE_UNINITIALIZED: return "UNINITIALIZED";
        case MCP_STATE_INITIALIZING: return "INITIALIZING";
        case MCP_STATE_INITIALIZED: return "INITIALIZED";
        case MCP_STATE_READY: return "READY";
        case MCP_STATE_ERROR: return "ERROR";
        case MCP_STATE_SHUTDOWN: return "SHUTDOWN";
        default: return "UNKNOWN";
    }
}

const char *mcp_protocol_event_to_string(mcp_protocol_event_t event) {
    switch (event) {
        case MCP_EVENT_INITIALIZE_REQUEST: return "INITIALIZE_REQUEST";
        case MCP_EVENT_INITIALIZE_RESPONSE: return "INITIALIZE_RESPONSE";
        case MCP_EVENT_INITIALIZED_NOTIFICATION: return "INITIALIZED_NOTIFICATION";
        case MCP_EVENT_REQUEST: return "REQUEST";
        case MCP_EVENT_RESPONSE: return "RESPONSE";
        case MCP_EVENT_NOTIFICATION: return "NOTIFICATION";
        case MCP_EVENT_ERROR: return "ERROR";
        case MCP_EVENT_SHUTDOWN: return "SHUTDOWN";
        default: return "UNKNOWN";
    }
}

bool mcp_protocol_version_is_supported(const char *version) {
    if (!version) return false;

    // Currently only support the current protocol version
    return strcmp(version, MCP_PROTOCOL_VERSION) == 0;
}
