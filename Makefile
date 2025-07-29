# Makefile for EmbedMCP - C Language MCP Server (Modular Architecture)
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g -O2 -D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L
LDFLAGS = -lm -lpthread

# Directories
SRC_DIR = src
INC_DIR = include
OBJ_DIR = obj
BIN_DIR = bin
CJSON_DIR = cjson

# Module directories
PROTOCOL_SRC_DIR = $(SRC_DIR)/protocol
TRANSPORT_SRC_DIR = $(SRC_DIR)/transport
APPLICATION_SRC_DIR = $(SRC_DIR)/application
TOOLS_SRC_DIR = $(SRC_DIR)/tools
UTILS_SRC_DIR = $(SRC_DIR)/utils

PROTOCOL_OBJ_DIR = $(OBJ_DIR)/protocol
TRANSPORT_OBJ_DIR = $(OBJ_DIR)/transport
APPLICATION_OBJ_DIR = $(OBJ_DIR)/application
TOOLS_OBJ_DIR = $(OBJ_DIR)/tools
UTILS_OBJ_DIR = $(OBJ_DIR)/utils

# Source files
PROTOCOL_SOURCES = $(wildcard $(PROTOCOL_SRC_DIR)/*.c)
TRANSPORT_SOURCES = $(wildcard $(TRANSPORT_SRC_DIR)/*.c)
APPLICATION_SOURCES = $(wildcard $(APPLICATION_SRC_DIR)/*.c)
TOOLS_SOURCES = $(wildcard $(TOOLS_SRC_DIR)/*.c)
UTILS_SOURCES = $(wildcard $(UTILS_SRC_DIR)/*.c)
MAIN_SOURCES = $(wildcard $(SRC_DIR)/*.c)
CJSON_SOURCES = $(CJSON_DIR)/cJSON.c

# Object files
PROTOCOL_OBJECTS = $(PROTOCOL_SOURCES:$(PROTOCOL_SRC_DIR)/%.c=$(PROTOCOL_OBJ_DIR)/%.o)
TRANSPORT_OBJECTS = $(TRANSPORT_SOURCES:$(TRANSPORT_SRC_DIR)/%.c=$(TRANSPORT_OBJ_DIR)/%.o)
APPLICATION_OBJECTS = $(APPLICATION_SOURCES:$(APPLICATION_SRC_DIR)/%.c=$(APPLICATION_OBJ_DIR)/%.o)
TOOLS_OBJECTS = $(TOOLS_SOURCES:$(TOOLS_SRC_DIR)/%.c=$(TOOLS_OBJ_DIR)/%.o)
UTILS_OBJECTS = $(UTILS_SOURCES:$(UTILS_SRC_DIR)/%.c=$(UTILS_OBJ_DIR)/%.o)
MAIN_OBJECTS = $(MAIN_SOURCES:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
CJSON_OBJECTS = $(OBJ_DIR)/cJSON.o

ALL_OBJECTS = $(PROTOCOL_OBJECTS) $(TRANSPORT_OBJECTS) $(APPLICATION_OBJECTS) \
              $(TOOLS_OBJECTS) $(UTILS_OBJECTS) $(MAIN_OBJECTS) $(CJSON_OBJECTS)

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

# Compile protocol module
$(PROTOCOL_OBJ_DIR)/%.o: $(PROTOCOL_SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I$(INC_DIR) -I$(CJSON_DIR) -c $< -o $@

# Compile transport module
$(TRANSPORT_OBJ_DIR)/%.o: $(TRANSPORT_SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I$(INC_DIR) -I$(CJSON_DIR) -c $< -o $@

# Compile application module
$(APPLICATION_OBJ_DIR)/%.o: $(APPLICATION_SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I$(INC_DIR) -I$(CJSON_DIR) -c $< -o $@

# Compile tools module
$(TOOLS_OBJ_DIR)/%.o: $(TOOLS_SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I$(INC_DIR) -I$(CJSON_DIR) -c $< -o $@

# Compile utils module
$(UTILS_OBJ_DIR)/%.o: $(UTILS_SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I$(INC_DIR) -I$(CJSON_DIR) -c $< -o $@

# Compile main source files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I$(INC_DIR) -I$(CJSON_DIR) -c $< -o $@

# Compile cJSON
$(OBJ_DIR)/cJSON.o: $(CJSON_DIR)/cJSON.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I$(CJSON_DIR) -c $< -o $@

# Clean build artifacts (keep cjson directory)
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

# Clean everything including dependencies
distclean:
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

# Show build information
info:
	@echo "EmbedMCP Modular Build Information:"
	@echo "  Source directories:"
	@echo "    Protocol:    $(PROTOCOL_SRC_DIR)"
	@echo "    Transport:   $(TRANSPORT_SRC_DIR)"
	@echo "    Application: $(APPLICATION_SRC_DIR)"
	@echo "    Tools:       $(TOOLS_SRC_DIR)"
	@echo "    Utils:       $(UTILS_SRC_DIR)"
	@echo "  Object directories:"
	@echo "    Protocol:    $(PROTOCOL_OBJ_DIR)"
	@echo "    Transport:   $(TRANSPORT_OBJ_DIR)"
	@echo "    Application: $(APPLICATION_OBJ_DIR)"
	@echo "    Tools:       $(TOOLS_OBJ_DIR)"
	@echo "    Utils:       $(UTILS_OBJ_DIR)"
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

.PHONY: all clean distclean deps test debug protocol transport application tools utils info check
