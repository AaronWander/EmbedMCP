#include "mcp_server.h"

// Handle MCP request and return JSON response
cJSON *handle_mcp_request(const mcp_request_t *request) {
    if (!request || !request->method) return NULL;
    
    mcp_debug_print("Handling MCP request: %s\n", request->method);
    
    // Create response object
    cJSON *response = cJSON_CreateObject();
    if (!response) return NULL;
    
    cJSON_AddStringToObject(response, "jsonrpc", "2.0");
    if (request->id) {
        cJSON_AddItemToObject(response, "id", cJSON_Duplicate(request->id, 1));
    } else {
        cJSON_AddNullToObject(response, "id");
    }
    
    // Route to appropriate handler
    if (strcmp(request->method, "initialize") == 0) {
        cJSON *result = cJSON_CreateObject();
        // Add protocolVersion as required by MCP spec
        cJSON_AddStringToObject(result, "protocolVersion", MCP_PROTOCOL_VERSION);
        cJSON_AddItemToObject(result, "capabilities", mcp_create_server_capabilities());
        cJSON_AddItemToObject(result, "serverInfo", mcp_create_server_info());
        cJSON_AddItemToObject(response, "result", result);
        
    } else if (strcmp(request->method, "tools/list") == 0) {
        cJSON *result = cJSON_CreateObject();
        cJSON *tools_array = cJSON_CreateArray();
        
        // Add the "add" tool
        cJSON *add_tool = cJSON_CreateObject();
        cJSON_AddStringToObject(add_tool, "name", "add");
        cJSON_AddStringToObject(add_tool, "title", "Add Two Numbers");
        cJSON_AddStringToObject(add_tool, "description", "Calculate the sum of two numbers");
        
        // Create input schema
        cJSON *input_schema = cJSON_CreateObject();
        cJSON_AddStringToObject(input_schema, "type", "object");
        
        cJSON *properties = cJSON_CreateObject();
        cJSON *num1 = cJSON_CreateObject();
        cJSON_AddStringToObject(num1, "type", "number");
        cJSON_AddStringToObject(num1, "description", "First number to add");
        cJSON_AddItemToObject(properties, "num1", num1);
        
        cJSON *num2 = cJSON_CreateObject();
        cJSON_AddStringToObject(num2, "type", "number");
        cJSON_AddStringToObject(num2, "description", "Second number to add");
        cJSON_AddItemToObject(properties, "num2", num2);
        
        cJSON_AddItemToObject(input_schema, "properties", properties);
        
        cJSON *required = cJSON_CreateArray();
        cJSON_AddItemToArray(required, cJSON_CreateString("num1"));
        cJSON_AddItemToArray(required, cJSON_CreateString("num2"));
        cJSON_AddItemToObject(input_schema, "required", required);
        
        cJSON_AddItemToObject(add_tool, "inputSchema", input_schema);
        cJSON_AddItemToArray(tools_array, add_tool);
        
        cJSON_AddItemToObject(result, "tools", tools_array);
        cJSON_AddItemToObject(response, "result", result);
        
    } else if (strcmp(request->method, "tools/call") == 0) {
        // Handle tool call
        if (request->params) {
            cJSON *name = cJSON_GetObjectItem(request->params, "name");
            cJSON *arguments = cJSON_GetObjectItem(request->params, "arguments");
            
            if (name && cJSON_IsString(name) && arguments) {
                if (strcmp(name->valuestring, "add") == 0) {
                    cJSON *tool_result = NULL;
                    int tool_ret = mcp_tool_add(arguments, &tool_result);
                    
                    cJSON *result = cJSON_CreateObject();
                    if (tool_ret == 0 && tool_result) {
                        cJSON *content = cJSON_CreateArray();
                        cJSON *text_content = cJSON_CreateObject();
                        cJSON_AddStringToObject(text_content, "type", "text");
                        
                        char *result_str = cJSON_PrintUnformatted(tool_result);
                        cJSON_AddStringToObject(text_content, "text", result_str ? result_str : "null");
                        if (result_str) free(result_str);
                        
                        cJSON_AddItemToArray(content, text_content);
                        cJSON_AddItemToObject(result, "content", content);
                        cJSON_AddBoolToObject(result, "isError", false);
                        
                        cJSON_Delete(tool_result);
                    } else {
                        cJSON *content = cJSON_CreateArray();
                        cJSON *text_content = cJSON_CreateObject();
                        cJSON_AddStringToObject(text_content, "type", "text");
                        cJSON_AddStringToObject(text_content, "text", "Tool execution failed");
                        cJSON_AddItemToArray(content, text_content);
                        cJSON_AddItemToObject(result, "content", content);
                        cJSON_AddBoolToObject(result, "isError", true);
                    }
                    
                    cJSON_AddItemToObject(response, "result", result);
                } else {
                    cJSON *error = cJSON_CreateObject();
                    cJSON_AddNumberToObject(error, "code", JSONRPC_METHOD_NOT_FOUND);
                    cJSON_AddStringToObject(error, "message", "Unknown tool");
                    cJSON_AddItemToObject(response, "error", error);
                }
            } else {
                cJSON *error = cJSON_CreateObject();
                cJSON_AddNumberToObject(error, "code", JSONRPC_INVALID_PARAMS);
                cJSON_AddStringToObject(error, "message", "Invalid parameters");
                cJSON_AddItemToObject(response, "error", error);
            }
        } else {
            cJSON *error = cJSON_CreateObject();
            cJSON_AddNumberToObject(error, "code", JSONRPC_INVALID_PARAMS);
            cJSON_AddStringToObject(error, "message", "Missing parameters");
            cJSON_AddItemToObject(response, "error", error);
        }
        
    } else if (strcmp(request->method, "notifications/initialized") == 0) {
        // Client confirms initialization is complete - no response needed for notifications
        cJSON_Delete(response);
        return NULL;
        
    } else {
        cJSON *error = cJSON_CreateObject();
        cJSON_AddNumberToObject(error, "code", JSONRPC_METHOD_NOT_FOUND);
        cJSON_AddStringToObject(error, "message", "Method not found");
        cJSON_AddItemToObject(response, "error", error);
    }
    
    return response;
}
