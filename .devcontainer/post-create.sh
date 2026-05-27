#!/usr/bin/env bash
set -euo pipefail

echo "🚀 Setting up development environment..."

# Verify installations
echo "✅ Verifying installations..."
echo "Zig version: $(zig version)"
echo "Clang version: $(clang --version | head -n1)"
echo "CMake version: $(cmake --version | head -n1)"

# Always verify the TUI build itself, then add PTY coverage when Ghostty VT is
# available. libghostty-vt is a Ghostty flake output, not a nixpkgs package, so
# mirror CI's explicit flake build instead of relying on devcontainer Nix attrs.
echo "🔨 Building TUI-enabled project..."
zig build -Denable-tui=true

if ! pkg-config --exists libghostty-vt && command -v nix >/dev/null 2>&1; then
  ghostty_ref="github:ghostty-org/ghostty/2e5ad917eb4e325a3dbb161c3f41208a8cd35e44"
  echo "🔨 Building libghostty-vt from $ghostty_ref..."
  if ghostty_vt_dev=$(nix build --accept-flake-config --no-link --print-out-paths "$ghostty_ref#libghostty-vt.dev"); then
    export PKG_CONFIG_PATH="$ghostty_vt_dev/share/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
    profile_line="export PKG_CONFIG_PATH=$ghostty_vt_dev/share/pkgconfig\${PKG_CONFIG_PATH:+:\$PKG_CONFIG_PATH}"
    grep -qxF "$profile_line" "$HOME/.profile" 2>/dev/null || printf '%s\n' "$profile_line" >>"$HOME/.profile"
  else
    echo "⚠️  Could not build libghostty-vt; continuing with CLI-only setup."
  fi
fi

if pkg-config --exists libghostty-vt; then
  echo "Ghostty VT version: $(pkg-config --modversion libghostty-vt)"
  echo "🧪 Running Ghostty VT terminal tests..."
  zig build -Denable-tui=true -Dterminal-backend=ghostty terminal-test
else
  echo "⚠️  Ghostty VT not detected; skipping PTY terminal tests during setup."
  echo "   Run nix build github:ghostty-org/ghostty/<rev>#libghostty-vt.dev and"
  echo "   add its share/pkgconfig directory to PKG_CONFIG_PATH if you need"
  echo "   'zig build -Denable-tui=true terminal-test' inside the container."
  echo "🧪 Running CLI contract tests..."
  zig build test
fi

echo "✨ Development environment setup complete!"
