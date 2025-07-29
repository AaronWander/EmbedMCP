# EmbedMCP - Embedded MCP Server Library

A lightweight C library for creating MCP (Model Context Protocol) servers with pure business functions.

## Project Status

✅ **Tool System** - Complete implementation with pure function API
🚧 **Resource System** - Coming soon
🚧 **Prompt System** - Coming soon
🚧 **Sampling System** - Coming soon

Currently, EmbedMCP focuses on the **Tool** part of MCP protocol, allowing you to create powerful MCP servers with custom tools. Other MCP features will be added in future releases.

## Features

- **Pure Function API** - Write business logic without JSON handling
- **Universal Parameter Access** - Handle any parameter type combination
- **Automatic Schema Generation** - No manual JSON Schema required
- **Multiple Transports** - STDIO and HTTP support
- **Type Safety** - Compile-time parameter validation
- **Minimal Dependencies** - Only requires cJSON (included)
- **Extremely Simple** - Only 6 core API functions to learn
- **Easy Integration** - Copy one folder, include one header file
- **Multi-Client Ready** - Automatic concurrent client support

## Quick Integration Summary

**3 Simple Steps:**
1. Copy `embed_mcp/` folder to your project
2. Include `#include "embed_mcp/embed_mcp.h"` in your code
3. Compile all `.c` files together

**That's it!** You now have a full MCP server with multi-client support.

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
├── src/
│   └── main.c                 # Your application code
├── embed_mcp/                 # EmbedMCP library (copied)
│   ├── embed_mcp.h           # Main API header
│   ├── embed_mcp.c           # Main API implementation
│   ├── cjson/                # JSON dependency
│   │   ├── cJSON.h           # JSON parser header
│   │   └── cJSON.c           # JSON parser implementation
│   ├── protocol/             # MCP protocol implementation
│   ├── transport/            # HTTP/STDIO transport
│   ├── tools/                # Tool system
│   ├── application/          # Multi-client support
│   └── utils/                # Utilities
└── Makefile
```

### Step 2: Include Header File

In your source code, include the main header:

```c
#include "embed_mcp/embed_mcp.h"

// Your business function - no JSON handling required!
void* add_numbers(mcp_param_accessor_t* params) {
    double a = params->get_double(params, "a");
    double b = params->get_double(params, "b");

    double* result = malloc(sizeof(double));
    *result = a + b;
    return result;
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
        .debug = 0              // Debug logging (0=off, 1=on)
    };

    // Create server
    embed_mcp_server_t *server = embed_mcp_create(&config);

    // Define parameters
    mcp_param_desc_t params[] = {
        MCP_PARAM_DOUBLE_DEF("a", "First number", 1),
        MCP_PARAM_DOUBLE_DEF("b", "Second number", 1)
    };

    // Register your pure function
    embed_mcp_add_pure_function(server, "add", "Add two numbers",
                                params, 2, MCP_RETURN_DOUBLE, add_numbers);

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
gcc src/main.c \
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
gcc src/main.c libembed_mcp.a -I embed_mcp -o my_app
```

#### Option 3: Using Makefile

Create a simple Makefile:

```makefile
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -I embed_mcp
SRCDIR = embed_mcp
SOURCES = $(wildcard $(SRCDIR)/*.c $(SRCDIR)/*/*.c)
OBJECTS = $(SOURCES:.c=.o)

my_app: src/main.c $(OBJECTS)
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

**Multi-Client Support:** The library automatically handles multiple concurrent clients through the application layer, but your tool functions remain simple and don't need to worry about client management.

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
// Register pure function tool (handles all MCP scenarios)
int embed_mcp_add_pure_function(embed_mcp_server_t *server,
                                const char *name,
                                const char *description,
                                mcp_param_desc_t *params,
                                size_t param_count,
                                mcp_return_type_t return_type,
                                mcp_universal_func_t function_ptr);
```

**Function Parameters:**
- `server` - Server instance created with `embed_mcp_create()`
- `name` - Unique tool name (used in MCP protocol)
- `description` - Human-readable tool description
- `params` - Array of parameter descriptions
- `param_count` - Number of parameters in the array
- `return_type` - Return type (`MCP_RETURN_DOUBLE`, `MCP_RETURN_INT`, `MCP_RETURN_STRING`, `MCP_RETURN_VOID`)
- `function_ptr` - Pointer to your pure business function

#### Error Handling

```c
// Get last error message (returns NULL if no error)
const char *embed_mcp_get_error(void);
```

### Parameter Definition Macros

These macros simplify parameter definition:

```c
// Single value parameters
MCP_PARAM_DOUBLE_DEF(name, description, required)   // Double parameter
MCP_PARAM_INT_DEF(name, description, required)      // Integer parameter
MCP_PARAM_STRING_DEF(name, description, required)   // String parameter
MCP_PARAM_BOOL_DEF(name, description, required)     // Boolean parameter

// Array parameters
MCP_PARAM_ARRAY_DOUBLE_DEF(name, desc, elem_desc, required)  // Array of doubles
MCP_PARAM_ARRAY_STRING_DEF(name, desc, elem_desc, required)  // Array of strings
MCP_PARAM_ARRAY_INT_DEF(name, desc, elem_desc, required)     // Array of integers

// Complex object parameters
MCP_PARAM_OBJECT_DEF(name, description, json_schema, required)  // Custom JSON object
```

**Parameters:**
- `name` - Parameter name (string literal)
- `description` - Human-readable description (string literal)
- `elem_desc` - Description of array elements (string literal)
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

## Complete Examples

### 1. Basic Math Tool

```c
#include "embed_mcp.h"

// Pure business function - no JSON handling required!
void* add_numbers(mcp_param_accessor_t* params) {
    double a = params->get_double(params, "a");
    double b = params->get_double(params, "b");

    double* result = malloc(sizeof(double));
    *result = a + b;
    return result;
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

    // Define parameters
    mcp_param_desc_t params[] = {
        MCP_PARAM_DOUBLE_DEF("a", "First number to add", 1),
        MCP_PARAM_DOUBLE_DEF("b", "Second number to add", 1)
    };

    // Register tool
    if (embed_mcp_add_pure_function(server, "add", "Add two numbers together",
                                    params, 2, MCP_RETURN_DOUBLE, add_numbers) != 0) {
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
void* process_text(mcp_param_accessor_t* params) {
    const char* input = params->get_string(params, "text");
    const char* operation = params->get_string(params, "operation");

    size_t len = strlen(input);
    char* result = malloc(len + 1);

    if (strcmp(operation, "upper") == 0) {
        for (size_t i = 0; i < len; i++) {
            result[i] = toupper(input[i]);
        }
    } else if (strcmp(operation, "lower") == 0) {
        for (size_t i = 0; i < len; i++) {
            result[i] = tolower(input[i]);
        }
    } else {
        strcpy(result, input);  // No change
    }
    result[len] = '\0';

    return result;
}

// Register the tool
mcp_param_desc_t text_params[] = {
    MCP_PARAM_STRING_DEF("text", "Input text to process", 1),
    MCP_PARAM_STRING_DEF("operation", "Operation: 'upper' or 'lower'", 1)
};
embed_mcp_add_pure_function(server, "process_text", "Process text with various operations",
                            text_params, 2, MCP_RETURN_STRING, process_text);
```

### 3. Array Processing Tool

```c
void* sum_array(mcp_param_accessor_t* params) {
    size_t count;
    double* numbers = params->get_double_array(params, "numbers", &count);

    double total = 0.0;
    for (size_t i = 0; i < count; i++) {
        total += numbers[i];
    }

    double* result = malloc(sizeof(double));
    *result = total;
    return result;
}

// Register the tool
mcp_param_desc_t array_params[] = {
    MCP_PARAM_ARRAY_DOUBLE_DEF("numbers", "Array of numbers to sum", "A number", 1)
};
embed_mcp_add_pure_function(server, "sum_array", "Calculate sum of number array",
                            array_params, 1, MCP_RETURN_DOUBLE, sum_array);
```

### 4. Complex Parameters (Direct JSON Access)

```c
void* complex_handler(mcp_param_accessor_t* params) {
    // Use type-safe accessors for simple parameters
    const char* operation = params->get_string(params, "operation");

    // Use direct JSON access for complex nested structures
    const cJSON* config = params->get_json(params, "config");
    if (config) {
        cJSON* database = cJSON_GetObjectItem(config, "database");
        if (database) {
            cJSON* host = cJSON_GetObjectItem(database, "host");
            cJSON* port = cJSON_GetObjectItem(database, "port");

            printf("Connecting to %s:%d\n",
                   cJSON_GetStringValue(host),
                   cJSON_GetNumberValue(port));
        }
    }

    char* result = malloc(256);
    snprintf(result, 256, "Processed %s operation", operation);
    return result;
}
```

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

The example server includes three demo tools:
- `add(a, b)` - Add two numbers
- `sum_array(numbers[])` - Sum an array of numbers
- `weather(city)` - Get weather info (supports Jinan/济南)

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
EmbedMCP automatically supports multiple concurrent clients. Each client gets its own session, and tool calls are properly isolated. You don't need to worry about client management in your tool functions.

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

- ✅ **v1.0** - Tool system with pure function API
- 🚧 **v1.1** - Resource system (file access, data sources)
- 🚧 **v1.2** - Prompt system (prompt templates, completion)
- 🚧 **v1.3** - Sampling system (LLM sampling control)
- 🚧 **v2.0** - Advanced features (logging, metrics, auth)

## Contributing

We welcome contributions! Please see CONTRIBUTING.md for guidelines.

## License

MIT License - see LICENSE file for details.

---

**EmbedMCP** - Making MCP servers as simple as writing pure functions! 🚀
