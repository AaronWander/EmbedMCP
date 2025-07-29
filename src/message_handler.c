#include "mcp_server.h"
#include <unistd.h>

// Read a JSON-RPC message from stdin
int mcp_read_message(char **buffer, size_t *buffer_size) {
    if (!buffer || !buffer_size) return -1;
    
    static const size_t INITIAL_SIZE = 1024;
    static const size_t MAX_SIZE = 1024 * 1024; // 1MB limit
    
    if (*buffer == NULL) {
        *buffer = malloc(INITIAL_SIZE);
        if (!*buffer) return -1;
        *buffer_size = INITIAL_SIZE;
    }
    
    size_t pos = 0;
    int brace_count = 0;
    bool in_string = false;
    bool escape_next = false;
    bool message_started = false;
    
    while (1) {
        // Ensure buffer has space
        if (pos >= *buffer_size - 1) {
            if (*buffer_size >= MAX_SIZE) {
                mcp_debug_print("Message too large\n");
                return -1;
            }
            
            size_t new_size = *buffer_size * 2;
            if (new_size > MAX_SIZE) new_size = MAX_SIZE;
            
            char *new_buffer = realloc(*buffer, new_size);
            if (!new_buffer) return -1;
            
            *buffer = new_buffer;
            *buffer_size = new_size;
        }
        
        // Read one character
        int ch = getchar();
        if (ch == EOF) {
            if (pos == 0) return 0; // Clean EOF
            return -1; // Incomplete message
        }
        
        (*buffer)[pos++] = (char)ch;
        
        // Skip whitespace before message starts
        if (!message_started && (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r')) {
            pos--; // Don't include leading whitespace
            continue;
        }
        
        if (ch == '{' && !message_started) {
            message_started = true;
        }
        
        if (!message_started) {
            pos--; // Skip non-JSON characters
            continue;
        }
        
        // Track JSON structure
        if (!in_string) {
            if (ch == '"' && !escape_next) {
                in_string = true;
            } else if (ch == '{') {
                brace_count++;
            } else if (ch == '}') {
                brace_count--;
                if (brace_count == 0) {
                    // Complete JSON object
                    (*buffer)[pos] = '\0';
                    return pos;
                }
            }
        } else {
            if (ch == '"' && !escape_next) {
                in_string = false;
            }
        }
        
        escape_next = (ch == '\\' && !escape_next);
    }
}

// Parse JSON-RPC request
int mcp_parse_request(const char *json_str, mcp_request_t *request) {
    if (!json_str || !request) {
        mcp_debug_print("mcp_parse_request: NULL parameters\n");
        return -1;
    }

    // mcp_debug_print("Parsing JSON: %s\n", json_str);
    memset(request, 0, sizeof(mcp_request_t));

    cJSON *json = cJSON_Parse(json_str);
    if (!json) {
        mcp_debug_print("Failed to parse JSON: %s\n", cJSON_GetErrorPtr() ? cJSON_GetErrorPtr() : "unknown error");
        return -1;
    }

    // Extract jsonrpc version
    cJSON *jsonrpc = cJSON_GetObjectItem(json, "jsonrpc");
    if (jsonrpc && cJSON_IsString(jsonrpc)) {
        request->jsonrpc = strdup(jsonrpc->valuestring);
    }

    // Extract id (can be string, number, or null) - notifications don't have id
    cJSON *id = cJSON_GetObjectItem(json, "id");
    if (id) {
        request->id = cJSON_Duplicate(id, 1);
        request->is_notification = false; // Has ID = request
    } else {
        request->is_notification = true;  // No ID = notification
    }

    // Extract method
    cJSON *method = cJSON_GetObjectItem(json, "method");
    if (method && cJSON_IsString(method)) {
        request->method = strdup(method->valuestring);
        mcp_debug_print("Extracted method: %s (is_notification: %s)\n",
                       request->method, request->is_notification ? "true" : "false");
    } else {
        mcp_debug_print("Failed to extract method: method=%p, is_string=%d\n",
                       method, method ? cJSON_IsString(method) : 0);
        cJSON_Delete(json);
        mcp_request_cleanup(request);
        return -1;
    }

    // Extract params (optional)
    cJSON *params = cJSON_GetObjectItem(json, "params");
    if (params) {
        request->params = cJSON_Duplicate(params, 1);
    }

    cJSON_Delete(json);
    return 0;
}

// Send JSON-RPC response
int mcp_send_response(const mcp_response_t *response) {
    if (!response) return -1;

    cJSON *json = cJSON_CreateObject();
    if (!json) return -1;

    // Add jsonrpc version
    cJSON_AddStringToObject(json, "jsonrpc", response->jsonrpc ? response->jsonrpc : "2.0");

    // Add id
    if (response->id) {
        cJSON_AddItemToObject(json, "id", cJSON_Duplicate(response->id, 1));
    } else {
        cJSON_AddNullToObject(json, "id");
    }

    // Add result or error
    if (response->result) {
        cJSON_AddItemToObject(json, "result", cJSON_Duplicate(response->result, 1));
    } else if (response->error) {
        cJSON_AddItemToObject(json, "error", cJSON_Duplicate(response->error, 1));
    }

    // Convert to string and send (use PrintUnformatted for single line)
    char *json_str = cJSON_PrintUnformatted(json);
    if (!json_str) {
        cJSON_Delete(json);
        return -1;
    }

    printf("%s\n", json_str);
    fflush(stdout);

    mcp_debug_print("Sent response: %s\n", json_str);

    free(json_str);
    cJSON_Delete(json);
    return 0;
}

// Send JSON-RPC error response
int mcp_send_error(cJSON *id, int code, const char *message) {
    mcp_response_t response = {0};
    response.jsonrpc = "2.0";
    response.id = id;

    cJSON *error = cJSON_CreateObject();
    if (!error) return -1;

    cJSON_AddNumberToObject(error, "code", code);
    cJSON_AddStringToObject(error, "message", message ? message : "Unknown error");

    response.error = error;

    int result = mcp_send_response(&response);
    cJSON_Delete(error);

    return result;
}
