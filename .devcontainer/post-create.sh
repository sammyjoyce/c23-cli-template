#!/bin/bash
set -e

echo "🚀 Setting up development environment..."

# Verify installations
echo "✅ Verifying installations..."
echo "Zig version: $(zig version)"
echo "Clang version: $(clang --version | head -n1)"
echo "CMake version: $(cmake --version | head -n1)"

# Build the project
echo "🔨 Building project..."
zig build

echo "✨ Development environment setup complete!"
