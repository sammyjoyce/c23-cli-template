# Justfile for C23 CLI Template
# Quick reference: https://just.systems/man/en/

set positional-arguments

# --- Build ---

# Build the project (TUI enabled by default)
build:
    zig build -Denable-tui=true

# Build without TUI support
build-no-tui:
    zig build

# Build with release optimization (TUI enabled by default)
release:
    zig build -Doptimize=ReleaseSafe -Denable-tui=true

# Build with release optimization without TUI
release-no-tui:
    zig build -Doptimize=ReleaseSafe

# --- Run ---

# Run the application (TUI enabled by default)
run *args:
    zig build -Denable-tui=true run -- {{ args }}

# Run the application without TUI
run-no-tui *args:
    zig build run -- {{ args }}

# --- Test ---

# Run tests
@test:
    zig build test

# Run end-to-end terminal scenario tests (uses current build default)
terminal-test:
    zig build terminal-test

# Run terminal scenarios with TUI explicitly enabled
tui-test:
    zig build -Denable-tui=true terminal-test

# Run tests with different optimization levels
test-debug:
    zig build test -Doptimize=Debug

test-release:
    zig build test -Doptimize=ReleaseSafe

test-fast:
    zig build test -Doptimize=ReleaseFast

# --- Quality ---

# Check code formatting
fmt:
    zig build fmt

# Check code formatting without fixing
fmt-check:
    zig build fmt-check

# Run all checks (format + tests)
check:
    zig build check

# --- Maintenance ---

# Clean build artifacts
clean:
    zig build clean

# --- Help ---

# Display available commands
help:
    @just --list --unsorted
