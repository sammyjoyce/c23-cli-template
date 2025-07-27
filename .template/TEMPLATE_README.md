# myapp

A modern C23 CLI application built with Zig build system and Aro compiler.

## Features

- Modern C23 with Aro compiler for better compatibility
- Zig build system for fast, reliable builds
- NCurses TUI support for interactive interfaces
- Secure memory management
- Structured logging with configurable levels
- Configuration system (files, environment, command-line)
- Color output with smart terminal detection
- Comprehensive error handling
- Testing framework
- OpenCLI compliant

## Quick Start

### Prerequisites

- Zig (master branch recommended): Install via [zvm](https://github.com/tristanisham/zvm)
- C compiler (for system libraries)

### Build

```bash
zig build -Doptimize=ReleaseSafe
```

### Run

```bash
./zig-out/bin/myapp --help
```

### Test

```bash
zig build test
```

## Usage

```bash
# Show help
myapp --help

# Show version
myapp --version

# Example commands
myapp hello
myapp hello Alice
myapp echo Hello World
myapp info
```

## Development

### Project Structure

```
.
├── src/
│   ├── main.c              # Application entry point
│   ├── core/               # Core functionality
│   │   ├── config.c/h      # Configuration management
│   │   ├── error.c/h       # Error handling
│   │   └── types.h         # Type definitions
│   ├── cli/                # CLI interface
│   │   ├── args.c/h        # Argument parsing
│   │   └── help.c/h        # Help text
│   ├── io/                 # Input/Output
│   │   ├── input.c/h       # Input handling
│   │   └── output.c/h      # Output formatting
│   └── utils/              # Utilities
│       ├── colors.c/h      # Terminal colors
│       ├── logging.c/h     # Logging system
│       └── memory.c/h      # Secure memory
├── test/                   # Test suite
├── build.zig               # Build configuration
└── build.zig.zon           # Build dependencies
```

### Adding Commands

Add new commands in `src/main.c`:

```c
if (strcmp(command, "mycommand") == 0) {
    // Handle your command
    return APP_SUCCESS;
}
```

### Configuration

Configuration precedence (highest to lowest):
1. Command-line arguments
2. Environment variables
3. Configuration file
4. Default values

## License

MIT