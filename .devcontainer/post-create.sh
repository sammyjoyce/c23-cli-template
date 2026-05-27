#!/usr/bin/env bash
set -euo pipefail

echo "🚀 Setting up development environment..."

# Verify installations
echo "✅ Verifying installations..."
echo "Zig version: $(zig version)"
echo "Clang version: $(clang --version | head -n1)"
echo "CMake version: $(cmake --version | head -n1)"

# Always verify the TUI build itself, then add PTY coverage when Ghostty VT is
# available in the devcontainer.
echo "🔨 Building TUI-enabled project..."
zig build -Denable-tui=true

if pkg-config --exists libghostty-vt; then
  echo "Ghostty VT version: $(pkg-config --modversion libghostty-vt)"
  echo "🧪 Running Ghostty VT terminal tests..."
  zig build -Denable-tui=true -Dterminal-backend=ghostty terminal-test
else
  echo "⚠️  Ghostty VT not detected; skipping PTY terminal tests during setup."
  echo "   Rebuild the devcontainer or verify the Nix feature if you need"
  echo "   'zig build -Denable-tui=true terminal-test' inside the container."
  echo "🧪 Running CLI contract tests..."
  zig build test
fi

echo "✨ Development environment setup complete!"
