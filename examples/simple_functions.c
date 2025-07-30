#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "embed_mcp.h"

// =============================================================================
// Simple User Functions - No Complex Wrappers Needed!
// =============================================================================

// Simple integer function: int func(int, int)
int add_numbers(int a, int b) {
    printf("Adding %d + %d\n", a, b);
    return a + b;
}

// Simple double function: double func(double, double)
double multiply_numbers(double a, double b) {
    printf("Multiplying %.2f * %.2f\n", a, b);
    return a * b;
}

// Simple string function: char* func(const char*)
char* greet_user(const char* name) {
    printf("Greeting user: %s\n", name);
    
    char* greeting = malloc(256);
    if (!greeting) return NULL;
    
    snprintf(greeting, 256, "Hello, %s! Welcome to EmbedMCP!", name);
    return greeting;
}

// Simple array function: double func(double*, size_t)
double calculate_average(double* numbers, size_t count) {
    printf("Calculating average of %zu numbers\n", count);
    
    if (!numbers || count == 0) return 0.0;
    
    double sum = 0.0;
    for (size_t i = 0; i < count; i++) {
        sum += numbers[i];
    }
    
    return sum / count;
}

int main() {
    printf("=== EmbedMCP Simple Functions Demo ===\n\n");
    
    // Create server configuration
    embed_mcp_config_t config = {
        .name = "Simple Functions Demo",
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
        printf("Failed to create server: %s\n", embed_mcp_get_error());
        return -1;
    }
    
    printf("Registering simple functions...\n");
    
    // Register functions using the main embed_mcp_add_tool API

    // Add function: int add(int a, int b)
    const char* add_param_names[] = {"a", "b"};
    mcp_param_type_t add_param_types[] = {MCP_PARAM_INT, MCP_PARAM_INT};
    if (embed_mcp_add_tool(server, "add", "Add two integers",
                           add_param_names, add_param_types, 2,
                           MCP_RETURN_INT, add_numbers) != 0) {
        printf("Failed to register add function: %s\n", embed_mcp_get_error());
    }

    // Multiply function: double multiply(double a, double b)
    const char* multiply_param_names[] = {"a", "b"};
    mcp_param_type_t multiply_param_types[] = {MCP_PARAM_DOUBLE, MCP_PARAM_DOUBLE};
    if (embed_mcp_add_tool(server, "multiply", "Multiply two numbers",
                           multiply_param_names, multiply_param_types, 2,
                           MCP_RETURN_DOUBLE, multiply_numbers) != 0) {
        printf("Failed to register multiply function: %s\n", embed_mcp_get_error());
    }

    // Greet function: char* greet(const char* input)
    const char* greet_param_names[] = {"input"};
    mcp_param_type_t greet_param_types[] = {MCP_PARAM_STRING};
    if (embed_mcp_add_tool(server, "greet", "Greet a user",
                           greet_param_names, greet_param_types, 1,
                           MCP_RETURN_STRING, greet_user) != 0) {
        printf("Failed to register greet function: %s\n", embed_mcp_get_error());
    }

    // Note: Array functions need special handling - skipping average for now
    printf("⚠️  Array functions need special implementation - skipping average for now\n");
    
    printf("\nSimple Functions Demo Server starting...\n");
    printf("Available tools:\n");
    printf("  • add(a, b) - Add two integers\n");
    printf("  • multiply(a, b) - Multiply two numbers\n");
    printf("  • greet(input) - Greet a user\n");
    printf("\nServer running on http://localhost:8080/mcp\n");
    printf("Press Ctrl+C to stop\n\n");
    
    // Run server
    int result = embed_mcp_run(server, EMBED_MCP_TRANSPORT_HTTP);
    
    // Cleanup
    embed_mcp_destroy(server);
    return result;
}
