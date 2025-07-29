#!/bin/bash

# Test script for EmbedMCP HTTP server

SERVER_URL="http://localhost:8080"
SESSION_ID=""

echo "Testing EmbedMCP HTTP server..."

# Function to extract session ID from response headers
extract_session_id() {
    local response_file="$1"
    SESSION_ID=$(grep -i "mcp-session-id:" "$response_file" | cut -d' ' -f2 | tr -d '\r\n')
    echo "Extracted session ID: $SESSION_ID"
}

# Test 1: Initialize the server
echo "=== Test 1: Initialize server ==="
TEMP_RESPONSE=$(mktemp)
curl -s -i -X POST "$SERVER_URL" \
  -H "Content-Type: application/json" \
  -H "Accept: application/json" \
  -H "MCP-Protocol-Version: 2025-06-18" \
  -d '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-06-18","capabilities":{},"clientInfo":{"name":"TestClient","version":"1.0.0"}}}' \
  -o "$TEMP_RESPONSE"

echo "Response:"
cat "$TEMP_RESPONSE"
echo ""

# Extract session ID
extract_session_id "$TEMP_RESPONSE"

if [ -z "$SESSION_ID" ]; then
    echo "ERROR: Failed to get session ID"
    rm -f "$TEMP_RESPONSE"
    exit 1
fi

# Test 2: List tools
echo "=== Test 2: List tools ==="
curl -s -X POST "$SERVER_URL" \
  -H "Content-Type: application/json" \
  -H "Accept: application/json" \
  -H "MCP-Protocol-Version: 2025-06-18" \
  -H "Mcp-Session-Id: $SESSION_ID" \
  -d '{"jsonrpc":"2.0","id":2,"method":"tools/list","params":{}}' | \
  python3 -m json.tool

echo ""

# Test 3: Call add tool
echo "=== Test 3: Call add tool ==="
curl -s -X POST "$SERVER_URL" \
  -H "Content-Type: application/json" \
  -H "Accept: application/json" \
  -H "MCP-Protocol-Version: 2025-06-18" \
  -H "Mcp-Session-Id: $SESSION_ID" \
  -d '{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"add","arguments":{"num1":15,"num2":25}}}' | \
  python3 -m json.tool

echo ""

# Test 4: Call add tool with floating point numbers
echo "=== Test 4: Call add tool with floating point numbers ==="
curl -s -X POST "$SERVER_URL" \
  -H "Content-Type: application/json" \
  -H "Accept: application/json" \
  -H "MCP-Protocol-Version: 2025-06-18" \
  -H "Mcp-Session-Id: $SESSION_ID" \
  -d '{"jsonrpc":"2.0","id":4,"method":"tools/call","params":{"name":"add","arguments":{"num1":3.14,"num2":2.86}}}' | \
  python3 -m json.tool

echo ""

# Test 5: Test SSE connection (GET request)
echo "=== Test 5: Test SSE connection ==="
timeout 5 curl -s -N \
  -H "Accept: text/event-stream" \
  -H "MCP-Protocol-Version: 2025-06-18" \
  -H "Mcp-Session-Id: $SESSION_ID" \
  "$SERVER_URL" || echo "SSE connection test completed"

echo ""

# Test 6: Terminate session
echo "=== Test 6: Terminate session ==="
curl -s -X DELETE "$SERVER_URL" \
  -H "MCP-Protocol-Version: 2025-06-18" \
  -H "Mcp-Session-Id: $SESSION_ID" | \
  python3 -m json.tool

echo ""

# Cleanup
rm -f "$TEMP_RESPONSE"

echo "All HTTP tests completed!"
echo ""
echo "To test with MCP Inspector over HTTP:"
echo "1. Start the server: ./bin/mcp_server --http 8080"
echo "2. Use Inspector with HTTP transport: npx @modelcontextprotocol/inspector --transport http --url http://localhost:8080"
