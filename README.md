 # EmbedMCP - Embedded MCP Server Library

A lightweight C library for creating MCP (Model Context Protocol) servers that transforms your existing C functions into AI-accessible tools with minimal code changes.

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/MIT)
[![C Standard](https://img.shields.io/badge/C-99-blue.svg)](https://en.wikipedia.org/wiki/C99)
[![Platform](https://img.shields.io/badge/Platform-Cross--Platform-green.svg)](#platform-support)
[![MCP](https://img.shields.io/badge/MCP-2025--11--25-orange.svg)](https://modelcontextprotocol.io/)

[English](./README.md) • [简体中文](./README_zh.md)

## Why EmbedMCP?

EmbedMCP bridges the gap between your existing C codebase and modern AI systems. Instead of rewriting your battle-tested C functions, EmbedMCP lets you expose them to AI models through the standardized Model Context Protocol (MCP) with minimal code changes.

## Key Features

- **🚀 Simple Integration**: Copy one folder, include one header file
- **⚡ High Performance**: Direct C function calls with minimal overhead
- **🔧 Cross-Platform**: Runs on 15+ platforms via Universal HAL
- **📦 Zero Dependencies**: Self-contained library with no external requirements
- **🎯 Two Registration Methods**: Magic macros for simple functions, full control for complex ones
- **🌐 Multiple Transports**: Streamable HTTP and STDIO support for different use cases
- **🧠 Smart Memory Management**: Automatic cleanup with clear ownership rules
- **📊 Array Support**: Handle both simple parameters and complex data structures

## Quick Start

### Installation

1. **Download EmbedMCP**
   ```bash
   git clone https://github.com/AaronWander/EmbedMCP.git
   cd EmbedMCP
   ```

2. **Copy to your project**
   ```bash
   cp -r embed_mcp/ your_project/
   ```

### Basic Usage

```c
#include "embed_mcp/embed_mcp.h"

// Your business function
double add_numbers(double a, double b) {
    return a + b;
}

// Generate wrapper with macro
EMBED_MCP_WRAPPER(add_wrapper, add_numbers, DOUBLE, DOUBLE, a, DOUBLE, b)

int main() {
    embed_mcp_config_t config = {
        .name = "MathServer",
        .version = "1.0.0",
        .instructions = "Simple math operations server",
        .port = 8080
    };

    embed_mcp_server_t *server = embed_mcp_create(&config);

    // Register function
    const char* names[] = {"a", "b"};
    const char* descs[] = {"First number", "Second number"};
    mcp_param_type_t types[] = {MCP_PARAM_DOUBLE, MCP_PARAM_DOUBLE};

    embed_mcp_add_tool(server, "add", "Add two numbers",
                       names, descs, types, 2, MCP_RETURN_DOUBLE, add_wrapper, NULL);

    embed_mcp_run(server, EMBED_MCP_TRANSPORT_HTTP);
    embed_mcp_destroy(server);
    return 0;
}
```

### Build and Run

```bash
# Build
make

# Run Streamable HTTP server
./bin/mcp_server --transport http --port 8080

# Or run STDIO server
./bin/mcp_server --transport stdio
```

## Function Registration

EmbedMCP supports two registration approaches:

### Simple Functions (Recommended)

```c
// Business function
double add_numbers(double a, double b) {
    return a + b;
}

// One-line wrapper generation
EMBED_MCP_WRAPPER(add_wrapper, add_numbers, DOUBLE, DOUBLE, a, DOUBLE, b)

// Register
const char* names[] = {"a", "b"};
const char* descs[] = {"First number", "Second number"};
mcp_param_type_t types[] = {MCP_PARAM_DOUBLE, MCP_PARAM_DOUBLE};

embed_mcp_add_tool(server, "add", "Add two numbers",
                   names, descs, types, 2, MCP_RETURN_DOUBLE, add_wrapper, NULL);
```

### Array Functions (Advanced)

```c
// Business function
double sum_numbers(double* numbers, size_t count) {
    double sum = 0.0;
    for (size_t i = 0; i < count; i++) {
        sum += numbers[i];
    }
    return sum;
}

// Manual wrapper (handles memory management)
void* sum_wrapper(mcp_param_accessor_t* params, void* user_data) {
    size_t count;
    double* numbers = params->get_double_array(params, "numbers", &count);

    double result_val = sum_numbers(numbers, count);
    free(numbers); // Clean up

    double* result = malloc(sizeof(double));
    *result = result_val;
    return result;
}

// Register with array parameter
mcp_param_desc_t params[] = {
    MCP_PARAM_ARRAY_DOUBLE_DEF("numbers", "Array of numbers", "A number", 1)
};

embed_mcp_add_tool(server, "sum", "Sum numbers", params, NULL, NULL, 1,
                   MCP_RETURN_DOUBLE, sum_wrapper, NULL);
```

## Memory Management

EmbedMCP handles most memory management automatically:

- **Parameters**: All input parameters are automatically freed after your function returns
- **JSON processing**: Request/response parsing and cleanup is handled internally
- **Arrays**: Dynamic arrays are automatically allocated and freed
- **Error handling**: Memory is properly cleaned up even when errors occur

**Your responsibility**: String return values must use `malloc()`:

```c
char* get_weather(const char* city) {
    char* result = malloc(200);  // ✅ EmbedMCP will call free()
    sprintf(result, "Weather for %s: Sunny", city);
    return result;
}
```

## Server Modes

### Streamable HTTP Transport (Example)

```bash
./my_server --transport http --port 8080
```
- Multiple concurrent clients
- Session management with `MCP-Session-Id` headers
- Protocol version negotiation via `MCP-Protocol-Version` headers
- Web application backends
- Development and testing

### STDIO Transport
For MCP clients like Claude Desktop:
```bash
./my_server --transport stdio
```
- Claude Desktop integration
- AI assistant tools
- Command-line workflows
- Single client communication



## 🔧 Parameter Definition Macros

> **Powerful macros for complex parameter definitions**

<table>
<tr>
<td width="50%">

### 📊 **Array Parameters**
```c
// Double array
MCP_PARAM_ARRAY_DOUBLE_DEF(
    "numbers",
    "Array of numbers",
    "A numeric value",
    1  // required
)

// String array
MCP_PARAM_ARRAY_STRING_DEF(
    "items",
    "List of items",
    "An item name",
    1  // required
)
```

</td>
<td width="50%">

### 🎯 **Simple Parameters**
```c
// Double parameter
MCP_PARAM_DOUBLE_DEF(
    "temperature",
    "Temperature in Celsius",
    1  // required
)

// String parameter
MCP_PARAM_STRING_DEF(
    "city",
    "City name",
    0  // optional
)
```

</td>
</tr>
</table>

## Example Server

The included example demonstrates all EmbedMCP features:

```bash
# Build and run example
make && ./bin/mcp_server --transport stdio
```

### Available Demo Tools

| Tool | Parameters | Description | Example |
|------|------------|-------------|---------|
| `add` | `a: number, b: number` | Add two numbers | `add(10, 20)` → `30` |
| `sum_numbers` | `numbers: number[]` | Sum array of numbers | `sum_numbers([1,2,3])` → `6` |
| `join_strings` | `strings: string[], separator: string` | Join string array | `join_strings(["a","b"], ",")` → `"a,b"` |
| `weather` | `city: string` | Get weather info | `weather("济南")` → Weather report |
| `calculate_score` | `base_points: int, grade: string, multiplier: number` | Calculate score with bonus | `calculate_score(80, "A", 1.2)` → `120` |

### Testing with MCP Inspector

1. Start the server: `./bin/mcp_server --transport http --port 8080`
2. Open [MCP Inspector](https://inspector.mcp.dev)
3. Connect to: `http://localhost:8080/mcp`
4. Test the available tools

## Platform Support

EmbedMCP is designed for maximum portability across embedded systems:

### Embedded Systems
- **RTOS**: FreeRTOS, Zephyr, ThreadX, embOS
- **MCUs**: STM32, ESP32, Nordic nRF series
- **SBCs**: Raspberry Pi, BeagleBone, Orange Pi



### Requirements
- **Minimum**: C99 compiler, 64KB RAM, 100KB flash
- **Recommended**: 512KB RAM for complex applications
- **Dependencies**: None (self-contained)

## Use Cases

### Industrial IoT
- **Sensor data processing**: Expose C sensor drivers to AI models
- **Equipment monitoring**: Real-time analysis of machine data
- **Predictive maintenance**: AI-driven failure prediction

### Embedded AI
- **Edge computing**: Run AI inference on embedded devices
- **Smart devices**: Voice assistants, smart cameras, IoT hubs
- **Robotics**: AI-controlled robotic systems



## Troubleshooting

### Common Issues

**Build errors:**
```bash
# Missing dependencies
make deps

# Clean build
make clean && make
```

**Runtime errors:**
```bash
# Enable debug logging
./bin/mcp_server --transport stdio --debug

# Check memory usage
valgrind ./bin/mcp_server --transport stdio
```

**Connection issues:**
- Ensure correct transport mode (Streamable HTTP vs STDIO)
- Check firewall settings for Streamable HTTP mode
- Verify MCP client configuration and protocol version headers

## Contributing

At this time, we’re not accepting external code contributions (PRs).

Issues are welcome for bug reports and feature requests.

### Development Setup

```bash
# Clone repository
git clone https://github.com/AaronWander/EmbedMCP.git
cd EmbedMCP

# Build debug version
make debug

# Run tests
make test
```

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 👥 Community & Support

### Community Resources

- Contributing: At this time, we’re not accepting external code contributions (PRs).
- Issues are welcome for bug reports and feature requests.

- 🐛 Report bugs via [GitHub Issues](https://github.com/AaronWander/EmbedMCP/issues)
- 💡 Suggest features in [Discussions](https://github.com/AaronWander/EmbedMCP/discussions)
- 💬 Join our [Discord](https://discord.gg/D3D8Ngm9PB) for real-time community support

### Stay Connected

- 🐦 Twitter: [@Tiyuzhao](https://x.com/Tiyuzhao)
