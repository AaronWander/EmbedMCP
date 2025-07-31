<p align="center">
  <a href="./README.md"><img alt="README in English" src="https://img.shields.io/badge/English-d9d9d9"></a>
  <a href="./README_zh.md"><img alt="简体中文版自述文件" src="https://img.shields.io/badge/简体中文-d9d9d9"></a>
</p>

# EmbedMCP - Embedded MCP Server Library

A lightweight C library for creating MCP (Model Context Protocol) servers with pure business functions.

## Project Status

✅ **Tool System** - Complete implementation with flexible function API
✅ **Multi-Session Support** - Concurrent connections with session management
✅ **Library Validation** - Successfully dogfooded (we use our own library!)
✅ **HTTP/STDIO Transport** - Full MCP protocol support
✅ **MCP Protocol Compliance** - Proper content array format, tested with MCP Inspector
✅ **Production Ready** - Clean codebase, comprehensive testing, thread-safe
🚧 **Resource System** - Coming soon
🚧 **Prompt System** - Coming soon
🚧 **Sampling System** - Coming soon

Currently, EmbedMCP focuses on the **Tool** part of MCP protocol, allowing you to create powerful MCP servers with custom tools and support multiple concurrent clients. The library has been thoroughly tested by building our own example server with it and validated with MCP Inspector!

## Features

- **Extremely Simple** - Register C functions as MCP tools with just 1 API function
- **Multi-Session Support** - Handle multiple concurrent clients with session management
- **Easy Integration** - Copy one folder, include one header file
- **Multiple Transports** - HTTP and STDIO support
- **Thread-Safe** - Concurrent connections with proper synchronization
- **Production Ready** - MCP Inspector compatible, battle-tested

## Quick Start

1. Copy `embed_mcp/` folder to your project
2. Include `#include "embed_mcp/embed_mcp.h"`
3. Compile all `.c` files together

Done! You have a working MCP server.

## Integration Guide

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
│   ├── cjson/                # JSON dependency
│   ├── protocol/             # MCP protocol implementation
│   ├── transport/            # HTTP/STDIO transport
│   ├── tools/                # Tool system
│   └── utils/                # Utilities
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

#### Option 1: Simple Compilation (All Source Files)

```bash
# Compile all source files together
gcc main.c \
    embed_mcp/embed_mcp.c \
    embed_mcp/cjson/cJSON.c \
    embed_mcp/protocol/*.c \
    embed_mcp/transport/*.c \
    embed_mcp/tools/*.c \
    embed_mcp/utils/*.c \
    -I embed_mcp \
    -o my_app
```

#### Option 2: Create Static Library First

```bash
# Create object files
gcc -c embed_mcp/embed_mcp.c -I embed_mcp -o embed_mcp.o
gcc -c embed_mcp/cjson/cJSON.c -I embed_mcp -o cJSON.o
gcc -c embed_mcp/protocol/*.c -I embed_mcp
gcc -c embed_mcp/transport/*.c -I embed_mcp
gcc -c embed_mcp/tools/*.c -I embed_mcp
gcc -c embed_mcp/utils/*.c -I embed_mcp

# Create static library
ar rcs libembed_mcp.a *.o

# Compile your application
gcc main.c libembed_mcp.a -I embed_mcp -o my_app
```

#### Option 3: Using Makefile

Create a simple Makefile:

```makefile
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -I embed_mcp
SRCDIR = embed_mcp
SOURCES = $(wildcard $(SRCDIR)/*.c $(SRCDIR)/*/*.c)
OBJECTS = $(SOURCES:.c=.o)

my_app: main.c $(OBJECTS)
	$(CC) $(CFLAGS) $^ -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) my_app

.PHONY: clean
```

Then simply run:
```bash
make
```

## Library Files Overview

The `embed_mcp/` folder contains all the files needed to integrate EmbedMCP into your project:

### Core Files
- **`embed_mcp.h`** - Main API header file (this is what you include)
- **`embed_mcp.c`** - Main API implementation
- **`cjson/`** - JSON parsing dependency (bundled)
  - `cJSON.h` - JSON parser header
  - `cJSON.c` - JSON parser implementation

### Internal Implementation
- **`protocol/`** - MCP protocol implementation
  - `mcp_protocol.h/.c` - Core protocol handling
  - `message.h/.c` - Message parsing and formatting
  - `jsonrpc.h/.c` - JSON-RPC implementation
  - `protocol_state.h/.c` - Protocol state management

- **`transport/`** - Transport layer (HTTP/STDIO)
  - `transport_interface.h` - Transport abstraction
  - `http_transport.h/.c` - HTTP server implementation
  - `stdio_transport.h/.c` - STDIO transport for MCP clients
  - `sse_transport.h` - Server-Sent Events support

- **`tools/`** - Tool system
  - `tool_interface.h/.c` - Tool interface and execution
  - `tool_registry.h/.c` - Tool registration and management

- **`application/`** - Multi-client support
  - `client_manager.h` - Multiple client connection management
  - `session_manager.h` - Session isolation and management
  - `request_router.h` - Request routing to correct sessions
  - `mcp_server.h` - High-level server application layer

- **`utils/`** - Utilities
  - `logging.h/.c` - Logging system
  - `memory.h/.c` - Memory management utilities

### What You Need to Know

**For Users:** You only need to know about `embed_mcp.h` - that's your entire API!

**For Integration:** Copy the entire `embed_mcp/` folder and compile all `.c` files together.

**Single-Client Design:** The library is currently optimized for single-client scenarios. Multi-client support is planned for future releases.

## Core Data Structures

### Server Configuration (`embed_mcp_config_t`)

```c
typedef struct {
    const char *name;           // Server name (displayed in MCP protocol)
    const char *version;        // Server version (displayed in MCP protocol)
    const char *host;           // HTTP bind address (default: "0.0.0.0")
    int port;                   // HTTP port number (default: 8080)
    const char *path;           // HTTP endpoint path (default: "/mcp")
    int max_tools;              // Maximum number of tools allowed (default: 100)
    int debug;                  // Enable debug logging (0=off, 1=on, default: 0)
} embed_mcp_config_t;
```

**Configuration Fields:**

| Field | Type | Description | Typical Value |
|-------|------|-------------|---------------|
| `name` | `const char*` | Server name (displayed in MCP protocol) | `"MyApp"` |
| `version` | `const char*` | Server version (displayed in MCP protocol) | `"1.0.0"` |
| `host` | `const char*` | HTTP bind address | `"0.0.0.0"` |
| `port` | `int` | HTTP port number | `8080` |
| `path` | `const char*` | HTTP endpoint path | `"/mcp"` |
| `max_tools` | `int` | Maximum number of tools allowed | `100` |
| `debug` | `int` | Enable debug logging (0=off, 1=on) | `0` |

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

### Parameter Accessor (`mcp_param_accessor_t`)

The parameter accessor provides type-safe access to tool parameters:

```c
struct mcp_param_accessor {
    // Type-safe getters for basic types
    int64_t (*get_int)(mcp_param_accessor_t* self, const char* name);
    double (*get_double)(mcp_param_accessor_t* self, const char* name);
    const char* (*get_string)(mcp_param_accessor_t* self, const char* name);
    int (*get_bool)(mcp_param_accessor_t* self, const char* name);

    // Array getters for common MCP patterns
    double* (*get_double_array)(mcp_param_accessor_t* self, const char* name, size_t* count);
    char** (*get_string_array)(mcp_param_accessor_t* self, const char* name, size_t* count);
    int64_t* (*get_int_array)(mcp_param_accessor_t* self, const char* name, size_t* count);

    // Utility functions
    int (*has_param)(mcp_param_accessor_t* self, const char* name);
    size_t (*get_param_count)(mcp_param_accessor_t* self);

    // For rare complex cases: direct JSON access
    const cJSON* (*get_json)(mcp_param_accessor_t* self, const char* name);
};
```

## API Reference

### Core Functions (Only 6 functions!)

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

## Complete Examples

### 1. Basic Math Tool

```c
#include "embed_mcp.h"

// Simple C function - no JSON handling required!
double add_numbers(double a, double b) {
    return a + b;
}

int main() {
    // Create server configuration
    embed_mcp_config_t config = {
        .name = "MathServer",
        .version = "1.0.0",
        .host = "0.0.0.0",
        .port = 8080,
        .path = "/mcp",
        .max_tools = 100,
        .debug = 1
    };

    // Create server
    embed_mcp_server_t *server = embed_mcp_create(&config);
    if (!server) {
        printf("Error: %s\n", embed_mcp_get_error());
        return -1;
    }

    // Register tool with parameter names and types
    const char* param_names[] = {"a", "b"};
    mcp_param_type_t param_types[] = {MCP_PARAM_DOUBLE, MCP_PARAM_DOUBLE};

    if (embed_mcp_add_tool(server, "add", "Add two numbers together",
                           param_names, param_types, 2,
                           MCP_RETURN_DOUBLE, add_numbers) != 0) {
        printf("Error: %s\n", embed_mcp_get_error());
        embed_mcp_destroy(server);
        return -1;
    }

    // Run server
    printf("Starting MCP server on http://localhost:8080/mcp\n");
    int result = embed_mcp_run(server, EMBED_MCP_TRANSPORT_HTTP);

    // Cleanup
    embed_mcp_destroy(server);
    return result;
}
```

### 2. String Processing Tool

```c
char* process_text(const char* text, const char* operation) {
    size_t len = strlen(text);
    char* result = malloc(len + 1);

    if (strcmp(operation, "upper") == 0) {
        for (size_t i = 0; i < len; i++) {
            result[i] = toupper(text[i]);
        }
    } else if (strcmp(operation, "lower") == 0) {
        for (size_t i = 0; i < len; i++) {
            result[i] = tolower(text[i]);
        }
    } else {
        strcpy(result, text);  // No change
    }
    result[len] = '\0';

    return result;
}

// Register the tool
const char* text_param_names[] = {"text", "operation"};
mcp_param_type_t text_param_types[] = {MCP_PARAM_STRING, MCP_PARAM_STRING};

embed_mcp_add_tool(server, "process_text", "Process text with various operations",
                   text_param_names, text_param_types, 2,
                   MCP_RETURN_STRING, process_text);
```

### 3. Multi-Parameter Tool

```c
int calculate_score(int base_points, char grade, double multiplier) {
    int bonus = 0;
    switch (grade) {
        case 'A': bonus = 100; break;
        case 'B': bonus = 80; break;
        case 'C': bonus = 60; break;
        default: bonus = 0; break;
    }

    return (int)((base_points + bonus) * multiplier);
}

// Register the tool
const char* score_param_names[] = {"base_points", "grade", "multiplier"};
mcp_param_type_t score_param_types[] = {MCP_PARAM_INT, MCP_PARAM_CHAR, MCP_PARAM_DOUBLE};

embed_mcp_add_tool(server, "calculate_score", "Calculate score with grade bonus",
                   score_param_names, score_param_types, 3,
                   MCP_RETURN_INT, calculate_score);
```

### 4. Running the Server

```bash
# Build the server (uses embed_mcp/ library - dogfooding!)
make

# Run with HTTP transport
./bin/mcp_server -t http -p 8080

# Run with STDIO transport (for MCP clients)
./bin/mcp_server -t stdio

# Run with debug logging
./bin/mcp_server -t http -p 8080 -d
```

**Note:** Our example server is built using the `embed_mcp/` library itself, proving that the library works correctly!

## Dogfooding - We Use Our Own Library!

We practice "dogfooding" - our example server uses the `embed_mcp/` library itself. This proves the library works, is easy to integrate, and follows its own documentation.

**Proof:** Our `Makefile` compiles `examples/main.c` against the `embed_mcp/` library!

## Testing & Validation

✅ **MCP Inspector Compatible** - Passes all protocol compliance tests
✅ **Multi-Session Tested** - Supports concurrent connections with session isolation
✅ **Production Tested** - HTTP/STDIO transports, multiple parameter types
✅ **Real-World Validated** - We use our own library (dogfooding)

### Multi-Session Testing

Test concurrent connections with multiple MCP Inspector instances:

```bash
# Start the server
./bin/mcp_server -t http -p 8080 -d

# Start multiple MCP Inspector instances
npx @modelcontextprotocol/inspector --config config1.json
PORT=6278 npx @modelcontextprotocol/inspector  # Different port

# Connect both to: http://localhost:8080/mcp
```

Each connection creates an independent session with:
- Unique session ID
- Independent session state
- Automatic timeout and cleanup
- Thread-safe concurrent access

## Building and Running

### Build the Example (Development)

For development and testing, you can build the included example:

```bash
make debug    # Debug build with symbols
make release  # Optimized release build
make clean    # Clean build files
```

### Run the Example Server

```bash
# Build and run the example server
make debug
./bin/mcp_server -t http -p 8080

# Or with STDIO transport
./bin/mcp_server -t stdio
```

The example server includes three demo tools (registered using `embed_mcp_add_tool`):
- `add(a, b)` - Add two numbers (demonstrates basic math)
- `weather(city)` - Get weather info (demonstrates string processing, supports Jinan/济南)
- `calculate_score(base_points, grade, multiplier)` - Calculate score with grade bonus (demonstrates mixed parameter types)

### Test with MCP Inspector

1. Install MCP Inspector: `npm install -g @modelcontextprotocol/inspector`
2. Run your server: `./bin/mcp_server -t http -p 8080`
3. Open MCP Inspector: `mcp-inspector`
4. Connect to: `http://localhost:8080/mcp`

### Test with curl

```bash
# List available tools
curl -X POST http://localhost:8080/mcp \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":1,"method":"tools/list","params":{}}'

# Call the add tool
curl -X POST http://localhost:8080/mcp \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"add","arguments":{"a":10,"b":5}}}'

# Call the weather tool
curl -X POST http://localhost:8080/mcp \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"weather","arguments":{"city":"济南"}}}'
```

## Integration Guide

## Important Notes

### Multi-Client Support
**Current Status:** EmbedMCP currently supports single-client scenarios. Multi-client support is planned for future releases.

**Current Limitations:**
- Designed for single-client or sequential client access
- Concurrent clients may interfere with each other
- No session isolation between clients

**Workarounds:**
- Use a reverse proxy/load balancer for multiple clients
- Run multiple EmbedMCP server instances
- Ensure only one client connects at a time

### Thread Safety
The library handles concurrent requests safely. Your tool functions should be stateless or use proper synchronization if they access shared resources.

### Memory Management
- **Tool Parameters:** Automatically managed by the library
- **Return Values:** Your functions should return malloc'd memory for strings and complex types
- **Simple Types:** Return by value (double, int) or malloc'd pointer

### Error Handling
Always check return values and use `embed_mcp_get_error()` for detailed error information:

```c
if (embed_mcp_add_pure_function(...) != 0) {
    printf("Error: %s\n", embed_mcp_get_error());
    // Handle error appropriately
}
```

### Transport Types
- **HTTP Transport:** Best for web integration, supports multiple concurrent clients
- **STDIO Transport:** Best for MCP client integration (Claude Desktop, etc.)

### Performance Tips
- Keep tool functions lightweight and fast
- Use appropriate parameter types (avoid complex nested objects when possible)
- Consider caching expensive computations

### Memory Management

- **Return values:** Your pure functions should return malloc'd memory
- **Parameters:** Parameter accessor handles memory automatically
- **Strings:** String returns are freed by the library after sending response
- **Arrays:** Array parameters are managed by the library

### Error Handling

Always check return values and use `embed_mcp_get_error()` for diagnostics:

```c
if (embed_mcp_add_pure_function(...) != 0) {
    printf("Error: %s\n", embed_mcp_get_error());
    // Handle error
}
```

## Roadmap

- ✅ **v1.0** - Tool system, MCP Inspector compatible, production ready
- ✅ **v1.1** - Multi-session support, concurrent connections, session management
- 🚧 **v1.2** - RTOS/Embedded Linux platform abstraction layer (HAL)
- 🚧 **v1.3** - Resource system (file access, data sources)
- 🚧 **v1.4** - Prompt system (templates, completion)
- 🚧 **v2.0** - Advanced features (logging, metrics, auth)

## Contributing

We welcome contributions! Please see CONTRIBUTING.md for guidelines.

## License

MIT License - see LICENSE file for details.

---

**EmbedMCP** - Making MCP servers as simple as writing pure functions! 🚀
