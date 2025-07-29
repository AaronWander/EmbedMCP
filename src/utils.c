#include "mcp_server.h"

// Cleanup request structure
void mcp_request_cleanup(mcp_request_t *request) {
    if (!request) return;
    
    if (request->jsonrpc) {
        free(request->jsonrpc);
        request->jsonrpc = NULL;
    }
    
    if (request->id) {
        cJSON_Delete(request->id);
        request->id = NULL;
    }
    
    if (request->method) {
        free(request->method);
        request->method = NULL;
    }
    
    if (request->params) {
        cJSON_Delete(request->params);
        request->params = NULL;
    }
}

// Cleanup response structure
void mcp_response_cleanup(mcp_response_t *response) {
    if (!response) return;
    
    // Note: jsonrpc is usually a string literal, don't free it
    
    if (response->id) {
        cJSON_Delete(response->id);
        response->id = NULL;
    }
    
    if (response->result) {
        cJSON_Delete(response->result);
        response->result = NULL;
    }
    
    if (response->error) {
        cJSON_Delete(response->error);
        response->error = NULL;
    }
}

// Create server capabilities object
cJSON *mcp_create_server_capabilities(void) {
    cJSON *capabilities = cJSON_CreateObject();
    if (!capabilities) return NULL;

    // Add capabilities in the EXACT same order as working server:
    // 1. experimental (first)
    cJSON *experimental = cJSON_CreateObject();
    cJSON_AddItemToObject(capabilities, "experimental", experimental);

    // 2. prompts (second)
    cJSON *prompts = cJSON_CreateObject();
    cJSON_AddBoolToObject(prompts, "listChanged", true);
    cJSON_AddItemToObject(capabilities, "prompts", prompts);

    // 3. resources (third)
    cJSON *resources = cJSON_CreateObject();
    cJSON_AddBoolToObject(resources, "subscribe", false);
    cJSON_AddBoolToObject(resources, "listChanged", true);
    cJSON_AddItemToObject(capabilities, "resources", resources);

    // 4. tools (last)
    cJSON *tools = cJSON_CreateObject();
    cJSON_AddBoolToObject(tools, "listChanged", true);
    cJSON_AddItemToObject(capabilities, "tools", tools);

    return capabilities;
}

// Create server info object
cJSON *mcp_create_server_info(void) {
    cJSON *server_info = cJSON_CreateObject();
    if (!server_info) return NULL;

    // Only include required fields to match working servers
    cJSON_AddStringToObject(server_info, "name", MCP_SERVER_NAME);
    cJSON_AddStringToObject(server_info, "version", MCP_SERVER_VERSION);

    return server_info;
}
