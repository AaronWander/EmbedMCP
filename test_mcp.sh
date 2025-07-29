#!/bin/bash

# Test script for EmbedMCP server

echo "Testing EmbedMCP server..."

# Create a temporary file for the test session
TEMP_INPUT=$(mktemp)
TEMP_OUTPUT=$(mktemp)

# Cleanup function
cleanup() {
    rm -f "$TEMP_INPUT" "$TEMP_OUTPUT"
}
trap cleanup EXIT

# Test 1: Initialize the server
echo "=== Test 1: Initialize server ==="
cat > "$TEMP_INPUT" << 'EOF'
{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-06-18","capabilities":{},"clientInfo":{"name":"TestClient","version":"1.0.0"}}}
{"jsonrpc":"2.0","method":"notifications/initialized"}
EOF

echo "Input:"
cat "$TEMP_INPUT"
echo -e "\nOutput:"
./bin/mcp_server < "$TEMP_INPUT" > "$TEMP_OUTPUT" 2>/dev/null &
SERVER_PID=$!
sleep 1
kill $SERVER_PID 2>/dev/null
wait $SERVER_PID 2>/dev/null
cat "$TEMP_OUTPUT"

echo -e "\n=== Test 2: List tools ==="
cat > "$TEMP_INPUT" << 'EOF'
{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-06-18","capabilities":{},"clientInfo":{"name":"TestClient","version":"1.0.0"}}}
{"jsonrpc":"2.0","method":"notifications/initialized"}
{"jsonrpc":"2.0","id":2,"method":"tools/list","params":{}}
EOF

echo "Input:"
tail -n 1 "$TEMP_INPUT"
echo -e "\nOutput:"
./bin/mcp_server < "$TEMP_INPUT" > "$TEMP_OUTPUT" 2>/dev/null &
SERVER_PID=$!
sleep 1
kill $SERVER_PID 2>/dev/null
wait $SERVER_PID 2>/dev/null
# Show only the tools/list response (last JSON object)
tail -n 1 "$TEMP_OUTPUT"

echo -e "\n=== Test 3: Call add tool ==="
cat > "$TEMP_INPUT" << 'EOF'
{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-06-18","capabilities":{},"clientInfo":{"name":"TestClient","version":"1.0.0"}}}
{"jsonrpc":"2.0","method":"notifications/initialized"}
{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"add","arguments":{"num1":5,"num2":3}}}
EOF

echo "Input:"
tail -n 1 "$TEMP_INPUT"
echo -e "\nOutput:"
./bin/mcp_server < "$TEMP_INPUT" > "$TEMP_OUTPUT" 2>/dev/null &
SERVER_PID=$!
sleep 1
kill $SERVER_PID 2>/dev/null
wait $SERVER_PID 2>/dev/null
# Show only the tools/call response (last JSON object)
tail -n 1 "$TEMP_OUTPUT"

echo -e "\n=== Test 4: Call add tool with different numbers ==="
cat > "$TEMP_INPUT" << 'EOF'
{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-06-18","capabilities":{},"clientInfo":{"name":"TestClient","version":"1.0.0"}}}
{"jsonrpc":"2.0","method":"notifications/initialized"}
{"jsonrpc":"2.0","id":4,"method":"tools/call","params":{"name":"add","arguments":{"num1":10.5,"num2":7.3}}}
EOF

echo "Input:"
tail -n 1 "$TEMP_INPUT"
echo -e "\nOutput:"
./bin/mcp_server < "$TEMP_INPUT" > "$TEMP_OUTPUT" 2>/dev/null &
SERVER_PID=$!
sleep 1
kill $SERVER_PID 2>/dev/null
wait $SERVER_PID 2>/dev/null
# Show only the tools/call response (last JSON object)
tail -n 1 "$TEMP_OUTPUT"

echo -e "\nAll tests completed!"
