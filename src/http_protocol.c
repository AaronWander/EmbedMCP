#include "mcp_server.h"

// Parse HTTP request
int http_parse_request(const char *raw_request, http_request_t *request) {
    if (!raw_request || !request) return -1;
    
    memset(request, 0, sizeof(http_request_t));
    
    // Parse request line
    char *line_end = strstr(raw_request, "\r\n");
    if (!line_end) return -1;
    
    char request_line[512];
    size_t line_len = line_end - raw_request;
    if (line_len >= sizeof(request_line)) return -1;
    
    strncpy(request_line, raw_request, line_len);
    request_line[line_len] = '\0';
    
    // Extract method, path, and protocol
    char *token = strtok(request_line, " ");
    if (!token) return -1;
    strncpy(request->method, token, sizeof(request->method) - 1);
    
    token = strtok(NULL, " ");
    if (!token) return -1;
    strncpy(request->path, token, sizeof(request->path) - 1);
    
    token = strtok(NULL, " ");
    if (!token) return -1;
    strncpy(request->protocol, token, sizeof(request->protocol) - 1);
    
    // Parse headers
    const char *headers_start = line_end + 2;
    const char *body_start = strstr(headers_start, "\r\n\r\n");
    if (!body_start) return -1;
    
    size_t headers_len = body_start - headers_start;
    request->headers = malloc(headers_len + 1);
    if (!request->headers) return -1;
    
    strncpy(request->headers, headers_start, headers_len);
    request->headers[headers_len] = '\0';
    
    // Extract session ID from headers (try both cases)
    char *session_header = strstr(request->headers, "mcp-session-id:");
    if (!session_header) {
        session_header = strstr(request->headers, "Mcp-Session-Id:");
    }
    if (session_header) {
        session_header += 15; // Skip "mcp-session-id:" or "Mcp-Session-Id:"
        while (*session_header == ' ') session_header++; // Skip spaces

        char *session_end = strstr(session_header, "\r\n");
        if (session_end) {
            size_t session_len = session_end - session_header;
            if (session_len <= SESSION_ID_LENGTH) {
                strncpy(request->session_id, session_header, session_len);
                request->session_id[session_len] = '\0';
            }
        }
    }
    
    // Extract protocol version from headers
    char *version_header = strstr(request->headers, "MCP-Protocol-Version:");
    if (version_header) {
        version_header += 21; // Skip "MCP-Protocol-Version:"
        while (*version_header == ' ') version_header++; // Skip spaces
        
        char *version_end = strstr(version_header, "\r\n");
        if (version_end) {
            size_t version_len = version_end - version_header;
            if (version_len < sizeof(request->protocol_version)) {
                strncpy(request->protocol_version, version_header, version_len);
                request->protocol_version[version_len] = '\0';
            }
        }
    }
    
    // Parse body if present
    body_start += 4; // Skip "\r\n\r\n"
    size_t remaining_len = strlen(body_start);
    if (remaining_len > 0) {
        request->body = malloc(remaining_len + 1);
        if (!request->body) {
            free(request->headers);
            return -1;
        }
        strcpy(request->body, body_start);
        request->body_length = remaining_len;
    }
    
    mcp_debug_print("Parsed HTTP request: %s %s\n", request->method, request->path);
    return 0;
}

// Send HTTP response
int http_send_response(int client_fd, const http_response_t *response) {
    if (client_fd < 0 || !response) return -1;
    
    // Build status line
    char status_line[128];
    const char *status_text = "OK";
    switch (response->status_code) {
        case 200: status_text = "OK"; break;
        case 202: status_text = "Accepted"; break;
        case 400: status_text = "Bad Request"; break;
        case 404: status_text = "Not Found"; break;
        case 405: status_text = "Method Not Allowed"; break;
        case 500: status_text = "Internal Server Error"; break;
    }
    
    snprintf(status_line, sizeof(status_line), "HTTP/1.1 %d %s\r\n", 
             response->status_code, status_text);
    
    // Send status line
    if (send(client_fd, status_line, strlen(status_line), 0) < 0) {
        return -1;
    }
    
    // Send headers
    if (response->headers) {
        if (send(client_fd, response->headers, strlen(response->headers), 0) < 0) {
            return -1;
        }
    }
    
    // Send default headers
    char default_headers[512];
    if (response->is_sse) {
        snprintf(default_headers, sizeof(default_headers),
                "Content-Type: text/event-stream\r\n"
                "Cache-Control: no-cache\r\n"
                "Connection: keep-alive\r\n"
                "Access-Control-Allow-Origin: *\r\n"
                "Access-Control-Allow-Headers: *\r\n"
                "\r\n");
    } else {
        snprintf(default_headers, sizeof(default_headers),
                "Content-Type: application/json\r\n"
                "Content-Length: %zu\r\n"
                "Access-Control-Allow-Origin: *\r\n"
                "Access-Control-Allow-Headers: *\r\n"
                "Access-Control-Expose-Headers: mcp-session-id\r\n"
                "Access-Control-Allow-Methods: GET, POST, DELETE, OPTIONS\r\n"
                "\r\n", response->body_length);
    }
    
    if (send(client_fd, default_headers, strlen(default_headers), 0) < 0) {
        return -1;
    }
    
    // Send body if present and not SSE
    if (response->body && response->body_length > 0 && !response->is_sse) {
        if (send(client_fd, response->body, response->body_length, 0) < 0) {
            return -1;
        }
    }
    
    mcp_debug_print("Sent HTTP response: %d\n", response->status_code);
    return 0;
}

// Send SSE message
int http_send_sse_message(int client_fd, const char *data) {
    if (client_fd < 0 || !data) return -1;
    
    char sse_message[BUFFER_SIZE];
    snprintf(sse_message, sizeof(sse_message), "data: %s\n\n", data);
    
    if (send(client_fd, sse_message, strlen(sse_message), 0) < 0) {
        mcp_debug_print("Failed to send SSE message: %s\n", strerror(errno));
        return -1;
    }
    
    mcp_debug_print("Sent SSE message: %s\n", data);
    return 0;
}

// Cleanup HTTP request
void http_request_cleanup(http_request_t *request) {
    if (!request) return;
    
    if (request->headers) {
        free(request->headers);
        request->headers = NULL;
    }
    
    if (request->body) {
        free(request->body);
        request->body = NULL;
    }
}

// Cleanup HTTP response
void http_response_cleanup(http_response_t *response) {
    if (!response) return;
    
    if (response->headers) {
        free(response->headers);
        response->headers = NULL;
    }
    
    if (response->body) {
        free(response->body);
        response->body = NULL;
    }
}
