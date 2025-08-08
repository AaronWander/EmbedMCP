#include "error_codes.h"

const char* mcp_error_to_string(mcp_result_t result) {
    switch (result) {
        // Success
        case MCP_OK:
            return "Success";
            
        // General errors
        case MCP_ERROR_NULL_POINTER:
            return "Null pointer error";
        case MCP_ERROR_INVALID_PARAMETER:
            return "Invalid parameter";
        case MCP_ERROR_MEMORY_ALLOCATION:
            return "Memory allocation failed";
        case MCP_ERROR_NOT_INITIALIZED:
            return "Component not initialized";
        case MCP_ERROR_ALREADY_INITIALIZED:
            return "Component already initialized";
        case MCP_ERROR_NOT_SUPPORTED:
            return "Operation not supported";
        case MCP_ERROR_TIMEOUT:
            return "Operation timed out";
        case MCP_ERROR_BUFFER_TOO_SMALL:
            return "Buffer too small";
        case MCP_ERROR_INVALID_STATE:
            return "Invalid state";
        case MCP_ERROR_NOT_FOUND:
            return "Resource not found";
        case MCP_ERROR_ALREADY_EXISTS:
            return "Resource already exists";
        case MCP_ERROR_PERMISSION_DENIED:
            return "Permission denied";
        case MCP_ERROR_IO:
            return "I/O error";
        case MCP_ERROR_PARSE:
            return "Parse error";
        case MCP_ERROR_FORMAT:
            return "Format error";
            
        // Platform/HAL errors
        case MCP_ERROR_PLATFORM_NOT_AVAILABLE:
            return "Platform HAL not available";
        case MCP_ERROR_PLATFORM_INIT_FAILED:
            return "Platform initialization failed";
        case MCP_ERROR_HAL_OPERATION_FAILED:
            return "HAL operation failed";
        case MCP_ERROR_HARDWARE_FAILURE:
            return "Hardware failure";
            
        // Transport errors
        case MCP_ERROR_TRANSPORT_INIT_FAILED:
            return "Transport initialization failed";
        case MCP_ERROR_TRANSPORT_NOT_CONNECTED:
            return "Transport not connected";
        case MCP_ERROR_TRANSPORT_SEND_FAILED:
            return "Transport send failed";
        case MCP_ERROR_TRANSPORT_RECV_FAILED:
            return "Transport receive failed";
        case MCP_ERROR_TRANSPORT_TIMEOUT:
            return "Transport timeout";
        case MCP_ERROR_CONNECTION_LOST:
            return "Connection lost";
        case MCP_ERROR_CONNECTION_REFUSED:
            return "Connection refused";
            
        // Protocol errors
        case MCP_ERROR_PROTOCOL_VERSION_MISMATCH:
            return "Protocol version mismatch";
        case MCP_ERROR_PROTOCOL_INVALID_MESSAGE:
            return "Invalid protocol message";
        case MCP_ERROR_PROTOCOL_PARSE_ERROR:
            return "Protocol parse error";
        case MCP_ERROR_PROTOCOL_UNSUPPORTED_METHOD:
            return "Unsupported protocol method";
        case MCP_ERROR_PROTOCOL_INVALID_PARAMS:
            return "Invalid protocol parameters";
        case MCP_ERROR_PROTOCOL_INTERNAL_ERROR:
            return "Protocol internal error";
            
        // Tool errors
        case MCP_ERROR_TOOL_NOT_FOUND:
            return "Tool not found";
        case MCP_ERROR_TOOL_EXECUTION_FAILED:
            return "Tool execution failed";
        case MCP_ERROR_TOOL_INVALID_PARAMS:
            return "Invalid tool parameters";
        case MCP_ERROR_TOOL_TIMEOUT:
            return "Tool execution timeout";
        case MCP_ERROR_TOOL_PERMISSION_DENIED:
            return "Tool permission denied";
        case MCP_ERROR_TOOL_REGISTRY_FULL:
            return "Tool registry full";
            
        // Session errors
        case MCP_ERROR_SESSION_NOT_FOUND:
            return "Session not found";
        case MCP_ERROR_SESSION_EXPIRED:
            return "Session expired";
        case MCP_ERROR_SESSION_INVALID:
            return "Invalid session";
        case MCP_ERROR_SESSION_LIMIT_EXCEEDED:
            return "Session limit exceeded";
            
        // JSON/Data errors
        case MCP_ERROR_JSON_PARSE:
            return "JSON parse error";
        case MCP_ERROR_JSON_INVALID_TYPE:
            return "Invalid JSON type";
        case MCP_ERROR_JSON_MISSING_FIELD:
            return "Missing JSON field";
        case MCP_ERROR_JSON_INVALID_VALUE:
            return "Invalid JSON value";
            
        // Crypto/Encoding errors
        case MCP_ERROR_CRYPTO_OPERATION_FAILED:
            return "Crypto operation failed";
        case MCP_ERROR_BASE64_ENCODE_FAILED:
            return "Base64 encoding failed";
        case MCP_ERROR_BASE64_DECODE_FAILED:
            return "Base64 decoding failed";
        case MCP_ERROR_UUID_GENERATION_FAILED:
            return "UUID generation failed";
        case MCP_ERROR_HASH_OPERATION_FAILED:
            return "Hash operation failed";
            
        default:
            return "Unknown error";
    }
}
