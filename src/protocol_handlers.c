#include "mcp_server.h"

// Handle initialize request
int mcp_handle_initialize(mcp_server_t *server, const mcp_request_t *request) {
    if (!server || !request) return -1;
    
    mcp_debug_print("Handling initialize request\n");
    
    // Parse client capabilities and info
    if (request->params) {
        cJSON *capabilities = cJSON_GetObjectItem(request->params, "capabilities");
        if (capabilities) {
            server->client_capabilities = cJSON_Duplicate(capabilities, 1);
        }
        
        cJSON *protocol_version = cJSON_GetObjectItem(request->params, "protocolVersion");
        if (protocol_version && cJSON_IsString(protocol_version)) {
            // Check if we support this protocol version
            if (strcmp(protocol_version->valuestring, MCP_PROTOCOL_VERSION) != 0) {
                mcp_debug_print("Protocol version mismatch: client=%s, server=%s\n", 
                               protocol_version->valuestring, MCP_PROTOCOL_VERSION);
                // For now, we'll accept it and respond with our version
            }
        }
    }
    
    // Create response
    mcp_response_t response = {0};
    response.jsonrpc = "2.0";
    response.id = request->id;
    
    cJSON *result = cJSON_CreateObject();
    if (!result) return -1;
    
    // Add protocol version
    cJSON_AddStringToObject(result, "protocolVersion", MCP_PROTOCOL_VERSION);
    
    // Add server capabilities
    cJSON_AddItemToObject(result, "capabilities", cJSON_Duplicate(server->server_capabilities, 1));
    
    // Add server info
    cJSON *server_info = mcp_create_server_info();
    cJSON_AddItemToObject(result, "serverInfo", server_info);
    
    response.result = result;
    
    // Send response
    int ret = mcp_send_response(&response);
    
    // Mark server as initialized
    if (ret == 0) {
        server->initialized = true;
        mcp_debug_print("Server initialization completed\n");
    }
    
    cJSON_Delete(result);
    return ret;
}

// Handle tools/list request
int mcp_handle_list_tools(mcp_server_t *server, const mcp_request_t *request) {
    if (!server || !request) return -1;
    
    mcp_debug_print("Handling list_tools request\n");
    
    // Create response
    mcp_response_t response = {0};
    response.jsonrpc = "2.0";
    response.id = request->id;
    
    cJSON *result = cJSON_CreateObject();
    if (!result) return -1;
    
    cJSON *tools_array = cJSON_CreateArray();
    if (!tools_array) {
        cJSON_Delete(result);
        return -1;
    }
    
    // Add the "add" tool
    cJSON *add_tool = cJSON_CreateObject();
    cJSON_AddStringToObject(add_tool, "name", "add");
    cJSON_AddStringToObject(add_tool, "title", "Add Two Numbers");
    cJSON_AddStringToObject(add_tool, "description", "Calculate the sum of two numbers");
    
    // Create input schema for the add tool
    cJSON *input_schema = cJSON_CreateObject();
    cJSON_AddStringToObject(input_schema, "type", "object");
    
    cJSON *properties = cJSON_CreateObject();
    
    // First number parameter
    cJSON *num1 = cJSON_CreateObject();
    cJSON_AddStringToObject(num1, "type", "number");
    cJSON_AddStringToObject(num1, "description", "First number to add");
    cJSON_AddItemToObject(properties, "num1", num1);
    
    // Second number parameter
    cJSON *num2 = cJSON_CreateObject();
    cJSON_AddStringToObject(num2, "type", "number");
    cJSON_AddStringToObject(num2, "description", "Second number to add");
    cJSON_AddItemToObject(properties, "num2", num2);
    
    cJSON_AddItemToObject(input_schema, "properties", properties);
    
    // Required parameters
    cJSON *required = cJSON_CreateArray();
    cJSON_AddItemToArray(required, cJSON_CreateString("num1"));
    cJSON_AddItemToArray(required, cJSON_CreateString("num2"));
    cJSON_AddItemToObject(input_schema, "required", required);
    
    cJSON_AddItemToObject(add_tool, "inputSchema", input_schema);
    
    // Add tool to array
    cJSON_AddItemToArray(tools_array, add_tool);
    
    cJSON_AddItemToObject(result, "tools", tools_array);
    
    response.result = result;
    
    // Send response
    int ret = mcp_send_response(&response);
    
    cJSON_Delete(result);
    return ret;
}

// Handle tools/call request
int mcp_handle_call_tool(mcp_server_t *server, const mcp_request_t *request) {
    if (!server || !request || !request->params) return -1;

    mcp_debug_print("Handling call_tool request\n");

    // Extract tool name
    cJSON *name = cJSON_GetObjectItem(request->params, "name");
    if (!name || !cJSON_IsString(name)) {
        mcp_send_error(request->id, JSONRPC_INVALID_PARAMS, "Missing or invalid tool name");
        return -1;
    }

    // Extract arguments
    cJSON *arguments = cJSON_GetObjectItem(request->params, "arguments");
    if (!arguments) {
        mcp_send_error(request->id, JSONRPC_INVALID_PARAMS, "Missing arguments");
        return -1;
    }

    mcp_debug_print("Calling tool: %s\n", name->valuestring);

    // Route to appropriate tool handler
    cJSON *tool_result = NULL;
    int tool_ret = -1;

    if (strcmp(name->valuestring, "add") == 0) {
        tool_ret = mcp_tool_add(arguments, &tool_result);
    } else {
        mcp_send_error(request->id, JSONRPC_METHOD_NOT_FOUND, "Unknown tool");
        return -1;
    }

    // Create response
    mcp_response_t response = {0};
    response.jsonrpc = "2.0";
    response.id = request->id;

    cJSON *result = cJSON_CreateObject();
    if (!result) {
        if (tool_result) cJSON_Delete(tool_result);
        return -1;
    }

    if (tool_ret == 0 && tool_result) {
        // Success - create content array with text result
        cJSON *content = cJSON_CreateArray();
        cJSON *text_content = cJSON_CreateObject();
        cJSON_AddStringToObject(text_content, "type", "text");

        // Convert result to string
        char *result_str = cJSON_Print(tool_result);
        cJSON_AddStringToObject(text_content, "text", result_str ? result_str : "null");
        if (result_str) free(result_str);

        cJSON_AddItemToArray(content, text_content);
        cJSON_AddItemToObject(result, "content", content);
        cJSON_AddBoolToObject(result, "isError", false);
    } else {
        // Error - create error content
        cJSON *content = cJSON_CreateArray();
        cJSON *text_content = cJSON_CreateObject();
        cJSON_AddStringToObject(text_content, "type", "text");
        cJSON_AddStringToObject(text_content, "text", "Tool execution failed");
        cJSON_AddItemToArray(content, text_content);
        cJSON_AddItemToObject(result, "content", content);
        cJSON_AddBoolToObject(result, "isError", true);
    }

    response.result = result;

    // Send response
    int ret = mcp_send_response(&response);

    if (tool_result) cJSON_Delete(tool_result);
    cJSON_Delete(result);
    return ret;
}
