#include "embed_mcp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

// =============================================================================
// Pure Business Function Examples - No JSON handling required!
// =============================================================================

// Example 1: Add two numbers - demonstrates basic math operations
double add_impl(double a, double b) {
    return a + b;
}

// Example 2: Sum array of numbers - demonstrates array parameter handling
double sum_array_impl(double* numbers, size_t count) {
    double total = 0.0;
    for (size_t i = 0; i < count; i++) {
        total += numbers[i];
    }
    return total;
}

// Example 3: Weather query - demonstrates string parameter and return value handling
char* weather_impl(const char* city) {
    if (strcmp(city, "济南") == 0 || strcmp(city, "jinan") == 0 ||
        strcmp(city, "Jinan") == 0 || strcmp(city, "JINAN") == 0) {
        return strdup(
            "🌤️ Jinan Weather Forecast\n\n"
            "Tonight:\n"
            "Temperature: 59°F\n"
            "Wind: 2 to 10 mph S\n"
            "Forecast: Clear, with a low around 59. South wind 2 to 10 mph, with gusts as high as 18 mph.\n\n"
            "…………………………\n\n"
            "Thursday Night:\n"
            "Temperature: 57°F\n"
            "Wind: 5 to 10 mph SSW\n"
            "Forecast: Clear, with a low around 57. South southwest wind 5 to 10 mph, with gusts as high as 20 mph."
        );
    }
    return strdup("Sorry, currently only supports weather queries for Jinan (济南).");
}





void print_usage(const char *program_name) {
    printf("Usage: %s [OPTIONS]\n", program_name);
    printf("Options:\n");
    printf("  -t, --transport TYPE    Transport type (stdio|http) [default: stdio]\n");
    printf("  -p, --port PORT         HTTP port [default: 8080]\n");
    printf("  -b, --bind HOST         HTTP bind address [default: 0.0.0.0]\n");
    printf("  -e, --endpoint PATH     HTTP endpoint path [default: /mcp] (Note: currently fixed to /mcp)\n");
    printf("  -d, --debug             Enable debug logging\n");
    printf("  -h, --help              Show this help message\n");
    printf("\nExamples:\n");
    printf("  %s                      # STDIO transport\n", program_name);
    printf("  %s -t http -p 9943      # HTTP on port 9943\n", program_name);
    printf("  %s -t http -e /api/mcp  # HTTP with custom endpoint (planned feature)\n", program_name);
}

int main(int argc, char *argv[]) {
    // Parse command line arguments
    const char *transport_type = "stdio";
    int port = 8080;
    const char *bind_address = "0.0.0.0";
    const char *endpoint_path = "/mcp";
    int debug = 0;
    
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
    
    // Create server configuration
    embed_mcp_config_t config = {
        .name = "EmbedMCP-Example",
        .version = "1.0.0",
        .host = bind_address,
        .port = port,
        .path = endpoint_path,
        .max_tools = 100,
        .debug = debug
    };

    // Create server instance
    embed_mcp_server_t *server = embed_mcp_create(&config);
    if (!server) {
        fprintf(stderr, "Failed to create server: %s\n", embed_mcp_get_error());
        return 1;
    }
    
    // =============================================================================
    // 使用纯函数API注册工具 - 用户完全不需要处理JSON！
    // =============================================================================
    
    printf("Registering tools with pure functions...\n");
    
    // =============================================================================
    // Register three example tools - demonstrating different parameter types and handling
    // =============================================================================

    printf("Registering example tools...\n");

    // Example 1: Add numbers - demonstrates basic math operations (double, double) -> double
    mcp_param_desc_t add_params[] = {
        MCP_PARAM_DOUBLE_DEF("a", "First number to add", 1),
        MCP_PARAM_DOUBLE_DEF("b", "Second number to add", 1)
    };
    embed_mcp_add_pure_function(server, "add", "Add two numbers together",
                                add_params, 2, MCP_RETURN_DOUBLE, add_impl);

    // Example 2: Array sum - demonstrates array parameter handling (double[], size_t) -> double
    mcp_param_desc_t sum_params[] = {
        MCP_PARAM_ARRAY_DOUBLE_DEF("numbers", "Array of numbers to sum", "A number to include in the sum", 1)
    };
    embed_mcp_add_pure_function(server, "sum_array", "Calculate the sum of an array of numbers",
                                sum_params, 1, MCP_RETURN_DOUBLE, sum_array_impl);

    // Example 3: Weather query - demonstrates string parameter and return value (string) -> string
    mcp_param_desc_t weather_params[] = {
        MCP_PARAM_STRING_DEF("city", "Name of the city to get weather for (currently supports: Jinan/济南)", 1)
    };
    embed_mcp_add_pure_function(server, "weather", "Get weather information for a city",
                                weather_params, 1, MCP_RETURN_STRING, weather_impl);


    
    // Run server
    printf("EmbedMCP Example Server starting with %s transport...\n", transport_type);
    if (strcmp(transport_type, "http") == 0) {
        printf("HTTP server will start on %s:%d%s\n", bind_address, port, endpoint_path);
        printf("\nExample tools available:\n");
        printf("  • add(a, b) - Add two numbers (demonstrates basic math)\n");
        printf("  • sum_array(numbers[]) - Sum array of numbers (demonstrates array handling)\n");
        printf("  • weather(city) - Get weather info (demonstrates string processing, supports: Jinan/济南)\n");
        printf("\nTry these in MCP Inspector or with curl!\n");
    }
    
    int result;
    if (strcmp(transport_type, "http") == 0) {
        result = embed_mcp_run(server, EMBED_MCP_TRANSPORT_HTTP);
    } else {
        result = embed_mcp_run(server, EMBED_MCP_TRANSPORT_STDIO);
    }
    
    // Cleanup
    embed_mcp_destroy(server);
    return result;
}
