# Justfile for C23 CLI Template
# Quick reference: https://just.systems/man/en/

set positional-arguments

# --- Build ---

# Build the default CLI
build:
    zig build

# Build with TUI support
build-tui:
    zig build -Denable-tui=true

# Build with release optimization
release:
    zig build -Doptimize=ReleaseSafe

# Build with release optimization and TUI support
release-tui:
    zig build -Doptimize=ReleaseSafe -Denable-tui=true

# Build the reusable TUI menu static library
tui-menu-lib:
    zig build tui-menu-lib

# --- Run ---

# Run the application
run *args:
    zig build run -- {{ args }}

# Run the application with TUI support
run-tui *args:
    zig build -Denable-tui=true run -- {{ args }}

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

# Require Ghostty VT-backed TUI scenarios
tui-test-ghostty:
    zig build -Denable-tui=true -Dterminal-backend=ghostty terminal-test

# Require Ghostty VT-backed TUI scenarios inside the Nix shell
nix-test-ghostty:
    nix develop --command zig build -Denable-tui=true -Dterminal-backend=ghostty terminal-test

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
