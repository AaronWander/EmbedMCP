#include "embed_mcp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

// =============================================================================
// Pure Business Function Examples - No JSON handling required!
// =============================================================================

// =============================================================================
// Pure Business Functions - No Wrappers Needed!
// =============================================================================

// Example 1: Simple math operation - pure business logic
double add_numbers(double a, double b) {
    printf("[DEBUG] Adding %.2f + %.2f\n", a, b);
    return a + b;
}

// Example 2: Array processing - pure business logic
double sum_array(double* numbers, size_t count) {
    printf("[DEBUG] Summing array of %zu numbers\n", count);

    if (!numbers || count == 0) {
        return 0.0;
    }

    double total = 0.0;
    for (size_t i = 0; i < count; i++) {
        total += numbers[i];
        printf("[DEBUG]   numbers[%zu] = %.2f, running total = %.2f\n", i, numbers[i], total);
    }

    return total;
}

// Example 3: String processing - pure business logic
char* get_weather(const char* city) {
    printf("[DEBUG] Getting weather for city: %s\n", city);

    if (strcmp(city, "济南") == 0 || strcmp(city, "jinan") == 0 ||
        strcmp(city, "Jinan") == 0 || strcmp(city, "JINAN") == 0) {
        return strdup(
            "🌤️ Jinan Weather Forecast\n\n"
            "Current: 22°C, Partly Cloudy\n"
            "Humidity: 65%\n"
            "Wind: 12 km/h NE\n"
            "UV Index: 6 (High)\n\n"
            "Tomorrow: 25°C, Sunny\n"
            "Weekend: Light rain expected\n\n"
            "Air Quality: Good (AQI: 45)\n"
            "Sunrise: 06:12 | Sunset: 19:45"
        );
    }

    return strdup("Weather information is currently only available for Jinan (济南). Please try 'jinan', 'Jinan', or '济南'.");
}

// Example 4: Multi-parameter function with mixed types
int calculate_score(int base_points, char grade, double multiplier) {
    printf("[DEBUG] Calculating score: base=%d, grade='%c', multiplier=%.2f\n",
           base_points, grade, multiplier);

    double score = base_points * multiplier;

    // Apply grade bonus
    switch (grade) {
        case 'A': case 'a': score *= 1.2; break;
        case 'B': case 'b': score *= 1.1; break;
        case 'C': case 'c': score *= 1.0; break;
        case 'D': case 'd': score *= 0.9; break;
        default: score *= 0.8; break;
    }

    int final_score = (int)score;
    printf("[DEBUG] Final score: %d\n", final_score);

    return final_score;
}


void print_usage(const char *program_name) {
    printf("Usage: %s [OPTIONS]\n", program_name);
    printf("Options:\n");
    printf("  -t, --transport TYPE    Transport type (stdio|http) [default: stdio]\n");
    printf("  -p, --port PORT         HTTP port [default: 9943]\n");
    printf("  -b, --bind HOST         HTTP bind address [default: 0.0.0.0]\n");
    printf("  -e, --endpoint PATH     HTTP endpoint path [default: /mcp]\n");
    printf("  -d, --debug             Enable debug logging\n");
    printf("  -h, --help              Show this help message\n");
    printf("\nExamples:\n");
    printf("  %s                      # STDIO transport\n", program_name);
    printf("  %s -t http              # HTTP on default port 9943\n", program_name);
    printf("  %s -t http -p 8080      # HTTP on port 8080\n", program_name);
    printf("  %s -t http -b 192.168.1.100  # HTTP bind to specific IP\n", program_name);
    printf("\nRaspberry Pi Examples:\n");
    printf("  %s -t http -p 9943 -d   # HTTP with debug on Pi\n", program_name);
    printf("  %s -t http -b $(hostname -I | cut -d' ' -f1) # Bind to Pi's IP\n", program_name);
}

int main(int argc, char *argv[]) {
    // Parse command line arguments
    const char *transport_type = "stdio";
    int port = 9943;  // 默认使用 MCP 标准端口
    const char *bind_address = "0.0.0.0";
    const char *endpoint_path = "/mcp";
    int debug = 0;
    int result;
         
    static struct option long_options[] = {
        {"transport", required_argument, 0, 't'},
        {"port", required_argument, 0, 'p'},
        {"bind", required_argument, 0, 'b'},
        {"endpoint", required_argument, 0, 'e'},
        {"debug", no_argument, 0, 'd'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    int c;
    while ((c = getopt_long(argc, argv, "t:p:b:e:dh", long_options, NULL)) != -1) {
        switch (c) {
            case 't': transport_type = optarg; break;
            case 'p': port = atoi(optarg); break;
            case 'b': bind_address = optarg; break;
            case 'e': endpoint_path = optarg; break;
            case 'd': debug = 1; break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    // 显示系统信息 (对树莓派有用)
    printf("=== EmbedMCP Server ===\n");
    printf("Platform: %s\n",
#ifdef __arm__
           "ARM (Raspberry Pi)"
#elif defined(__aarch64__)
           "ARM64 (Raspberry Pi 64-bit)"
#elif defined(__x86_64__)
           "x86_64 (Linux/Mac)"
#else
           "Unknown"
#endif
    );

    // 如果是 HTTP 模式，显示网络信息
    if (strcmp(transport_type, "http") == 0) {
        printf("Network Interface: %s:%d\n", bind_address, port);
        printf("Endpoint: %s\n", endpoint_path);

        // 尝试获取本机 IP (对树莓派很有用)
        if (strcmp(bind_address, "0.0.0.0") == 0) {
            printf("Note: Server will bind to all interfaces (0.0.0.0)\n");
            printf("      Access via: http://<your-pi-ip>:%d%s\n", port, endpoint_path);
            printf("      Find Pi IP with: hostname -I\n");
        }
    }
    printf("\n");

    // Create server configuration
    embed_mcp_config_t config = {
        .name = "EmbedMCP-RaspberryPi",
        .version = "1.0.0",
        .instructions = "EmbedMCP server with mathematical and utility tools. "
                       "Available tools: add(a,b) for addition, weather(city) for weather info, "
                       "and calculate_score(base,grade,multiplier) for grade calculations.",
        .host = bind_address,
        .port = port,
        .path = endpoint_path,
        .max_tools = 100,
        .debug = debug,

        // 树莓派优化的连接配置
        .max_connections = 3,       // 树莓派资源有限，减少并发连接
        .session_timeout = 1800,    // 30 minutes session timeout
        .enable_sessions = 1,       // Enable session management
        .auto_cleanup = 1           // Auto cleanup expired sessions
    };

    // Create server instance
    embed_mcp_server_t *server = embed_mcp_create(&config);
    if (!server) {
        fprintf(stderr, "Failed to create server: %s\n", embed_mcp_get_error());
        return 1;
    }

    // Example 1: Simple math function - double add_numbers(double a, double b)
    const char* add_param_names[] = {"a", "b"};
    mcp_param_type_t add_param_types[] = {MCP_PARAM_DOUBLE, MCP_PARAM_DOUBLE};

    if (embed_mcp_add_tool(server, "add", "Add two numbers together",
                                  add_param_names, add_param_types, 2,
                                  MCP_RETURN_DOUBLE, add_numbers) != 0) {
        printf("Failed to register 'add' function: %s\n", embed_mcp_get_error());
    } else {
        printf("Registered add(double, double) -> double\n");
    }

    // Example 2: Array processing function - double sum_array(double*, size_t)
    // Note: This requires special handling since it's not a simple parameter combination
    // For now, we'll skip this and implement it later with array support
    printf("⚠️  Array functions need special implementation - skipping sum_array for now\n");

    // Example 3: String function - char* get_weather(const char*)
    const char* weather_param_names[] = {"city"};
    mcp_param_type_t weather_param_types[] = {MCP_PARAM_STRING};

    if (embed_mcp_add_tool(server, "weather", "Get weather information for a city",
                                  weather_param_names, weather_param_types, 1,
                                  MCP_RETURN_STRING, get_weather) != 0) {
        printf("Failed to register 'weather' function: %s\n", embed_mcp_get_error());
    } else {
        printf("Registered get_weather(const char*) -> char*\n");
    }

    // Example 4: Multi-parameter function - int calculate_score(int, char, double)
    const char* score_param_names[] = {"base_points", "grade", "multiplier"};
    mcp_param_type_t score_param_types[] = {MCP_PARAM_INT, MCP_PARAM_CHAR, MCP_PARAM_DOUBLE};

    if (embed_mcp_add_tool(server, "calculate_score", "Calculate score with grade bonus",
                                  score_param_names, score_param_types, 3,
                                  MCP_RETURN_INT, calculate_score) != 0) {
        printf("Failed to register 'calculate_score' function: %s\n", embed_mcp_get_error());
    } else {
        printf("Registered calculate_score(int, char, double) -> int\n");
    }




    
    // Run server
    printf("EmbedMCP Example Server starting with %s transport...\n", transport_type);
    if (strcmp(transport_type, "http") == 0) {
        printf("HTTP server will start on %s:%d%s\n", bind_address, port, endpoint_path);
        printf("\nExample tools available:\n");
        printf("  • add(a, b) - Add two numbers (demonstrates basic math)\n");
        printf("  • sum_array(numbers[]) - Sum array of numbers (demonstrates array handling)\n");
        printf("  • weather(city) - Get weather info (supports: Jinan/济南)\n");
        printf("  • calculate_score(base, grade, multiplier) - Calculate score with grade bonus\n");
        printf("\nTry these in MCP Inspector, Dify, or with curl!\n");
        printf("Example curl test:\n");
        printf("  curl -X POST http://%s:%d%s \\\n",
               strcmp(bind_address, "0.0.0.0") == 0 ? "localhost" : bind_address, port, endpoint_path);
        printf("       -H 'Content-Type: application/json' \\\n");
        printf("       -d '{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/list\"}'\n");
    }
    

    if (strcmp(transport_type, "http") == 0) {
        result = embed_mcp_run(server, EMBED_MCP_TRANSPORT_HTTP);
    } else {
        result = embed_mcp_run(server, EMBED_MCP_TRANSPORT_STDIO);
    }
    
    // Cleanup
    embed_mcp_destroy(server);
    return result;
}
