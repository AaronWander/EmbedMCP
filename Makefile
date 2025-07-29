# Makefile for EmbedMCP - C Language MCP Server
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g -O2
LDFLAGS = -lm -lpthread

# Directories
SRC_DIR = src
INC_DIR = include
OBJ_DIR = obj
BIN_DIR = bin
CJSON_DIR = cjson

# Source files
SOURCES = $(wildcard $(SRC_DIR)/*.c)
CJSON_SOURCES = $(CJSON_DIR)/cJSON.c
ALL_SOURCES = $(SOURCES) $(CJSON_SOURCES)
OBJECTS = $(SOURCES:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o) $(OBJ_DIR)/cJSON.o

# Target executable
TARGET = $(BIN_DIR)/mcp_server

# Default target
all: $(TARGET)

# Create directories if they don't exist
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(CJSON_DIR):
	mkdir -p $(CJSON_DIR)

# Download cJSON if not present
$(CJSON_DIR)/cJSON.c: | $(CJSON_DIR)
	curl -L -o $(CJSON_DIR)/cJSON.c https://raw.githubusercontent.com/DaveGamble/cJSON/master/cJSON.c
	curl -L -o $(CJSON_DIR)/cJSON.h https://raw.githubusercontent.com/DaveGamble/cJSON/master/cJSON.h

# Build target
$(TARGET): $(OBJECTS) | $(BIN_DIR)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)

# Compile source files
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

.PHONY: all clean distclean deps test debug
