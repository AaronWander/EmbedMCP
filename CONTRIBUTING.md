# Contributing to EmbedMCP

We welcome contributions to EmbedMCP! This document provides comprehensive guidelines for contributing to the project.

## 🎯 Ways to Contribute

### 🐛 Bug Reports
- Use GitHub Issues to report bugs
- Include detailed reproduction steps
- Specify platform, compiler, and environment details
- Attach relevant logs or error messages

### 💡 Feature Requests
- Describe the use case and problem you're solving
- Explain how it fits with EmbedMCP's goals
- Consider backward compatibility implications
- Provide examples of the proposed API

### 📝 Documentation
- Improve README files (English and Chinese)
- Add code examples and tutorials
- Fix typos and clarify explanations
- Update API documentation

### 🔧 Code Contributions
1. **Fork** the repository and create a feature branch
2. **Follow** the coding standards below
3. **Write tests** for new functionality
4. **Update documentation** as needed
5. **Test** on multiple platforms when possible
6. **Submit** a pull request with clear description

## 🚀 Development Setup

### Prerequisites

**Required:**
- C99-compatible compiler (GCC, Clang, MSVC)
- Make build system
- Git for version control

**Optional but recommended:**
- Valgrind (for memory leak detection)
- GDB (for debugging)
- Docker (for cross-platform testing)

### Quick Setup

```bash
# Clone the repository
git clone https://github.com/AaronWander/EmbedMCP.git
cd EmbedMCP

# Install dependencies (downloads cJSON)
make deps

# Build debug version (recommended for development)
make debug

# Run tests
make test

# Run example server
./bin/mcp_server --transport stdio
```

### Development Workflow

```bash
# Create feature branch
git checkout -b feature/your-feature-name

# Make changes and test
make debug && make test

# Test with example
./bin/mcp_server --transport http --port 8080

# Test memory usage
valgrind --leak-check=full ./bin/mcp_server --transport stdio

# Clean build
make clean
```

## 📋 Coding Standards

### C Code Style

**Formatting:**
- Use 4 spaces for indentation (no tabs)
- Follow K&R brace style
- Line length: 100 characters maximum
- Use Unix line endings (LF)

**Naming Conventions:**
- Functions: `embed_mcp_function_name()`
- Variables: `snake_case`
- Constants: `UPPER_SNAKE_CASE`
- Types: `typedef_name_t`
- Macros: `MACRO_NAME`

**Code Organization:**
- One declaration per line
- Group related functions together
- Use descriptive variable and function names
- Add comments for complex logic
- Keep functions focused and reasonably sized (< 50 lines)

### Good Example:

```c
/**
 * Add a tool to the MCP server
 * @param server The MCP server instance
 * @param name Tool name (must be unique)
 * @param description Human-readable description
 * @param param_names Array of parameter names
 * @param param_types Array of parameter types
 * @param param_count Number of parameters
 * @param return_type Expected return type
 * @param wrapper_func Function wrapper
 * @param user_data Optional user data
 * @return 0 on success, -1 on error
 */
int embed_mcp_add_tool(embed_mcp_server_t *server,
                       const char *name,
                       const char *description,
                       const void *param_names,
                       const char *param_descriptions[],
                       mcp_param_type_t param_types[],
                       size_t param_count,
                       mcp_return_type_t return_type,
                       mcp_universal_func_t wrapper_func,
                       void *user_data) {
    // Validate input parameters
    if (!server || !name || !wrapper_func) {
        return -1;
    }

    // Implementation here...
    return 0;
}
```

### Memory Management Rules

- Always check malloc() return values
- Free all allocated memory
- Use consistent error handling patterns
- Avoid memory leaks in error paths
- Document ownership of pointers

## 🧪 Testing Guidelines

### Test Types

**Unit Tests:**
- Test individual functions in isolation
- Cover edge cases and error conditions
- Use descriptive test names
- Aim for high code coverage

**Integration Tests:**
- Test MCP protocol compliance
- Verify tool registration and execution
- Test different transport modes (HTTP/STDIO)
- Validate JSON-RPC message handling

**Platform Testing:**
- Test on Linux, macOS, Windows when possible
- Verify embedded system compatibility
- Check memory usage on resource-constrained systems
- Test with different compilers (GCC, Clang, MSVC)

### Test Examples

```bash
# Run all tests
make test

# Run with memory checking
valgrind --leak-check=full ./bin/test_runner

# Test specific functionality
./test_mcp.sh

# Test HTTP transport
./bin/mcp_server --transport http --port 8080 &
curl -X POST http://localhost:8080/mcp -d '{"jsonrpc":"2.0","method":"initialize",...}'
```

### Adding New Tests

1. Create test files in `tests/` directory
2. Follow naming convention: `test_feature_name.c`
3. Include test in `Makefile`
4. Document test purpose and expected behavior

## 📚 Documentation Standards

### Code Documentation
- Use Doxygen-style comments for functions
- Document all public APIs
- Explain complex algorithms
- Include usage examples

### User Documentation
- Update README.md for user-facing changes
- Add examples for new features
- Update both English and Chinese versions
- Keep documentation in sync with code

### API Documentation
```c
/**
 * @brief Brief description of the function
 * @param param1 Description of parameter 1
 * @param param2 Description of parameter 2
 * @return Description of return value
 * @note Any important notes
 * @example
 * // Usage example
 * result = function_name(arg1, arg2);
 */
```

## 🔄 Pull Request Process

### Before Submitting

1. **Create a feature branch** from `main`
   ```bash
   git checkout -b feature/descriptive-name
   ```

2. **Make your changes** following the guidelines above

3. **Test thoroughly**
   ```bash
   make clean && make debug
   make test
   valgrind --leak-check=full ./bin/mcp_server --transport stdio
   ```

4. **Update documentation** as needed
   - README.md (English and Chinese if applicable)
   - Code comments and API documentation
   - CHANGELOG.md for significant changes

### Pull Request Template

When submitting a PR, include:

**Title:** Clear, descriptive title (e.g., "feat: add array parameter support")

**Description:**
- **What:** Brief description of changes
- **Why:** Motivation and context
- **How:** Technical approach taken
- **Testing:** What testing was performed
- **Breaking Changes:** Any backward compatibility issues

**Checklist:**
- [ ] Code follows project style guidelines
- [ ] Tests added/updated and passing
- [ ] Documentation updated
- [ ] No memory leaks (tested with valgrind)
- [ ] Backward compatibility maintained
- [ ] Both English and Chinese README updated (if applicable)

### Code Review Process

- All contributions require code review
- Address feedback promptly and professionally
- Be open to suggestions and improvements
- Maintain a collaborative attitude
- Reviews focus on:
  - Code correctness and safety
  - Performance implications
  - API design consistency
  - Documentation completeness
  - Test coverage

## 🎯 Contribution Areas

### High Priority
- **Cross-platform testing** - Test on embedded systems
- **Performance optimization** - Memory usage, speed improvements
- **Documentation** - Examples, tutorials, API docs
- **Array support enhancements** - More data types, nested arrays

### Medium Priority
- **Transport improvements** - WebSocket, TCP support
- **Tool discovery** - Dynamic tool registration
- **Error handling** - Better error messages and recovery
- **Logging system** - Structured logging, log levels

### Future Features
- **Resource system** - File and data resource support
- **Prompt system** - AI prompt templates
- **Plugin architecture** - Dynamic loading of tools
- **Configuration system** - Runtime configuration

## 📜 License

By contributing to EmbedMCP, you agree that your contributions will be licensed under the MIT License.

## ❓ Questions and Support

If you have questions about contributing:

1. **Check existing resources:**
   - README.md and documentation
   - Existing issues and discussions
   - Code examples in `examples/`

2. **Ask for help:**
   - Open a GitHub Issue for bugs or feature requests
   - Start a GitHub Discussion for general questions
   - Join our Discord community: https://discord.gg/D3D8Ngm9PB

3. **Contact maintainers:**
   - GitHub: [@AaronWander](https://github.com/AaronWander)

## 🙏 Recognition

Contributors will be recognized in:
- Release notes for significant contributions
- Project documentation

Thank you for contributing to EmbedMCP! Your contributions help make AI more accessible to embedded and C developers worldwide. 🚀
