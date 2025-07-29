#include "mcp_server.h"

extern mcp_server_t *g_http_server; // Global reference for thread access

// Safe string duplication with fallback
static char* safe_strdup(const char *str) {
    if (!str) return NULL;

    char *result = strdup(str);
    if (!result) {
        mcp_debug_print("strdup failed for string of length %zu\n", strlen(str));
        // Try to free some memory and retry once
        result = strdup(str);
    }
    return result;
}

// Safe helper function to create error response
static int send_error_response(int client_fd, int status_code, const char *error_msg) {
    http_response_t error_response = {0};
    error_response.status_code = status_code;
    error_response.body = safe_strdup(error_msg);

    if (!error_response.body) {
        mcp_debug_print("Failed to allocate memory for error response\n");
        // Send minimal response without body
        error_response.body = NULL;
        error_response.body_length = 0;
        int result = http_send_response(client_fd, &error_response);
        return result;
    }

    error_response.body_length = strlen(error_response.body);
    int result = http_send_response(client_fd, &error_response);
    http_response_cleanup(&error_response);
    return result;
}

// Send JSON response with optional session header
static int send_json_response(int client_fd, cJSON *response_json, const char *session_id) {
    if (!response_json) {
        return send_error_response(client_fd, 500,
            "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32603,\"message\":\"Internal server error\"},\"id\":null}");
    }

    char *response_str = cJSON_PrintUnformatted(response_json);
    if (!response_str) {
        cJSON_Delete(response_json);
        return send_error_response(client_fd, 500,
            "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32603,\"message\":\"Failed to serialize response\"},\"id\":null}");
    }

    http_response_t response = {0};
    response.status_code = 200;
    response.body = safe_strdup(response_str);
    response.body_length = strlen(response_str);

    // Add session header and connection header if provided
    if (session_id) {
        char session_header[512];
        snprintf(session_header, sizeof(session_header),
                "mcp-session-id: %s\r\n"
                "Connection: keep-alive\r\n"
                "Access-Control-Expose-Headers: mcp-session-id\r\n",
                session_id);
        response.headers = safe_strdup(session_header);
        mcp_debug_print("Added session header: mcp-session-id: %s\n", session_id);
    }

    int result = http_send_response(client_fd, &response);
    http_response_cleanup(&response);
    free(response_str);
    cJSON_Delete(response_json);
    return result;
}

// Validate session from request
static mcp_session_t* validate_session(const http_request_t *request) {
    if (request->session_id[0] == '\0') {
        mcp_debug_print("No session ID in request\n");
        return NULL;
    }

    mcp_debug_print("Looking for session: '%s'\n", request->session_id);
    mcp_session_t *session = mcp_find_session(g_http_server, request->session_id);

    if (!session) {
        mcp_debug_print("Session not found\n");
        return NULL;
    }

    if (!session->initialized) {
        mcp_debug_print("Session not initialized\n");
        return NULL;
    }

    mcp_debug_print("Found valid session: '%s'\n", session->session_id);
    return session;
}

// Handle initialized notification
static int handle_initialized_notification(int client_fd, const http_request_t *request, const mcp_request_t *mcp_req) {
    (void)mcp_req; // Unused parameter

    mcp_debug_print("Handling initialized notification\n");

    if (request->session_id[0] == '\0') {
        mcp_debug_print("No session ID in initialized notification\n");
        return send_error_response(client_fd, 400,
            "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32600,\"message\":\"Missing session ID\"},\"id\":null}");
    }

    mcp_session_t *session = mcp_find_session(g_http_server, request->session_id);
    if (!session) {
        mcp_debug_print("Session not found for initialized notification\n");
        return send_error_response(client_fd, 400,
            "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32600,\"message\":\"Invalid session ID\"},\"id\":null}");
    }

    // Mark session as fully initialized
    session->initialized = true;
    mcp_debug_print("Session '%s' marked as initialized\n", session->session_id);

    // Send 202 Accepted for notification (no response body needed)
    // This is the correct response for MCP notifications according to the spec
    http_response_t response = {0};
    response.status_code = 202;  // 202 Accepted for notifications
    response.body = NULL;        // No body for notifications
    response.body_length = 0;

    mcp_debug_print("Sending 202 Accepted for initialized notification\n");
    int result = http_send_response(client_fd, &response);
    return result;
}

// HTTP client handler thread
void *http_client_handler(void *arg) {
    int client_fd = *(int*)arg;
    free(arg); // Free the allocated integer

    // Use heap memory to avoid stack overflow
    char *buffer = malloc(BUFFER_SIZE);
    if (!buffer) {
        mcp_debug_print("Failed to allocate buffer for client %d\n", client_fd);
        close(client_fd);
        return NULL;
    }

    ssize_t bytes_received;

    mcp_debug_print("HTTP client handler started for fd %d\n", client_fd);

    // Check if global server is available
    if (!g_http_server) {
        mcp_debug_print("ERROR: g_http_server is NULL in client handler\n");
        close(client_fd);
        return NULL;
    }
    
    while (g_running) {
        // Receive HTTP request - may need multiple recv calls
        size_t total_received = 0;
        bool request_complete = false;

        while (!request_complete && total_received < BUFFER_SIZE - 1) {
            bytes_received = recv(client_fd, buffer + total_received,
                                BUFFER_SIZE - 1 - total_received, 0);
            if (bytes_received <= 0) {
                if (bytes_received == 0) {
                    mcp_debug_print("Client %d disconnected\n", client_fd);
                } else {
                    mcp_debug_print("Error receiving from client %d: %s\n", client_fd, strerror(errno));
                }
                goto cleanup;
            }

            total_received += bytes_received;
            buffer[total_received] = '\0';

            // Check if we have a complete HTTP request (ends with \r\n\r\n)
            if (strstr(buffer, "\r\n\r\n") != NULL) {
                request_complete = true;
            }
        }

        mcp_debug_print("Received HTTP request from client %d (%zu bytes)\n", client_fd, total_received);
        mcp_debug_print("Raw HTTP request:\n%s\n", buffer);

        // Parse HTTP request
        http_request_t request;
        if (http_parse_request(buffer, &request) != 0) {
            mcp_debug_print("Failed to parse HTTP request\n");
            
            // Send 400 Bad Request
            send_error_response(client_fd, 400, "{\"error\":\"Bad Request\"}");
            break;
        }
        
        // Check if this is a valid MCP endpoint
        if (request.method[0] == '\0' || request.path[0] == '\0') {
            mcp_debug_print("ERROR: Empty method or path in request\n");
            break;
        }

        mcp_debug_print("Processing request: method=%s, path=%s, session_id='%s'\n",
                       request.method, request.path, request.session_id);

        // Check for keep-alive connection early, before headers might be cleaned up
        bool keep_alive = false;
        if (request.headers) {
            mcp_debug_print("HTTP headers:\n%s\n", request.headers);
            if (strstr(request.headers, "connection: keep-alive") ||
                strstr(request.headers, "Connection: keep-alive")) {
                keep_alive = true;
                mcp_debug_print("Keep-alive connection detected\n");
            }
        } else {
            mcp_debug_print("No HTTP headers found\n");
        }

        bool is_mcp_endpoint = (strcmp(request.path, "/") == 0 || strcmp(request.path, "/mcp") == 0);  // Accept both root and /mcp
        bool is_sse_endpoint = (strcmp(request.path, "/sse") == 0);  // Legacy SSE endpoint
        bool is_messages_endpoint = (strcmp(request.path, "/messages") == 0);  // Legacy messages endpoint

        if (!is_mcp_endpoint && !is_sse_endpoint && !is_messages_endpoint) {
            // Send 404 Not Found for unknown endpoints
            http_response_t error_response = {0};
            error_response.status_code = 404;
            error_response.body = strdup("{\"error\":\"Not Found\"}");
            error_response.body_length = strlen(error_response.body);

            http_send_response(client_fd, &error_response);
            http_response_cleanup(&error_response);
            http_request_cleanup(&request);
            continue;
        }

        // Handle different HTTP methods based on endpoint
        if (is_mcp_endpoint) {
            // New Streamable HTTP transport (single endpoint)
            mcp_debug_print("Handling MCP endpoint\n");
            if (strcmp(request.method, "POST") == 0) {
                handle_http_post(client_fd, &request);
            } else if (strcmp(request.method, "GET") == 0) {
                handle_http_get(client_fd, &request);
            } else if (strcmp(request.method, "DELETE") == 0) {
                handle_http_delete(client_fd, &request);
            } else {
                // Send 405 Method Not Allowed
                http_response_t error_response = {0};
                error_response.status_code = 405;
                error_response.body = strdup("{\"error\":\"Method Not Allowed\"}");
                error_response.body_length = strlen(error_response.body);

                http_send_response(client_fd, &error_response);
                http_response_cleanup(&error_response);
            }
        } else if (is_sse_endpoint) {
            // Legacy SSE endpoint (GET only)
            if (strcmp(request.method, "GET") == 0) {
                handle_legacy_sse(client_fd, &request);
            } else {
                http_response_t error_response = {0};
                error_response.status_code = 405;
                error_response.body = strdup("{\"error\":\"Method Not Allowed\"}");
                error_response.body_length = strlen(error_response.body);

                http_send_response(client_fd, &error_response);
                http_response_cleanup(&error_response);
            }
        } else if (is_messages_endpoint) {
            // Legacy messages endpoint (POST only)
            if (strcmp(request.method, "POST") == 0) {
                handle_legacy_messages(client_fd, &request);
            } else {
                http_response_t error_response = {0};
                error_response.status_code = 405;
                error_response.body = strdup("{\"error\":\"Method Not Allowed\"}");
                error_response.body_length = strlen(error_response.body);

                http_send_response(client_fd, &error_response);
                http_response_cleanup(&error_response);
            }
        }
        
        http_request_cleanup(&request);

        if (!keep_alive) {
            mcp_debug_print("Request handled, closing connection (no keep-alive)\n");
            break;
        } else {
            mcp_debug_print("Request handled, keeping connection alive, waiting for next request...\n");
            // Continue to next iteration to wait for more requests
        }
    }

cleanup:
    // Cleanup
    mcp_remove_client(g_http_server, client_fd);
    mcp_debug_print("HTTP client handler ended for fd %d\n", client_fd);

    free(buffer);
    return NULL;
}

// Handle HTTP POST requests (JSON-RPC messages)
int handle_http_post(int client_fd, const http_request_t *request) {
    if (!request || !request->body) {
        http_response_t error_response = {0};
        error_response.status_code = 400;
        error_response.body = strdup("{\"error\":\"Missing request body\"}");
        error_response.body_length = strlen(error_response.body);
        
        http_send_response(client_fd, &error_response);
        http_response_cleanup(&error_response);
        return -1;
    }
    
    mcp_debug_print("Handling POST request\n");
    
    // Parse JSON-RPC message
    mcp_request_t mcp_req;
    mcp_debug_print("About to parse JSON request body\n");
    int parse_result = mcp_parse_request(request->body, &mcp_req);
    mcp_debug_print("Parse result: %d\n", parse_result);

    if (parse_result != 0) {
        mcp_debug_print("JSON parsing failed\n");
        http_response_t error_response = {0};
        error_response.status_code = 400;
        error_response.body = strdup("{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32700,\"message\":\"Parse error\"},\"id\":null}");
        error_response.body_length = strlen(error_response.body);

        http_send_response(client_fd, &error_response);
        http_response_cleanup(&error_response);
        return -1;
    }

    mcp_debug_print("JSON parsing succeeded, method=%p\n", mcp_req.method);
    mcp_debug_print("Method string: '%s'\n", mcp_req.method ? mcp_req.method : "NULL");

    // Check if method is valid
    if (!mcp_req.method) {
        mcp_debug_print("ERROR: mcp_req.method is NULL\n");
        http_response_t error_response = {0};
        error_response.status_code = 400;
        error_response.body = strdup("{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32600,\"message\":\"Invalid request - missing method\"},\"id\":null}");
        error_response.body_length = strlen(error_response.body);

        http_send_response(client_fd, &error_response);
        http_response_cleanup(&error_response);
        mcp_request_cleanup(&mcp_req);
        return -1;
    }

    // Handle initialize request specially (no session required)
    if (strcmp(mcp_req.method, "initialize") == 0) {
        int result = handle_http_initialize(client_fd, request, &mcp_req);
        mcp_request_cleanup(&mcp_req);
        return result;
    }

    // Handle initialized notification (session required but not fully initialized yet)
    if (strcmp(mcp_req.method, "notifications/initialized") == 0 ||
        strcmp(mcp_req.method, "initialized") == 0) {
        mcp_debug_print("Received initialized notification: %s (is_notification: %s)\n",
                       mcp_req.method, mcp_req.is_notification ? "true" : "false");
        int result = handle_initialized_notification(client_fd, request, &mcp_req);
        mcp_request_cleanup(&mcp_req);
        return result;
    }

    // Check if this is a notification (no id field)
    if (mcp_req.is_notification) {
        mcp_debug_print("Handling notification: %s\n", mcp_req.method);

        // For notifications, we need a valid session (except for initialize)
        mcp_session_t *session = validate_session(request);
        if (!session) {
            mcp_request_cleanup(&mcp_req);
            return send_error_response(client_fd, 400,
                "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32600,\"message\":\"Server not initialized or invalid session\"},\"id\":null}");
        }

        // Process the notification (no response expected)
        // For now, we just acknowledge it with 202 Accepted
        http_response_t response = {0};
        response.status_code = 202;  // 202 Accepted for notifications
        response.body = NULL;        // No body for notifications
        response.body_length = 0;

        mcp_debug_print("Sending 202 Accepted for notification: %s\n", mcp_req.method);
        int result = http_send_response(client_fd, &response);
        mcp_request_cleanup(&mcp_req);
        return result;
    }

    // For requests (have id field), require initialized session
    mcp_session_t *session = validate_session(request);
    if (!session) {
        mcp_request_cleanup(&mcp_req);
        return send_error_response(client_fd, 400,
            "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32600,\"message\":\"Server not initialized or invalid session\"},\"id\":null}");
    }

    // Process the request with the validated session
    cJSON *response_json = handle_mcp_request(&mcp_req);
    int result = send_json_response(client_fd, response_json, NULL);

    mcp_request_cleanup(&mcp_req);
    return result;
}

// Handle HTTP GET requests (SSE connections)
int handle_http_get(int client_fd, const http_request_t *request) {
    mcp_debug_print("Handling GET request for SSE connection\n");
    
    // Check if client accepts SSE
    if (!request->headers || !strstr(request->headers, "text/event-stream")) {
        http_response_t error_response = {0};
        error_response.status_code = 405;
        error_response.body = strdup("{\"error\":\"SSE not supported by client\"}");
        error_response.body_length = strlen(error_response.body);
        
        http_send_response(client_fd, &error_response);
        http_response_cleanup(&error_response);
        return -1;
    }
    
    // Find session by session ID
    mcp_session_t *session = NULL;
    if (request->session_id[0] != '\0') {
        session = mcp_find_session(g_http_server, request->session_id);
    }

    if (!session) {
        http_response_t error_response = {0};
        error_response.status_code = 404;
        error_response.body = strdup("{\"error\":\"Session not found\"}");
        error_response.body_length = strlen(error_response.body);

        http_send_response(client_fd, &error_response);
        http_response_cleanup(&error_response);
        return -1;
    }
    
    // Start SSE stream
    http_response_t sse_response = {0};
    sse_response.status_code = 200;
    sse_response.is_sse = true;
    
    if (http_send_response(client_fd, &sse_response) == 0) {
        mcp_debug_print("SSE connection established for session %s\n", session->session_id);

        // Send initial SSE message
        http_send_sse_message(client_fd, "{\"type\":\"connection\",\"status\":\"connected\"}");

        return 0;
    }
    
    return -1;
}

// Handle HTTP DELETE requests (session termination)
int handle_http_delete(int client_fd, const http_request_t *request) {
    mcp_debug_print("Handling DELETE request for session termination\n");
    
    if (request->session_id[0] == '\0') {
        http_response_t error_response = {0};
        error_response.status_code = 400;
        error_response.body = strdup("{\"error\":\"Missing session ID\"}");
        error_response.body_length = strlen(error_response.body);
        
        http_send_response(client_fd, &error_response);
        http_response_cleanup(&error_response);
        return -1;
    }
    
    mcp_session_t *session = mcp_find_session(g_http_server, request->session_id);
    if (!session) {
        http_response_t error_response = {0};
        error_response.status_code = 404;
        error_response.body = strdup("{\"error\":\"Session not found\"}");
        error_response.body_length = strlen(error_response.body);

        http_send_response(client_fd, &error_response);
        http_response_cleanup(&error_response);
        return -1;
    }
    
    // Send 200 OK response
    http_response_t response = {0};
    response.status_code = 200;
    response.body = strdup("{\"status\":\"session terminated\"}");
    response.body_length = strlen(response.body);
    
    http_send_response(client_fd, &response);
    http_response_cleanup(&response);
    
    // Remove session
    mcp_remove_session(g_http_server, request->session_id);
    
    return 0;
}

// Handle HTTP initialize request
int handle_http_initialize(int client_fd, const http_request_t *request, const mcp_request_t *mcp_req) {
    (void)request; // Unused parameter
    mcp_debug_print("Handling HTTP initialize request\n");

    // Create new session
    char *session_id = mcp_create_session(g_http_server);
    if (!session_id) {
        return send_error_response(client_fd, 500,
            "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32603,\"message\":\"Server full\"},\"id\":null}");
    }

    // Extract client capabilities from request
    cJSON *client_capabilities = NULL;
    if (mcp_req->params) {
        client_capabilities = cJSON_GetObjectItem(mcp_req->params, "capabilities");
    }

    // Initialize session (but don't mark as fully initialized yet)
    if (mcp_initialize_session(g_http_server, session_id, client_capabilities) != 0) {
        mcp_remove_session(g_http_server, session_id);
        free(session_id);
        return send_error_response(client_fd, 500,
            "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32603,\"message\":\"Failed to initialize session\"},\"id\":null}");
    }

    // Handle initialize request and generate response
    cJSON *response_json = handle_mcp_request(mcp_req);
    if (!response_json) {
        mcp_remove_session(g_http_server, session_id);
        free(session_id);
        return send_error_response(client_fd, 500,
            "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32603,\"message\":\"Failed to process initialize request\"},\"id\":null}");
    }

    mcp_debug_print("Sending initialize response with session ID: %s\n", session_id);

    // Debug: Print the actual JSON response
    char *debug_json = cJSON_Print(response_json);
    if (debug_json) {
        mcp_debug_print("Initialize response JSON:\n%s\n", debug_json);
        free(debug_json);
    }

    // Send response with session ID in header
    int result = send_json_response(client_fd, response_json, session_id);

    if (result != 0) {
        // Cleanup on failure
        mcp_remove_session(g_http_server, session_id);
    }

    free(session_id);
    return result;
}

// Handle legacy SSE endpoint (/sse)
int handle_legacy_sse(int client_fd, const http_request_t *request) {
    mcp_debug_print("Handling legacy SSE endpoint\n");

    // For now, redirect to the main SSE handler
    return handle_http_get(client_fd, request);
}

// Handle legacy messages endpoint (/messages)
int handle_legacy_messages(int client_fd, const http_request_t *request) {
    mcp_debug_print("Handling legacy messages endpoint\n");

    // For now, redirect to the main POST handler
    return handle_http_post(client_fd, request);
}
