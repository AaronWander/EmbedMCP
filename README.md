<p align="center">
  <a href="./README.md"><img alt="README in English" src="https://img.shields.io/badge/English-d9d9d9"></a>
  <a href="./README_zh.md"><img alt="简体中文版自述文件" src="https://img.shields.io/badge/简体中文-d9d9d9"></a>
</p>

# EmbedMCP - Embedded MCP Server Library

A lightweight C library for creating MCP (Model Context Protocol) servers with pure business functions.

## Project Status

✅ **Tool System** - Complete implementation with flexible function API
✅ **Multi-Session Support** - Concurrent connections with session management
✅ **HTTP/STDIO Transport** - Full MCP protocol support
🚧 **Prompt System** - Coming soon
🚧 **Sampling System** - Coming soon

## Features

- **Cross-Platform** - Same code runs on 15+ platforms via Universal HAL
- **Multi-Session Support** - Handle multiple concurrent clients with session management
- **Easy Integration** - Copy one folder, include one header file
- **Multiple Transports** - HTTP and STDIO support

### Write Once, Run Everywhere
```c
// This exact code runs on ALL platforms
double add_numbers(double a, double b) {
    return a + b;  // Pure business logic
}

int main() {
    embed_mcp_config_t config = {
        .name = "MyApp",
        .version = "1.0.0",
        .instructions = "Simple math server. Use 'add' tool to add two numbers.",
        .port = 8080
    };

    embed_mcp_server_t *server = embed_mcp_create(&config);

    // Register the add function
    const char* param_names[] = {"a", "b"};
    mcp_param_type_t param_types[] = {MCP_PARAM_DOUBLE, MCP_PARAM_DOUBLE};
    embed_mcp_add_tool(server, "add", "Add numbers", param_names, param_types, 2, MCP_RETURN_DOUBLE, add_numbers);

    embed_mcp_run(server, EMBED_MCP_TRANSPORT_HTTP);  // Works on Linux, RTOS, etc.
    embed_mcp_destroy(server);
    return 0;
}
```

## Quick Start

1. Copy `embed_mcp/` folder to your project
2. Include `#include "embed_mcp/embed_mcp.h"`
3. Compile all `.c` files together

Done! You have a working MCP server.

**💡 Tip:** Check out the `examples/` folder for complete working examples!

## MCP Protocol Compliance

EmbedMCP is fully compliant with the official MCP specification and works seamlessly with all MCP clients including Dify, MCP Inspector, and any application using the official MCP SDKs.

### Dynamic Capabilities

EmbedMCP automatically generates server capabilities based on what you actually implement:

```json
{
  "capabilities": {
    "tools": {"listChanged": true},     // Only appears when you register tools
    "resources": {"listChanged": true}, // Only appears when you add resources
    "prompts": {"listChanged": true},   // Only appears when you add prompts
    "logging": {}                       // Always available for debugging
  }
}
```

**Key Benefits:**
- ✅ **No false advertising** - Only advertise features you actually support
- ✅ **Client compatibility** - Clients know exactly what your server can do
- ✅ **Automatic updates** - Capabilities update as you add/remove features

### MCP Server Configuration Example
Configure helpful instructions that appear in MCP clients:

```c
embed_mcp_config_t config = {
    .name = "WeatherServer",
    .version = "1.0.0",
    .instructions = "Weather information server. Use 'get_weather(city)' to get current weather for any city.",
    // ... other config
};
```

### MCP Server Session Configuration Example
```c
embed_mcp_config_t config = {
    .max_connections = 10,    // Up to 10 concurrent clients
    .session_timeout = 3600,  // 1 hour session timeout
    .enable_sessions = 1,     // Enable session management
    .auto_cleanup = 1         // Auto cleanup expired sessions
};
```

These instructions help users understand how to use your server and appear in MCP Inspector, Dify, and other clients.

## Integration Guide

**💡 Quick Start:** See complete examples in the `examples/` folder!

### Step 1: Copy Library Files

Copy the `embed_mcp/` folder to your project:

```bash
# Copy the entire embed_mcp folder to your project
cp -r /path/to/EmbedMCP/embed_mcp/ your_project/
```

Your project structure will look like:
```
your_project/
├── main.c                     # Your application code
├── embed_mcp/                 # EmbedMCP library (copied)
│   ├── embed_mcp.h           # Main API header
│   ├── embed_mcp.c           # Main API implementation
│   ├── Makefile.inc          # Makefile configuration
│   ├── application/          # Session management & multi-client support
│   ├── cjson/                # JSON dependency
│   ├── hal/                  # Hardware abstraction layer
│   ├── platform/             # Platform-specific implementations
│   ├── protocol/             # MCP protocol implementation
│   ├── tools/                # Tool system
│   ├── transport/            # HTTP/STDIO transport
│   └── utils/                # Utilities (logging, UUID, base64, etc.)
└── Makefile
```

### Step 2: Include Header File

In your source code, include the main header:

```c
#include "embed_mcp.h"

// Your business function - no JSON handling required!
double add_numbers(double a, double b) {
    return a + b;
}

int main() {
    // Create server configuration
    embed_mcp_config_t config = {
        .name = "MyApp",
        .version = "1.0.0",
        .instructions = "Mathematical tools server. Use 'add' to add two numbers together.",
        .host = "0.0.0.0",      // HTTP bind address
        .port = 8080,           // HTTP port
        .path = "/mcp",         // HTTP endpoint path
        .max_tools = 100,       // Maximum number of tools
        .debug = 0,             // Debug logging (0=off, 1=on)

        // Multi-session configuration
        .max_connections = 10,  // Max concurrent connections
        .session_timeout = 3600,// Session timeout (seconds)
        .enable_sessions = 1,   // Enable session management
        .auto_cleanup = 1       // Auto cleanup expired sessions
    };

    // Create server
    embed_mcp_server_t *server = embed_mcp_create(&config);

    // Register your function with parameter names and types
    const char* param_names[] = {"a", "b"};
    mcp_param_type_t param_types[] = {MCP_PARAM_DOUBLE, MCP_PARAM_DOUBLE};

    embed_mcp_add_tool(server, "add", "Add two numbers",
                       param_names, param_types, 2,
                       MCP_RETURN_DOUBLE, add_numbers);

    // Run server
    embed_mcp_run(server, EMBED_MCP_TRANSPORT_HTTP);

    // Cleanup
    embed_mcp_destroy(server);
    return 0;
}
```

### Step 3: Compile Your Project

**Method 1: Using provided Makefile configuration**
```makefile
# Include in your Makefile
include embed_mcp/Makefile.inc

my_app: main.c $(EMBED_MCP_SOURCES)
	$(CC) $(EMBED_MCP_INCLUDES) main.c $(EMBED_MCP_SOURCES) $(EMBED_MCP_LIBS) -o my_app
```

**Method 2: Direct compilation**
```bash
gcc main.c embed_mcp/*.c embed_mcp/*/*.c embed_mcp/cjson/*.c \
    -Iembed_mcp -lpthread -lm -o my_app
```

## Core Data Structures

### Server Configuration (`embed_mcp_config_t`)

```c
typedef struct {
    const char *name;           // Server name (displayed in MCP protocol)
    const char *version;        // Server version (displayed in MCP protocol)
    const char *instructions;   // Server usage instructions (optional, displayed in MCP protocol)
    const char *host;           // HTTP bind address (default: "0.0.0.0")
    int port;                   // HTTP port number (default: 8080)
    const char *path;           // HTTP endpoint path (default: "/mcp")
    int max_tools;              // Maximum number of tools allowed (default: 100)
    int debug;                  // Enable debug logging (0=off, 1=on, default: 0)

    // Multi-session support
    int max_connections;        // Maximum concurrent connections (default: 10)
    int session_timeout;        // Session timeout in seconds (default: 3600)
    int enable_sessions;        // Enable session management (0=off, 1=on, default: 1)
    int auto_cleanup;           // Auto cleanup expired sessions (0=off, 1=on, default: 1)
} embed_mcp_config_t;
```

**Configuration Fields:**

| Field | Type | Description | Typical Value |
|-------|------|-------------|---------------|
| `name` | `const char*` | Server name (displayed in MCP protocol) | `"MyApp"` |
| `version` | `const char*` | Server version (displayed in MCP protocol) | `"1.0.0"` |
| `instructions` | `const char*` | Server usage instructions (optional) | `"Use 'add' to add numbers"` |
| `host` | `const char*` | HTTP bind address | `"0.0.0.0"` |
| `port` | `int` | HTTP port number | `8080` |
| `path` | `const char*` | HTTP endpoint path | `"/mcp"` |
| `max_tools` | `int` | Maximum number of tools allowed | `100` |
| `debug` | `int` | Enable debug logging (0=off, 1=on) | `0` |
| `max_connections` | `int` | Maximum concurrent connections | `10` |
| `session_timeout` | `int` | Session timeout in seconds | `3600` |
| `enable_sessions` | `int` | Enable session management (0=off, 1=on) | `1` |
| `auto_cleanup` | `int` | Auto cleanup expired sessions (0=off, 1=on) | `1` |

### Parameter Description (`mcp_param_desc_t`)

```c
typedef struct {
    const char *name;                   // Parameter name (used in JSON)
    const char *description;            // Human-readable parameter description
    mcp_param_category_t category;      // Parameter category (single/array/object)
    int required;                       // 1 if required, 0 if optional

    union {
        mcp_param_type_t single_type;   // For single-value parameters
        mcp_array_desc_t array_desc;    // For array parameters
        const char *object_schema;      // JSON Schema string for complex objects
    };
} mcp_param_desc_t;
```



## API Reference

### Core Functions

#### Server Management

```c
// Create MCP server instance
embed_mcp_server_t *embed_mcp_create(const embed_mcp_config_t *config);

// Destroy server instance and free resources
void embed_mcp_destroy(embed_mcp_server_t *server);

// Stop running server (can be called from signal handler)
void embed_mcp_stop(embed_mcp_server_t *server);
```

#### Server Execution

```c
// Run server with specified transport
// transport: EMBED_MCP_TRANSPORT_STDIO or EMBED_MCP_TRANSPORT_HTTP
// This function blocks until server is stopped
int embed_mcp_run(embed_mcp_server_t *server, embed_mcp_transport_t transport);
```

#### Tool Registration

```c
// Register tool function with flexible parameter specification
int embed_mcp_add_tool(embed_mcp_server_t *server,
                       const char *name,
                       const char *description,
                       const char *param_names[],
                       mcp_param_type_t param_types[],
                       size_t param_count,
                       mcp_return_type_t return_type,
                       void *function_ptr);
```

**Function Parameters:**
- `server` - Server instance created with `embed_mcp_create()`
- `name` - Unique tool name (used in MCP protocol)
- `description` - Human-readable tool description
- `param_names` - Array of parameter names
- `param_types` - Array of parameter types
- `param_count` - Number of parameters
- `return_type` - Return type (`MCP_RETURN_DOUBLE`, `MCP_RETURN_INT`, `MCP_RETURN_STRING`, `MCP_RETURN_VOID`)
- `function_ptr` - Pointer to your C function

#### Error Handling

```c
// Get last error message (returns NULL if no error)
const char *embed_mcp_get_error(void);
```

### Parameter Types

Use these parameter types when registering tools:

```c
typedef enum {
    MCP_PARAM_INT,        // Integer parameter
    MCP_PARAM_DOUBLE,     // Double parameter
    MCP_PARAM_STRING,     // String parameter
    MCP_PARAM_CHAR        // Character parameter
} mcp_param_type_t;
```

**Example Usage:**
```c
// For function: int add(int a, int b)
const char* param_names[] = {"a", "b"};
mcp_param_type_t param_types[] = {MCP_PARAM_INT, MCP_PARAM_INT};

embed_mcp_add_tool(server, "add", "Add two integers",
                   param_names, param_types, 2, MCP_RETURN_INT, add_function);
```

### Parameter Definition Macros

These macros simplify parameter definition:

```c
// Single-value parameters
MCP_PARAM_DOUBLE_DEF(name, description, required)   // Double parameter
MCP_PARAM_INT_DEF(name, description, required)      // Integer parameter
MCP_PARAM_STRING_DEF(name, description, required)   // String parameter
MCP_PARAM_BOOL_DEF(name, description, required)     // Boolean parameter

// Array parameters
MCP_PARAM_ARRAY_DOUBLE_DEF(name, desc, elem_desc, required)  // Double array
MCP_PARAM_ARRAY_STRING_DEF(name, desc, elem_desc, required)  // String array
MCP_PARAM_ARRAY_INT_DEF(name, desc, elem_desc, required)     // Integer array

// Complex object parameters
MCP_PARAM_OBJECT_DEF(name, description, json_schema, required)  // Custom JSON object
```

**Parameters:**
- `name` - Parameter name (string literal)
- `description` - Human-readable description (string literal)
- `elem_desc` - Array element description (string literal)
- `json_schema` - JSON Schema string for object validation
- `required` - 1 if required, 0 if optional

### Return Types

```c
typedef enum {
    MCP_RETURN_DOUBLE,    // Return double value
    MCP_RETURN_INT,       // Return integer value
    MCP_RETURN_STRING,    // Return string value (caller must free)
    MCP_RETURN_VOID       // No return value
} mcp_return_type_t;
```

### Transport Types

```c
typedef enum {
    EMBED_MCP_TRANSPORT_STDIO,    // Standard input/output transport
    EMBED_MCP_TRANSPORT_HTTP      // HTTP transport
} embed_mcp_transport_t;
```

### Build Example (Development)

For development and testing, you can build the included example:

```bash
make debug    # Debug build with symbols
make clean    # Clean build files
```

### Run Example Server

```bash
# Build and run example server (using embed_mcp/ library - self-validation!)
make
./bin/mcp_server -t http -p 8080

# Or use STDIO transport
./bin/mcp_server -t stdio

# Enable debug logging
./bin/mcp_server -t http -p 8080 -d
```

Example server includes demo tools and resources:

**Tools** (registered using `embed_mcp_add_tool`):
- `add(a, b)` - Add two numbers (demonstrates basic math operations)
- `weather(city)` - Get weather information (demonstrates string processing, supports Jinan/济南)
- `calculate_score(base_points, grade, multiplier)` - Calculate score with grade bonus (demonstrates mixed parameter types)

**Resources** (demonstrates resource system):
- `config://readme` - Project README (static text resource)
- `status://system` - System status (dynamic JSON resource)
- `config://server` - Server configuration (dynamic JSON resource)
- `file://example.txt` - Example text file (file resource)
- `file:///./{path}` - Project files template (file resource template)
- `file:///./examples/{path}` - Examples template (file resource template)

### Test with MCP Inspector

1. Open MCP Inspector: visit https://inspector.mcp.dev
2. Run your server: `./bin/mcp_server -t http -p 8080`
3. Connect in MCP Inspector
4. Connect to: `http://localhost:8080/mcp`



## Important Notes

### Error Handling
Always check return values and use `embed_mcp_get_error()` for detailed error information

### Transport Types
- **HTTP Transport:** Best for web integration, supports multiple concurrent clients
- **STDIO Transport:** Best for MCP client integration (Claude Desktop, etc.)

### Thread Safety
The library handles concurrent requests safely. Your tool functions should be stateless or use proper synchronization if they access shared resources.

### Memory Management
- **Tool Parameters:** Automatically managed by the library
- **Return Values:** Your functions should return malloc'd memory for strings and complex types
- **Simple Types:** Return by value (double, int) or malloc'd pointer



## Contributing

We welcome contributions! Please see CONTRIBUTING.md for guidelines.

## License

MIT License - see LICENSE file for details.

---

**EmbedMCP** - Connecting Embedded Devices to Intelligence
