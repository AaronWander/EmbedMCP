#include "mcp_server.h"
#include <signal.h>
#include <unistd.h>

// Global server instance for signal handling
static mcp_server_t g_server;
volatile bool g_running = true;

// Signal handler for graceful shutdown
void signal_handler(int sig) {
    (void)sig; // Suppress unused parameter warning
    g_running = false;
    mcp_debug_print("Received shutdown signal\n");
}

int main(int argc, char *argv[]) {
    int ret = 0;
    mcp_transport_type_t transport = MCP_TRANSPORT_STDIO;
    int port = HTTP_PORT;

    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--http") == 0) {
            transport = MCP_TRANSPORT_HTTP;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                port = atoi(argv[i + 1]);
                i++; // Skip the port argument
            }
        } else if (strcmp(argv[i], "--stdio") == 0) {
            transport = MCP_TRANSPORT_STDIO;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("Usage: %s [OPTIONS]\n", argv[0]);
            printf("Options:\n");
            printf("  --stdio          Use stdio transport (default)\n");
            printf("  --http [PORT]    Use HTTP transport on specified port (default: %d)\n", HTTP_PORT);
            printf("  --help, -h       Show this help message\n");
            return 0;
        }
    }

    // Set up signal handlers for graceful shutdown
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    mcp_debug_print("Starting EmbedMCP server with %s transport...\n",
                   transport == MCP_TRANSPORT_HTTP ? "HTTP" : "stdio");

    // Initialize the MCP server
    if (mcp_server_init(&g_server) != 0) {
        fprintf(stderr, "Failed to initialize MCP server\n");
        return 1;
    }

    g_server.transport_type = transport;

    mcp_debug_print("MCP server initialized successfully\n");

    // Run the appropriate server loop
    if (transport == MCP_TRANSPORT_HTTP) {
        ret = mcp_server_run_http(&g_server, port);
    } else {
        ret = mcp_server_run(&g_server);
    }

    // Cleanup
    mcp_server_cleanup(&g_server);

    mcp_debug_print("EmbedMCP server shutdown complete\n");

    return ret;
}
