# EmbedMCP Library Makefile Include
# Include this file in your project's Makefile to easily compile with EmbedMCP

# Assume EMBED_MCP_DIR is set to the path of embed_mcp directory
# If not set, assume it's in the current directory
EMBED_MCP_DIR ?= embed_mcp

# EmbedMCP source files
EMBED_MCP_SOURCES = \
	$(EMBED_MCP_DIR)/embed_mcp.c \
	$(wildcard $(EMBED_MCP_DIR)/protocol/*.c) \
	$(wildcard $(EMBED_MCP_DIR)/transport/*.c) \
	$(wildcard $(EMBED_MCP_DIR)/application/*.c) \
	$(wildcard $(EMBED_MCP_DIR)/tools/*.c) \
	$(wildcard $(EMBED_MCP_DIR)/utils/*.c) \
	$(wildcard $(EMBED_MCP_DIR)/hal/*.c) \
	$(wildcard $(EMBED_MCP_DIR)/platform/*.c) \
	$(wildcard $(EMBED_MCP_DIR)/platform/custom/*.c) \
	$(wildcard $(EMBED_MCP_DIR)/platform/linux/*.c) \
	$(wildcard $(EMBED_MCP_DIR)/platform/freertos/*.c) \
	$(EMBED_MCP_DIR)/cjson/cJSON.c

# EmbedMCP include directories
EMBED_MCP_INCLUDES = -I$(EMBED_MCP_DIR) -I$(EMBED_MCP_DIR)/cjson

# EmbedMCP required libraries
EMBED_MCP_LIBS = -lpthread -lm

# Platform detection
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
    EMBED_MCP_CFLAGS += -DMCP_PLATFORM_LINUX
endif
ifeq ($(UNAME_S),Darwin)
    EMBED_MCP_CFLAGS += -DMCP_PLATFORM_LINUX  # macOS uses Linux HAL
endif

# Usage example:
# In your Makefile:
# include embed_mcp/Makefile.inc
# 
# my_app: main.c $(EMBED_MCP_SOURCES)
# 	$(CC) $(CFLAGS) $(EMBED_MCP_CFLAGS) $(EMBED_MCP_INCLUDES) \
# 	      main.c $(EMBED_MCP_SOURCES) $(EMBED_MCP_LIBS) -o my_app
