# EmbedMCP - C Language MCP Server

A lightweight Model Context Protocol (MCP) server implementation in C, designed for embedded Linux systems and resource-constrained environments.

## Features

- **Lightweight**: Minimal memory footprint and dependencies
- **Embedded-friendly**: Designed for embedded Linux systems
- **Standards-compliant**: Implements MCP protocol version 2025-06-18
- **JSON-RPC 2.0**: Full JSON-RPC 2.0 support over stdio transport
- **Extensible**: Easy to add new tools and capabilities

## Current Tools

### `add` Tool
Calculates the sum of two numbers (integers or floating-point).

**Parameters:**
- `num1` (number): First number to add
- `num2` (number): Second number to add

**Returns:**
- JSON object with `num1`, `num2`, `sum`, and human-readable `message`

## Building

### Prerequisites
- GCC compiler
- Make
- curl (for downloading cJSON dependency)

### Build Steps

1. **Download dependencies:**
   ```bash
   make deps
   ```

2. **Compile the server:**
   ```bash
   make
   ```

3. **Run tests:**
   ```bash
   ./test_mcp.sh
   ```

### Build Options

- **Debug build:** `make debug` (includes debug symbols and logging)
- **Clean build:** `make clean` (removes all build artifacts)

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

### Core Components

- **Message Handler** (`message_handler.c`): JSON-RPC message parsing and sending
- **Protocol Handlers** (`protocol_handlers.c`): MCP protocol method implementations
- **Tools** (`tools.c`): Tool implementations (currently just `add`)
- **Utils** (`utils.c`): Utility functions and cleanup routines

### Dependencies

- **cJSON**: Lightweight JSON parser (automatically downloaded)
- **Standard C Library**: stdio, stdlib, string, math

## Extending the Server

### Adding New Tools

1. **Define the tool function** in `src/tools.c`:
   ```c
   int mcp_tool_your_tool(const cJSON *params, cJSON **result) {
       // Implementation here
       return 0; // Success
   }
   ```

2. **Add tool definition** in `mcp_handle_list_tools()` in `src/protocol_handlers.c`

3. **Add tool routing** in `mcp_handle_call_tool()` in `src/protocol_handlers.c`

4. **Declare the function** in `include/mcp_server.h`

### Memory Management

The server uses careful memory management with cJSON:
- All cJSON objects are properly freed
- Request/response cleanup functions handle memory deallocation
- No memory leaks in normal operation

## Testing

Run the included test script to verify functionality:

```bash
./test_mcp.sh
```

This tests:
- Server initialization
- Tool listing
- Tool execution with integers
- Tool execution with floating-point numbers

## Embedded Linux Considerations

- **Small binary size**: Typically under 100KB
- **Low memory usage**: Minimal heap allocation
- **No external dependencies**: Self-contained with bundled cJSON
- **Signal handling**: Graceful shutdown on SIGINT/SIGTERM
- **Error resilience**: Robust error handling and recovery

## License

This project is provided as-is for educational and development purposes.

## Contributing

Feel free to extend this server with additional tools and capabilities. The modular design makes it easy to add new functionality while maintaining the lightweight footprint.
