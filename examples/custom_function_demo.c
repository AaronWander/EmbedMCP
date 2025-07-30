#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "embed_mcp.h"

// =============================================================================
// User's Pure Business Functions - No Wrappers Needed!
// =============================================================================

// Your exact function signature: int pp(char c, int a, int b, char d)
int pp(char c, int a, int b, char d) {
    printf("pp called with: c='%c', a=%d, b=%d, d='%c'\n", c, a, b, d);
    
    // Some business logic
    int result = (int)c + a + b + (int)d;
    printf("Result: %d + %d + %d + %d = %d\n", (int)c, a, b, (int)d, result);
    
    return result;
}

// Another custom function with different signature
double calculate_score(int points, char grade, double multiplier) {
    printf("calculate_score called with: points=%d, grade='%c', multiplier=%.2f\n", 
           points, grade, multiplier);
    
    double base_score = points * multiplier;
    
    // Grade bonus
    switch (grade) {
        case 'A': base_score *= 1.2; break;
        case 'B': base_score *= 1.1; break;
        case 'C': base_score *= 1.0; break;
        default: base_score *= 0.9; break;
    }
    
    printf("Final score: %.2f\n", base_score);
    return base_score;
}

int main() {
    printf("=== EmbedMCP Custom Function Demo ===\n\n");
    
    // Create server configuration
    embed_mcp_config_t config = {
        .name = "Custom Function Demo",
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
    
    printf("Registering custom functions...\n");
    
    // Register your pp function: int pp(char c, int a, int b, char d)
    const char* pp_param_names[] = {"c", "a", "b", "d"};
    mcp_param_type_t pp_param_types[] = {MCP_PARAM_CHAR, MCP_PARAM_INT, MCP_PARAM_INT, MCP_PARAM_CHAR};
    
    if (embed_mcp_add_tool(server, "pp", "Process with char-int-int-char parameters",
                                  pp_param_names, pp_param_types, 4,
                                  MCP_RETURN_INT, pp) != 0) {
        printf("Failed to register pp function: %s\n", embed_mcp_get_error());
    } else {
        printf("✅ Successfully registered pp(char, int, int, char) -> int\n");
    }
    
    // Register calculate_score function: double calculate_score(int, char, double)
    const char* score_param_names[] = {"points", "grade", "multiplier"};
    mcp_param_type_t score_param_types[] = {MCP_PARAM_INT, MCP_PARAM_CHAR, MCP_PARAM_DOUBLE};
    
    if (embed_mcp_add_tool(server, "calculate_score", "Calculate score with grade bonus",
                                  score_param_names, score_param_types, 3,
                                  MCP_RETURN_DOUBLE, calculate_score) != 0) {
        printf("Failed to register calculate_score function: %s\n", embed_mcp_get_error());
    } else {
        printf("✅ Successfully registered calculate_score(int, char, double) -> double\n");
    }
    
    printf("\nCustom Function Demo Server starting...\n");
    printf("Available tools:\n");
    printf("  • pp(c, a, b, d) - Your exact function signature!\n");
    printf("    Example: {\"c\": \"X\", \"a\": 10, \"b\": 20, \"d\": \"Y\"}\n");
    printf("  • calculate_score(points, grade, multiplier) - Score calculation\n");
    printf("    Example: {\"points\": 85, \"grade\": \"A\", \"multiplier\": 1.5}\n");
    printf("\nServer running on http://localhost:8080/mcp\n");
    printf("Press Ctrl+C to stop\n\n");
    
    // Test commands you can try:
    printf("Test commands:\n");
    printf("curl -X POST http://localhost:8080/mcp -H \"Content-Type: application/json\" \\\n");
    printf("  -d '{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/call\",\"params\":{\"name\":\"pp\",\"arguments\":{\"c\":\"A\",\"a\":10,\"b\":20,\"d\":\"Z\"}}}'\n\n");
    
    // Run server
    int result = embed_mcp_run(server, EMBED_MCP_TRANSPORT_HTTP);
    
    // Cleanup
    embed_mcp_destroy(server);
    return result;
}
