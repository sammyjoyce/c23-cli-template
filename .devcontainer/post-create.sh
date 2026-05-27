#!/usr/bin/env bash
set -euo pipefail

echo "🚀 Setting up development environment..."

# Verify installations
echo "✅ Verifying installations..."
echo "Zig version: $(zig version)"
echo "Clang version: $(clang --version | head -n1)"
echo "CMake version: $(cmake --version | head -n1)"
echo "Ghostty VT version: $(pkg-config --modversion libghostty-vt)"

# Verify the recommended TUI terminal-test workflow.
echo "🧪 Running Ghostty VT terminal tests..."
zig build -Denable-tui=true -Dterminal-backend=ghostty terminal-test

echo "✨ Development environment setup complete!"
