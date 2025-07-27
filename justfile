# Justfile for C23 CLI Template

# Build the project
build:
    zig build

default: build

# Build with release optimization
release:
    zig build -Doptimize=ReleaseSafe

# Run the application
run:
    zig build run

# Run with arguments
run *args:
    zig build run -- {{args}}

# Run tests
test:
    zig build test

# Run tests with different optimization levels
test-debug:
    zig build test -Doptimize=Debug

test-release:
    zig build test -Doptimize=ReleaseSafe

test-fast:
    zig build test -Doptimize=ReleaseFast

# Check code formatting
fmt:
    zig build fmt

fmt-check:
    zig build fmt-check

# Run all checks
check:
    zig build check

# Clean build artifacts
clean:
    zig build clean

# Install dependencies (if any)
install:
    # No additional installation needed for this template
    echo "No additional installation needed"

# Generate documentation
docs:
    # Generate documentation using your preferred tool
    echo "Documentation generation not configured"

# Display help
help:
    echo "Available commands:"
    echo "  build       - Build the project"
    echo "  release     - Build with release optimization"
    echo "  run         - Run the application"
    echo "  test        - Run tests"
    echo "  test-debug  - Run tests with debug optimization"
    echo "  test-release - Run tests with release optimization"
    echo "  test-fast   - Run tests with fast optimization"
    echo "  fmt         - Format code"
    echo "  fmt-check   - Check code formatting"
    echo "  check       - Run all checks"
    echo "  clean       - Clean build artifacts"
    echo "  install     - Install dependencies"
    echo "  docs        - Generate documentation"
    echo "  help        - Display this help message"
