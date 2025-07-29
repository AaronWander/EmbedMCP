# EmbedMCP - High-Level C MCP Server

A lightweight Model Context Protocol (MCP) server implementation in C with **FastMCP-like simplicity**. Designed for embedded Linux systems and resource-constrained environments.

## ✨ Features

- **🚀 FastMCP-like API**: No need to write JSON Schema manually
- **⚡ Lightweight**: Minimal memory footprint and dependencies
- **🔧 Embedded-friendly**: Perfect for embedded Linux systems
- **📋 Standards-compliant**: Implements MCP protocol version 2025-06-18
- **🏗️ Modular Architecture**: Clean separation of layers
- **🌐 Multiple Transports**: HTTP and STDIO support
- **🔌 Extensible**: Easy to add new tools and capabilities
- **🧵 Thread-safe**: Proper synchronization for multi-threaded environments

## 🎯 Quick Start

### Minimal Example (10 lines)
```c
#include "embed_mcp.h"

cJSON *hello_handler(const cJSON *args) {
    cJSON *response = cJSON_CreateObject();
    cJSON *content = cJSON_CreateArray();
    cJSON *text_content = cJSON_CreateObject();
    cJSON_AddStringToObject(text_content, "type", "text");
    cJSON_AddStringToObject(text_content, "text", "Hello from EmbedMCP!");
    cJSON_AddItemToArray(content, text_content);
    cJSON_AddItemToObject(response, "content", content);
    return response;
}

int main() {
    embed_mcp_server_t *server = embed_mcp_create_simple("MyServer", "1.0.0");
    embed_mcp_add_tool(server, "hello", "Say hello", hello_handler);
    embed_mcp_run_http(server);  // Starts on http://localhost:8080/mcp
    embed_mcp_destroy(server);
    return 0;
}
```

## 🔧 High-Level API

### Math Tools (Auto-generates number parameters)
```c
embed_mcp_add_math_tool(server, "add", "Add two numbers", add_handler);
embed_mcp_add_math_tool(server, "multiply", "Multiply numbers", multiply_handler);
```

### Text Tools (Auto-generates string parameters)
```c
embed_mcp_add_text_tool(server, "weather", "Get weather info",
                       "city", "City name", weather_handler);
```

### Generic Tools (Flexible schema)
```c
embed_mcp_add_tool(server, "custom", "Custom tool", custom_handler);
```

## Building

### Prerequisites
- GCC compiler
- Make
- curl (for downloading cJSON dependency)
- POSIX-compliant system (Linux, macOS, etc.)

### Build Steps

1. **Download dependencies:**
   ```bash
   make deps
   ```

2. **Compile the server:**
   ```bash
   make
   ```

3. **Run the server:**
   ```bash
   ./bin/mcp_server
   ```

### Build Options

- **Debug build:** `make debug` (includes debug symbols and logging)
- **Clean build:** `make clean` (removes all build artifacts)
- **Individual modules:** `make protocol`, `make transport`, `make tools`
- **Build info:** `make info` (shows build configuration)
- **Check files:** `make check` (verifies source files exist)

## Usage

### Standalone Testing

Test the server directly with JSON-RPC messages:

```bash
echo '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-06-18","capabilities":{},"clientInfo":{"name":"TestClient","version":"1.0.0"}}}' | ./bin/mcp_server
```

### Integration with MCP Clients

The server can be integrated with any MCP-compatible client (like Claude Desktop) by configuring it as a stdio transport server.

Example configuration for Claude Desktop (`claude_desktop_config.json`):

```json
{
  "mcpServers": {
    "embedmcp": {
      "command": "/path/to/EmbedMCP/bin/mcp_server",
      "args": []
    }
  }
}
```

## Protocol Support

### Implemented Methods

- `initialize` - Server initialization and capability negotiation
- `notifications/initialized` - Client initialization confirmation
- `tools/list` - List available tools
- `tools/call` - Execute a tool

### Capabilities

- **Tools**: Exposes callable functions to LLMs
- **Logging**: Basic logging support (stderr)

## Architecture

### Modular Design

The EmbedMCP server now features a clean, layered architecture:

```
EmbedMCP/
├── include/
│   ├── protocol/     # Protocol Layer: MCP protocol, JSON-RPC, message handling
│   ├── transport/    # Transport Layer: STDIO, HTTP, SSE transport implementations
│   ├── application/  # Application Layer: Server core, session/client management
│   ├── tools/        # Tools Layer: Tool interface, registry, built-in tools
│   └── utils/        # Utilities: Memory management, error handling, logging
└── src/
    ├── protocol/     # Protocol layer implementations
    ├── transport/    # Transport layer implementations
    ├── application/  # Application layer implementations
    ├── tools/        # Tools layer implementations
    └── utils/        # Utility implementations
```

### Layer Responsibilities

#### 1. Protocol Layer (`include/protocol/`, `src/protocol/`)
- **Message Handling** (`message.h/c`): MCP message structures and utilities
- **JSON-RPC Processing** (`jsonrpc.h/c`): JSON-RPC 2.0 parser and serializer
- **Protocol State Machine** (`protocol_state.h/c`): MCP session state management
- **MCP Protocol Handler** (`mcp_protocol.h/c`): Core MCP protocol implementation

#### 2. Transport Layer (`include/transport/`, `src/transport/`)
- **Transport Interface** (`transport_interface.h`): Unified transport abstraction
- **STDIO Transport** (`stdio_transport.h/c`): Standard input/output transport (✅ Implemented)
- **HTTP Transport** (`http_transport.h`): HTTP-based transport (🚧 Planned)
- **SSE Transport** (`sse_transport.h`): Server-Sent Events transport (🚧 Planned)

#### 3. Application Layer (`include/application/`)
- **MCP Server Core** (`mcp_server.h`): Main server orchestration
- **Session Manager** (`session_manager.h`): MCP session lifecycle management
- **Client Manager** (`client_manager.h`): Client connection management
- **Request Router** (`request_router.h`): Request routing and handling

#### 4. Tools Layer (`include/tools/`, `src/tools/`)
- **Tool Interface** (`tool_interface.h/c`): Tool definition and execution framework
- **Tool Registry** (`tool_registry.h`): Tool registration and discovery
- **Built-in Tools** (`builtin_tools.h`): Math, text, and utility tools

### Dependencies

- **cJSON**: Lightweight JSON parser (automatically downloaded)
- **Standard C Library**: stdio, stdlib, string, math, pthread
- **POSIX Threads**: For multi-threading support

## Extending the Server

### Adding New Tools

With the new modular architecture, adding tools is much cleaner:

1. **Create the tool implementation** using the tool interface:
   ```c
   #include "tools/tool_interface.h"

   cJSON *my_custom_tool_execute(const cJSON *parameters, void *user_data) {
       // Extract parameters
       cJSON *input = cJSON_GetObjectItem(parameters, "input");
       if (!input || !cJSON_IsString(input)) {
           return mcp_tool_create_validation_error("Missing or invalid 'input' parameter");
       }

       // Process the input
       char *result_text = process_input(input->valuestring);

       // Return success result
       return mcp_tool_create_text_result(result_text);
   }

   mcp_tool_t *create_my_custom_tool(void) {
       // Create input schema
       cJSON *properties = cJSON_CreateObject();
       cJSON *input_prop = mcp_tool_create_string_schema("Input text to process", NULL);
       cJSON_AddItemToObject(properties, "input", input_prop);

       cJSON *required = cJSON_CreateArray();
       cJSON_AddItemToArray(required, cJSON_CreateString("input"));

       cJSON *schema = mcp_tool_create_object_schema("My custom tool parameters",
                                                    properties, required);

       // Create the tool
       mcp_tool_t *tool = mcp_tool_create("my_custom_tool",
                                         "My Custom Tool",
                                         "Processes input text in a custom way",
                                         schema,
                                         my_custom_tool_execute,
                                         NULL);

       mcp_tool_set_category(tool, MCP_TOOL_CATEGORY_TEXT);

       cJSON_Delete(properties);
       cJSON_Delete(required);
       cJSON_Delete(schema);

       return tool;
   }
   ```

2. **Register the tool** with the tool registry:
   ```c
   mcp_tool_t *tool = create_my_custom_tool();
   mcp_tool_registry_register_tool(registry, tool);
   ```

### Adding New Transport Types

To add a new transport type (e.g., WebSocket):

1. **Define the transport interface** in `include/transport/websocket_transport.h`
2. **Implement the interface** in `src/transport/websocket_transport.c`
3. **Add to transport factory** in `src/transport/transport.c`
4. **Update the transport type enum** in `include/transport/transport_interface.h`

### Memory Management

The modular architecture uses careful memory management:
- **Reference counting**: Tools and other objects use reference counting
- **Automatic cleanup**: Each layer handles its own resource cleanup
- **cJSON management**: All cJSON objects are properly freed
- **Thread safety**: Proper synchronization prevents memory corruption
- **No memory leaks**: Comprehensive cleanup in all error paths

## Testing

### Basic Testing

Test the modular server directly:

```bash
# Test basic functionality
echo '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-06-18","capabilities":{},"clientInfo":{"name":"TestClient","version":"1.0.0"}}}' | ./bin/mcp_server
```

### Module Testing

Test individual modules during development:

```bash
# Test protocol module compilation
make clean && make protocol

# Test transport module compilation
make clean && make transport

# Test tools module compilation
make clean && make tools

# Test full build
make clean && make
```

### Integration Testing

Run the included test script (when available):

```bash
./test_mcp.sh
```

## Embedded Linux Considerations

The modular architecture maintains embedded-friendly characteristics:

- **Small binary size**: Modular design allows selective compilation
- **Low memory usage**: Each layer manages memory efficiently
- **Minimal dependencies**: Only cJSON and POSIX threads required
- **Signal handling**: Graceful shutdown on SIGINT/SIGTERM
- **Error resilience**: Robust error handling at each layer
- **Thread efficiency**: Optional threading with configurable thread pools
- **Resource monitoring**: Built-in resource usage tracking

## Migration from Previous Version

The server has been completely refactored with a modular architecture. Key improvements:

### Benefits of the New Architecture

- **Maintainability**: Clear separation of concerns makes code easier to understand and modify
- **Extensibility**: Adding new transports, tools, or protocol features is straightforward
- **Testability**: Each module can be tested independently
- **Reusability**: Protocol and tool layers can be reused across different transport types
- **Thread Safety**: Proper synchronization throughout the architecture
- **Performance**: More efficient resource management and optional threading

### Breaking Changes

- **File Structure**: Source files have been reorganized into module directories
- **Include Paths**: Header files are now organized by module (e.g., `protocol/mcp_protocol.h`)
- **Build System**: Updated Makefile supports modular compilation
- **API Changes**: Some function signatures have changed for better consistency

### Compatibility

- **Protocol Compatibility**: Fully compatible with MCP protocol version 2025-06-18
- **JSON-RPC Compatibility**: Full JSON-RPC 2.0 compliance maintained
- **Transport Compatibility**: STDIO transport works identically to previous version

## License

This project is provided as-is for educational and development purposes.

## Contributing

Feel free to extend this server with additional tools and capabilities. The modular design makes it easy to add new functionality while maintaining the lightweight footprint.
