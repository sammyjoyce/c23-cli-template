# Contributing to CLI Application Starter

Thank you for your interest in contributing to this project! This document provides guidelines and instructions for contributing.

## Code of Conduct

By participating in this project, you agree to be respectful and constructive in all interactions.

## How to Contribute

### Reporting Issues

Before creating an issue, please:

- Check existing issues to avoid duplicates
- Use the issue templates when available
- Include as much relevant information as possible

When reporting bugs, include:

- Your operating system and version
- Zig version (run `zig version`)
- Steps to reproduce the issue
- Expected vs actual behavior
- Any error messages or logs

### Suggesting Features

Feature requests are welcome! Please:

- Check if the feature has already been requested
- Explain the use case and why it would be valuable
- Consider if it aligns with the project's goals

### Pull Requests

1. **Fork the repository** and create your branch from `main`
2. **Follow the coding style** used throughout the project
3. **Write tests** for new functionality
4. **Update documentation** as needed
5. **Ensure all tests pass** by running `zig build test`
6. **Format your code** with `clang-format`
7. **Write clear commit messages** following conventional commits

#### Commit Message Format

We follow the [Conventional Commits](https://www.conventionalcommits.org/) specification:

```
<type>(<scope>): <subject>

<body>

<footer>
```

Types:

- `feat`: New feature
- `fix`: Bug fix
- `docs`: Documentation changes
- `style`: Code style changes (formatting, etc.)
- `refactor`: Code refactoring
- `test`: Test additions or modifications
- `chore`: Maintenance tasks
- `perf`: Performance improvements

Examples:

```
feat(cli): add new command for file processing
fix(config): handle missing config file gracefully
docs(readme): update installation instructions
```

### Development Setup

1. **Install Zig** (master branch recommended):

   ```bash
   # Using zvm (recommended)
   zvm install master
   zvm use master
   
   # Or download directly
   # Visit https://ziglang.org/download/
   ```

2. **Clone your fork**:

   ```bash
   git clone https://github.com/yourusername/yourproject.git
   cd yourproject
   ```

3. **Install platform dependencies**:

   ```bash
   # macOS
   brew install ncurses
   
   # Ubuntu/Debian
   sudo apt-get install libncurses-dev clang-format clang-tidy
   
   # Fedora/RHEL
   sudo dnf install ncurses-devel clang-tools-extra
   
   # Windows (using vcpkg)
   git clone https://github.com/Microsoft/vcpkg.git
   cd vcpkg && bootstrap-vcpkg.bat
   vcpkg install pdcurses:x64-windows
   ```

4. **Set up pre-commit hooks** (recommended):

   ```bash
   # Install pre-commit
   pip install pre-commit
   
   # Install Node.js for markdownlint
   # (varies by OS - use your preferred method)
   
   # Install the hooks
   pre-commit install
   ```

5. **Alternative: Use Devcontainer** (recommended for consistency):

   This project includes a devcontainer configuration for VS Code that provides a consistent development environment.
   - Install the [Dev Containers extension](https://marketplace.visualstudio.com/items?itemName=ms-vscode-remote.remote-containers) for VS Code
   - Open the project in VS Code
   - When prompted, choose "Reopen in Container"
   - The container will automatically install all dependencies

### Understanding the Build System

This project uses Zig as its build system. If you're new to Zig, see our [Zig Primer for C Developers](docs/ZIG_PRIMER.md).

#### Quick Build Commands

```bash
# Build the project (debug mode)
zig build

# Build with optimizations
zig build -Doptimize=ReleaseSafe

# Run the application
zig build run -- --help

# Run tests
zig build test

# Check code without building
zig build check

# Cross-compile for Windows (from Linux/macOS)
zig build -Dtarget=x86_64-windows

# Install to a custom prefix
zig build install --prefix ~/.local
```

#### Build Options

| Option | Description | Default |
|--------|-------------|---------|
| `-Doptimize=` | Build mode: `Debug`, `ReleaseSafe`, `ReleaseFast`, `ReleaseSmall` | `Debug` |
| `-Dtarget=` | Target triple (e.g., `x86_64-windows`, `aarch64-linux`) | Native |
| `-Denable-tui=` | Enable TUI support | `true` |
| `--prefix` | Installation directory | `zig-out` |

#### Adding New Source Files

1. Add your C file to the appropriate directory under `src/`
2. Update `build.zig` to include the new file:
   ```zig
   exe.addCSourceFiles(.{
       .files = &.{
           // existing files...
           "src/your_module/new_file.c",
       },
       .flags = &.{ "-std=c23", "-Wall", "-Wextra" },
   });
   ```
3. If adding a new module, update the architecture diagram in [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)

#### Build Troubleshooting

**Common Issues:**

- **"Unable to find ncurses"**: Install the development package for your OS (see step 3 above)
- **"C header not found"**: Check that all include paths are added in `build.zig`
- **Cache issues**: Run `rm -rf zig-cache zig-out` and rebuild
- **Windows DLL issues**: Ensure vcpkg bin directory is in PATH

For more details, see the [Architecture Overview](docs/ARCHITECTURE.md).

### Testing

#### Running Tests

```bash
# Run all tests
zig build test

# Run tests with different optimization levels
zig build test -Doptimize=Debug
zig build test -Doptimize=ReleaseSafe
zig build test -Doptimize=ReleaseFast
```

#### Writing Tests

- Write tests for new functionality in `test/` directory
- Ensure all existing tests pass
- Test on multiple platforms if possible
- Test error conditions and edge cases

### Documentation

- Update README.md for user-facing changes
- Add inline comments for complex logic
- Keep examples up to date

## Coding Standards

### C Code Style

- Use 2 spaces for indentation
- Opening braces on same line for functions
- Use descriptive variable names
- Add comments for complex logic
- Keep functions focused and small
- Follow the .clang-format configuration

Example:

```c
app_error process_input(const char* input, size_t len) {
  // Validate input parameters
  if (!input || len == 0) {
    return APP_ERROR_INVALID_ARG;
  }

  // Process the input
  app_buffer_t buffer = {0};
  app_error err = process_data(input, len, &buffer);

  if (err != APP_SUCCESS) {
    app_secure_free(buffer.data, buffer.capacity);
    return err;
  }

  // Clean up
  app_secure_free(buffer.data, buffer.capacity);
  return APP_SUCCESS;
}
```

### Error Handling

- Always check return values
- Provide meaningful error messages
- Clean up resources on error paths
- Use early returns for error conditions

### Memory Management

- Free all allocated memory
- Use secure memory functions for sensitive data
- Check for allocation failures
- Avoid memory leaks in error paths

## Project Structure

```
cli-starter/
├── src/              # Core implementation
│   ├── main.c       # Entry point
│   ├── core/        # Core functionality
│   ├── cli/         # CLI interface
│   ├── io/          # Input/Output
│   └── utils/       # Utilities
├── test/            # Test suite
├── build.zig        # Build configuration
└── build.zig.zon    # Build dependencies
```

## Getting Help

- Check the [documentation](README.md)
- Look through existing issues
- Ask questions in issues with the "question" label

## License

By contributing to this project, you agree that your contributions will be licensed under the MIT License.
