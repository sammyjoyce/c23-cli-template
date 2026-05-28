#!/usr/bin/env bash
set -euo pipefail

echo "🚀 Setting up development environment..."

# Verify installations
echo "✅ Verifying installations..."
echo "Zig version: $(zig version)"
echo "Clang version: $(clang --version | head -n1)"
echo "CMake version: $(cmake --version | head -n1)"

# Keep the default container path Nix-free: Zig + platform packages are enough
# for the baseline CLI and TUI build. PTY-backed terminal scenarios run only
# when libghostty-vt is already installed and discoverable by pkg-config.
echo "🔨 Building default CLI..."
zig build

echo "🔨 Building TUI-enabled project..."
zig build -Denable-tui=true

echo "🧪 Running terminal-test in auto mode..."
zig build -Denable-tui=true terminal-test

echo "✨ Development environment setup complete!"
