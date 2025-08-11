# Contributing to EmbedMCP

We welcome contributions to EmbedMCP! This document provides guidelines for contributing to the project.

## How to Contribute

### Reporting Issues

- Use the GitHub issue tracker to report bugs or request features
- Provide clear, detailed descriptions of the issue
- Include steps to reproduce bugs
- Specify your platform and environment details

### Code Contributions

1. **Fork the repository** and create a feature branch
2. **Follow the coding style** used in the existing codebase
3. **Write tests** for new functionality
4. **Update documentation** as needed
5. **Submit a pull request** with a clear description

## Development Setup

### Prerequisites

- GCC or compatible C compiler
- Make build system
- POSIX-compatible system (Linux, macOS, etc.)

### Building

```bash
# Clone the repository
git clone <repository-url>
cd EmbedMCP

# Build the project
make

# Run tests
make test

# Clean build files
make clean
```

## Coding Standards

### C Code Style

- Use 4 spaces for indentation (no tabs)
- Follow K&R brace style
- Use descriptive variable and function names
- Add comments for complex logic
- Keep functions focused and reasonably sized

### Example:

```c
// Good: Clear function name and proper formatting
int embed_mcp_add_tool(embed_mcp_server_t *server,
                       const char *name,
                       const char *description,
                       const char *param_names[],
                       mcp_param_type_t param_types[],
                       size_t param_count,
                       mcp_return_type_t return_type,
                       void *function_ptr) {
    if (!server || !name) {
        return -1;
    }
    
    // Implementation here...
    return 0;
}
```

## Testing

- Write unit tests for new functionality
- Ensure all existing tests pass
- Test on multiple platforms when possible
- Include integration tests for MCP protocol compliance

## Documentation

- Update README.md for user-facing changes
- Add inline code comments for complex logic
- Update API documentation for new functions
- Include examples for new features

## Pull Request Process

1. **Create a feature branch** from `main`
2. **Make your changes** following the guidelines above
3. **Test thoroughly** on your local system
4. **Update documentation** as needed
5. **Submit a pull request** with:
   - Clear title and description
   - Reference to related issues
   - Summary of changes made
   - Testing performed

## Code Review

- All contributions require code review
- Address feedback promptly and professionally
- Be open to suggestions and improvements
- Maintain a collaborative attitude

## License

By contributing to EmbedMCP, you agree that your contributions will be licensed under the MIT License.

## Questions?

If you have questions about contributing, please:
- Check existing issues and documentation
- Open a new issue for discussion
- Reach out to the maintainers

Thank you for contributing to EmbedMCP! 🚀
