#ifndef MCP_ERROR_CODES_H
#define MCP_ERROR_CODES_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Unified error codes for the entire MCP system
 * All modules should use these standardized error codes
 */
typedef enum {
    // Success
    MCP_OK = 0,
    
    // General errors (-1 to -99)
    MCP_ERROR_NULL_POINTER = -1,
    MCP_ERROR_INVALID_PARAMETER = -2,
    MCP_ERROR_MEMORY_ALLOCATION = -3,
    MCP_ERROR_NOT_INITIALIZED = -4,
    MCP_ERROR_ALREADY_INITIALIZED = -5,
    MCP_ERROR_NOT_SUPPORTED = -6,
    MCP_ERROR_TIMEOUT = -7,
    MCP_ERROR_BUFFER_TOO_SMALL = -8,
    MCP_ERROR_INVALID_STATE = -9,
    MCP_ERROR_NOT_FOUND = -10,
    MCP_ERROR_ALREADY_EXISTS = -11,
    MCP_ERROR_PERMISSION_DENIED = -12,
    MCP_ERROR_IO = -13,
    MCP_ERROR_PARSE = -14,
    MCP_ERROR_FORMAT = -15,
    
    // Platform/HAL errors (-100 to -199)
    MCP_ERROR_PLATFORM_NOT_AVAILABLE = -100,
    MCP_ERROR_PLATFORM_INIT_FAILED = -101,
    MCP_ERROR_HAL_OPERATION_FAILED = -102,
    MCP_ERROR_HARDWARE_FAILURE = -103,
    
    // Transport errors (-200 to -299)
    MCP_ERROR_TRANSPORT_INIT_FAILED = -200,
    MCP_ERROR_TRANSPORT_NOT_CONNECTED = -201,
    MCP_ERROR_TRANSPORT_SEND_FAILED = -202,
    MCP_ERROR_TRANSPORT_RECV_FAILED = -203,
    MCP_ERROR_TRANSPORT_TIMEOUT = -204,
    MCP_ERROR_CONNECTION_LOST = -205,
    MCP_ERROR_CONNECTION_REFUSED = -206,
    
    // Protocol errors (-300 to -399)
    MCP_ERROR_PROTOCOL_VERSION_MISMATCH = -300,
    MCP_ERROR_PROTOCOL_INVALID_MESSAGE = -301,
    MCP_ERROR_PROTOCOL_PARSE_ERROR = -302,
    MCP_ERROR_PROTOCOL_UNSUPPORTED_METHOD = -303,
    MCP_ERROR_PROTOCOL_INVALID_PARAMS = -304,
    MCP_ERROR_PROTOCOL_INTERNAL_ERROR = -305,
    
    // Tool errors (-400 to -499)
    MCP_ERROR_TOOL_NOT_FOUND = -400,
    MCP_ERROR_TOOL_EXECUTION_FAILED = -401,
    MCP_ERROR_TOOL_INVALID_PARAMS = -402,
    MCP_ERROR_TOOL_TIMEOUT = -403,
    MCP_ERROR_TOOL_PERMISSION_DENIED = -404,
    MCP_ERROR_TOOL_REGISTRY_FULL = -405,
    
    // Session errors (-500 to -599)
    MCP_ERROR_SESSION_NOT_FOUND = -500,
    MCP_ERROR_SESSION_EXPIRED = -501,
    MCP_ERROR_SESSION_INVALID = -502,
    MCP_ERROR_SESSION_LIMIT_EXCEEDED = -503,
    
    // JSON/Data errors (-600 to -699)
    MCP_ERROR_JSON_PARSE = -600,
    MCP_ERROR_JSON_INVALID_TYPE = -601,
    MCP_ERROR_JSON_MISSING_FIELD = -602,
    MCP_ERROR_JSON_INVALID_VALUE = -603,
    
    // Crypto/Encoding errors (-700 to -799)
    MCP_ERROR_CRYPTO_OPERATION_FAILED = -700,
    MCP_ERROR_BASE64_ENCODE_FAILED = -701,
    MCP_ERROR_BASE64_DECODE_FAILED = -702,
    MCP_ERROR_UUID_GENERATION_FAILED = -703,
    MCP_ERROR_HASH_OPERATION_FAILED = -704
} mcp_result_t;

/**
 * Convert error code to human-readable string
 * @param result Error code
 * @return Error description string
 */
const char* mcp_error_to_string(mcp_result_t result);

/**
 * Check if result indicates success
 * @param result Error code to check
 * @return true if success, false if error
 */
static inline bool mcp_is_success(mcp_result_t result) {
    return result == MCP_OK;
}

/**
 * Check if result indicates error
 * @param result Error code to check
 * @return true if error, false if success
 */
static inline bool mcp_is_error(mcp_result_t result) {
    return result != MCP_OK;
}

#ifdef __cplusplus
}
#endif

#endif // MCP_ERROR_CODES_H
