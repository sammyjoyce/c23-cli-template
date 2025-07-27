#!/bin/bash
set -e

echo "🚀 Setting up development environment..."

# Install Zig
ZIG_VERSION="0.14.1"
echo "📦 Installing Zig ${ZIG_VERSION}..."
cd /tmp
wget -q https://ziglang.org/download/${ZIG_VERSION}/zig-linux-x86_64-${ZIG_VERSION}.tar.xz
tar -xf zig-linux-x86_64-${ZIG_VERSION}.tar.xz
sudo mv zig-linux-x86_64-${ZIG_VERSION} /opt/zig
sudo ln -sf /opt/zig/zig /usr/local/bin/zig
rm zig-linux-x86_64-${ZIG_VERSION}.tar.xz

# Verify installations
echo "✅ Verifying installations..."
echo "Zig version: $(zig version)"
echo "Clang version: $(clang --version | head -n1)"
echo "CMake version: $(cmake --version | head -n1)"

# Install any project dependencies
if [ -f "requirements.txt" ]; then
    echo "📦 Installing Python dependencies..."
    pip install -r requirements.txt
fi

# Build the project
echo "🔨 Building project..."
zig build

echo "✨ Development environment setup complete!"