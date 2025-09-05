# Makefile for EmbedMCP - Using embed_mcp/ library (dogfooding our own library!)
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g -O2 -D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L -DMG_ENABLE_LINES=1
LDFLAGS = -lm -lpthread

# Note: libffi removed - not used in current implementation

# Directories
EMBED_MCP_DIR = embed_mcp
OBJ_DIR = obj
BIN_DIR = bin

# EmbedMCP library module directories (from embed_mcp/)
PROTOCOL_SRC_DIR = $(EMBED_MCP_DIR)/protocol
TRANSPORT_SRC_DIR = $(EMBED_MCP_DIR)/transport
APPLICATION_SRC_DIR = $(EMBED_MCP_DIR)/application
TOOLS_SRC_DIR = $(EMBED_MCP_DIR)/tools
UTILS_SRC_DIR = $(EMBED_MCP_DIR)/utils
HAL_SRC_DIR = $(EMBED_MCP_DIR)/hal
PLATFORM_SRC_DIR = $(EMBED_MCP_DIR)/platform
CJSON_DIR = $(EMBED_MCP_DIR)/cjson

PROTOCOL_OBJ_DIR = $(OBJ_DIR)/protocol
TRANSPORT_OBJ_DIR = $(OBJ_DIR)/transport
APPLICATION_OBJ_DIR = $(OBJ_DIR)/application
TOOLS_OBJ_DIR = $(OBJ_DIR)/tools
UTILS_OBJ_DIR = $(OBJ_DIR)/utils
HAL_OBJ_DIR = $(OBJ_DIR)/hal
PLATFORM_OBJ_DIR = $(OBJ_DIR)/platform

# Source files (using embed_mcp/ library)
PROTOCOL_SOURCES = $(wildcard $(PROTOCOL_SRC_DIR)/*.c)
TRANSPORT_SOURCES = $(wildcard $(TRANSPORT_SRC_DIR)/*.c)
APPLICATION_SOURCES = $(wildcard $(APPLICATION_SRC_DIR)/*.c)
TOOLS_SOURCES = $(wildcard $(TOOLS_SRC_DIR)/*.c)
UTILS_SOURCES = $(wildcard $(UTILS_SRC_DIR)/*.c)
HAL_SOURCES = $(wildcard $(HAL_SRC_DIR)/*.c)
PLATFORM_SOURCES = $(wildcard $(PLATFORM_SRC_DIR)/*.c) $(wildcard $(PLATFORM_SRC_DIR)/*/*.c)
EMBED_MCP_MAIN = $(EMBED_MCP_DIR)/embed_mcp.c
EXAMPLE_MAIN = examples/main.c
CJSON_SOURCES = $(CJSON_DIR)/cJSON.c

# Object files
PROTOCOL_OBJECTS = $(PROTOCOL_SOURCES:$(PROTOCOL_SRC_DIR)/%.c=$(PROTOCOL_OBJ_DIR)/%.o)
TRANSPORT_OBJECTS = $(TRANSPORT_SOURCES:$(TRANSPORT_SRC_DIR)/%.c=$(TRANSPORT_OBJ_DIR)/%.o)
APPLICATION_OBJECTS = $(APPLICATION_SOURCES:$(APPLICATION_SRC_DIR)/%.c=$(APPLICATION_OBJ_DIR)/%.o)
TOOLS_OBJECTS = $(TOOLS_SOURCES:$(TOOLS_SRC_DIR)/%.c=$(TOOLS_OBJ_DIR)/%.o)
UTILS_OBJECTS = $(UTILS_SOURCES:$(UTILS_SRC_DIR)/%.c=$(UTILS_OBJ_DIR)/%.o)
HAL_OBJECTS = $(HAL_SOURCES:$(HAL_SRC_DIR)/%.c=$(HAL_OBJ_DIR)/%.o)
PLATFORM_OBJECTS = $(PLATFORM_SOURCES:$(PLATFORM_SRC_DIR)/%.c=$(PLATFORM_OBJ_DIR)/%.o)
EMBED_MCP_OBJECT = $(OBJ_DIR)/embed_mcp.o
EXAMPLE_OBJECT = $(OBJ_DIR)/main.o
CJSON_OBJECTS = $(OBJ_DIR)/cJSON.o

ALL_OBJECTS = $(PROTOCOL_OBJECTS) $(TRANSPORT_OBJECTS) $(APPLICATION_OBJECTS) \
              $(TOOLS_OBJECTS) $(UTILS_OBJECTS) $(HAL_OBJECTS) $(PLATFORM_OBJECTS) \
              $(EMBED_MCP_OBJECT) $(EXAMPLE_OBJECT) $(CJSON_OBJECTS)

# Target executable
TARGET = $(BIN_DIR)/mcp_server

# Default target
all: $(TARGET)

# Create directories if they don't exist
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)
	mkdir -p $(PROTOCOL_OBJ_DIR)
	mkdir -p $(TRANSPORT_OBJ_DIR)
	mkdir -p $(APPLICATION_OBJ_DIR)
	mkdir -p $(TOOLS_OBJ_DIR)
	mkdir -p $(UTILS_OBJ_DIR)
	mkdir -p $(HAL_OBJ_DIR)
	mkdir -p $(PLATFORM_OBJ_DIR)
	mkdir -p $(PLATFORM_OBJ_DIR)/linux
	mkdir -p $(PLATFORM_OBJ_DIR)/freertos
	mkdir -p $(PLATFORM_OBJ_DIR)/custom

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(CJSON_DIR):
	mkdir -p $(CJSON_DIR)

# Download cJSON if not present
$(CJSON_DIR)/cJSON.c: | $(CJSON_DIR)
	curl -L -o $(CJSON_DIR)/cJSON.c https://raw.githubusercontent.com/DaveGamble/cJSON/master/cJSON.c
	curl -L -o $(CJSON_DIR)/cJSON.h https://raw.githubusercontent.com/DaveGamble/cJSON/master/cJSON.h

# Build target
$(TARGET): $(ALL_OBJECTS) | $(BIN_DIR)
	$(CC) $(ALL_OBJECTS) -o $@ $(LDFLAGS)

# Compile protocol module (from embed_mcp/)
$(PROTOCOL_OBJ_DIR)/%.o: $(PROTOCOL_SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I$(EMBED_MCP_DIR) -I$(CJSON_DIR) -c $< -o $@

# Compile transport module (from embed_mcp/)
$(TRANSPORT_OBJ_DIR)/%.o: $(TRANSPORT_SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I$(EMBED_MCP_DIR) -I$(CJSON_DIR) -c $< -o $@

# Compile application module (from embed_mcp/)
$(APPLICATION_OBJ_DIR)/%.o: $(APPLICATION_SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I$(EMBED_MCP_DIR) -I$(CJSON_DIR) -c $< -o $@

# Compile tools module (from embed_mcp/)
$(TOOLS_OBJ_DIR)/%.o: $(TOOLS_SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I$(EMBED_MCP_DIR) -I$(CJSON_DIR) -c $< -o $@

# Compile utils module (from embed_mcp/)
$(UTILS_OBJ_DIR)/%.o: $(UTILS_SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I$(EMBED_MCP_DIR) -I$(CJSON_DIR) -c $< -o $@

# Compile HAL module (from embed_mcp/)
$(HAL_OBJ_DIR)/%.o: $(HAL_SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I$(EMBED_MCP_DIR) -I$(CJSON_DIR) -c $< -o $@

# Compile platform modules (from embed_mcp/platform/)
$(PLATFORM_OBJ_DIR)/%.o: $(PLATFORM_SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I$(EMBED_MCP_DIR) -I$(CJSON_DIR) -I$(PLATFORM_SRC_DIR)/linux -c $< -o $@

# Special rule for mongoose (needs specific flags for macOS)
$(PLATFORM_OBJ_DIR)/linux/mongoose.o: $(PLATFORM_SRC_DIR)/linux/mongoose.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I$(EMBED_MCP_DIR) -I$(CJSON_DIR) -I$(PLATFORM_SRC_DIR)/linux \
		-DMG_ARCH=MG_ARCH_UNIX -DMG_ENABLE_SOCKET=1 -DMG_ENABLE_IPV6=1 \
		-DMG_ENABLE_LOG=1 -DMG_ENABLE_POLL=0 -DMG_ENABLE_EPOLL=0 \
		-D_DARWIN_C_SOURCE -std=gnu99 -Wno-unused-function -c $< -o $@

# Compile embed_mcp main library file
$(OBJ_DIR)/embed_mcp.o: $(EMBED_MCP_MAIN) | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I$(EMBED_MCP_DIR) -I$(CJSON_DIR) -c $< -o $@

# Compile example main (uses embed_mcp/ library)
$(OBJ_DIR)/main.o: $(EXAMPLE_MAIN) | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I$(EMBED_MCP_DIR) -I$(CJSON_DIR) -c $< -o $@

# Compile cJSON (from embed_mcp/)
$(OBJ_DIR)/cJSON.o: $(CJSON_DIR)/cJSON.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I$(CJSON_DIR) -c $< -o $@

# Clean build artifacts (keep cjson directory)
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

# Clean everything including dependencies
distclean: clean
	rm -rf $(OBJ_DIR) $(BIN_DIR) $(CJSON_DIR)

# Download dependencies
deps: $(CJSON_DIR)/cJSON.c

# Test the server
test: $(TARGET)
	echo '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-06-18","capabilities":{},"clientInfo":{"name":"TestClient","version":"1.0.0"}}}' | $(TARGET)

# Debug build
debug: CFLAGS += -DDEBUG -g3
debug: $(TARGET)

# Build individual modules (for development)
protocol: $(PROTOCOL_OBJECTS)
	@echo "Protocol module compiled successfully"

transport: $(TRANSPORT_OBJECTS)
	@echo "Transport module compiled successfully"

application: $(APPLICATION_OBJECTS)
	@echo "Application module compiled successfully"

tools: $(TOOLS_OBJECTS)
	@echo "Tools module compiled successfully"

utils: $(UTILS_OBJECTS)
	@echo "Utils module compiled successfully"

hal: $(HAL_OBJECTS)
	@echo "HAL module compiled successfully"

# Show build information
info:
	@echo "EmbedMCP Modular Build Information:"
	@echo "  Source directories:"
	@echo "    Protocol:    $(PROTOCOL_SRC_DIR)"
	@echo "    Transport:   $(TRANSPORT_SRC_DIR)"
	@echo "    Application: $(APPLICATION_SRC_DIR)"
	@echo "    Tools:       $(TOOLS_SRC_DIR)"
	@echo "    Utils:       $(UTILS_SRC_DIR)"
	@echo "    HAL:         $(HAL_SRC_DIR)"
	@echo "  Object directories:"
	@echo "    Protocol:    $(PROTOCOL_OBJ_DIR)"
	@echo "    Transport:   $(TRANSPORT_OBJ_DIR)"
	@echo "    Application: $(APPLICATION_OBJ_DIR)"
	@echo "    Tools:       $(TOOLS_OBJ_DIR)"
	@echo "    Utils:       $(UTILS_OBJ_DIR)"
	@echo "    HAL:         $(HAL_OBJ_DIR)"
	@echo "  Include path: $(INC_DIR)"
	@echo "  cJSON path:   $(CJSON_DIR)"
	@echo "  Target:       $(TARGET)"

# Check for missing source files
check:
	@echo "Checking for source files..."
	@if [ ! -d "$(PROTOCOL_SRC_DIR)" ]; then echo "Warning: $(PROTOCOL_SRC_DIR) directory not found"; fi
	@if [ ! -d "$(TRANSPORT_SRC_DIR)" ]; then echo "Warning: $(TRANSPORT_SRC_DIR) directory not found"; fi
	@if [ ! -d "$(APPLICATION_SRC_DIR)" ]; then echo "Warning: $(APPLICATION_SRC_DIR) directory not found"; fi
	@if [ ! -d "$(TOOLS_SRC_DIR)" ]; then echo "Warning: $(TOOLS_SRC_DIR) directory not found"; fi
	@if [ ! -d "$(UTILS_SRC_DIR)" ]; then echo "Warning: $(UTILS_SRC_DIR) directory not found"; fi
	@if [ ! -f "$(CJSON_DIR)/cJSON.c" ]; then echo "Warning: cJSON not found, run 'make deps' first"; fi
	@echo "Check complete."

# Create distribution package for easy integration
dist: debug
	@echo "Creating distribution package..."
	@mkdir -p dist/embed_mcp
	@mkdir -p dist/embed_mcp/examples

	# Copy the complete embed_mcp library
	@cp -r embed_mcp/* dist/embed_mcp/

	# Note: Library is already included as source files in embed_mcp/

	# Copy examples
	@cp examples/*.c dist/embed_mcp/examples/ 2>/dev/null || true
	@cp examples/main.c dist/embed_mcp/examples/server_example.c

	@echo "Distribution package created in dist/embed_mcp/"
	@echo ""
	@echo "Integration instructions:"
	@echo "1. Copy dist/embed_mcp/ to your project"
	@echo "2. Include: #include \"embed_mcp/embed_mcp.h\""
	@echo "3. Compile: gcc your_app.c embed_mcp/*.c embed_mcp/*/*.c -I. -o your_app"

.PHONY: all clean distclean deps test debug protocol transport application tools utils info check dist
