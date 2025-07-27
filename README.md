# CLI Starter Template - C23 + Zig 🚀

<div align="center">

[![Use this template](https://img.shields.io/badge/Use%20this-template-success?style=for-the-badge&logo=github)](https://github.com/yourusername/maxprofiles/generate)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue?style=for-the-badge)](https://opensource.org/licenses/MIT)
[![C23](https://img.shields.io/badge/C-23-00599C?style=for-the-badge&logo=c)](https://en.wikipedia.org/wiki/C23)
[![Zig](https://img.shields.io/badge/Zig-master-F7A41D?style=for-the-badge&logo=zig)](https://ziglang.org/)

**A modern C23 CLI application starter template with Zig build system**

[**Use this template**](https://github.com/yourusername/maxprofiles/generate) • [**View Demo**](https://github.com/yourusername/maxprofiles) • [**Report Bug**](https://github.com/yourusername/maxprofiles/issues)

</div>

---

## ✨ Features

- 🚀 **Modern C23** - Latest C standard with Aro compiler
- ⚡ **Zig Build System** - Fast, reliable builds with cross-compilation
- 🏗️ **Well-Structured** - Organized project layout ready for growth
- 🧪 **Testing Included** - Test framework with examples
- 🎨 **Smart CLI** - Colored output, help text, argument parsing
- 🖼️ **TUI Support** - NCurses integration for interactive terminal UIs
- 🔧 **Configuration** - Layered config system (file → env → args)
- 📦 **Minimal Dependencies** - Only Zig, libc, and ncurses
- 🤖 **CI/CD Ready** - GitHub Actions workflow included
- 📚 **OpenCLI Compliant** - Standardized CLI behavior

## 🎯 Quick Start

### Create Your Project

<table>
<tr>
<td>

**Option 1: GitHub UI**

1. Click ["Use this template"](https://github.com/yourusername/maxprofiles/generate)
2. Name your repository
3. Click "Create repository"

</td>
<td>

**Option 2: GitHub CLI**

```bash
gh repo create my-cli \
  --template yourusername/maxprofiles \
  --public \
  --clone
```

</td>
</tr>
</table>

### Build & Run

```bash
# Clone your new repo
git clone https://github.com/YOU/YOUR-REPO
cd YOUR-REPO

# Build (with TUI support)
zig build -Doptimize=ReleaseSafe

# Build without TUI (if ncurses is not available)
zig build -Doptimize=ReleaseSafe -Denable-tui=false

# Run
./zig-out/bin/YOUR-REPO --help
```

## 📖 What's Included

### Project Structure
```
your-cli/
├── src/
│   ├── main.c              # Entry point
│   ├── core/               # Core functionality
│   │   ├── config.c/h      # Configuration
│   │   ├── error.c/h       # Error handling
│   │   └── types.h         # Type definitions
│   ├── cli/                # CLI interface
│   │   ├── args.c/h        # Argument parsing
│   │   └── help.c/h        # Help text
│   ├── io/                 # Input/Output
│   └── utils/              # Utilities
├── test/                   # Test suite
├── build.zig               # Build config
└── opencli.json            # CLI specification
```

### Example Commands

The template includes working examples:

```bash
# Greeting command
$ myapp hello
Hello, World!

$ myapp hello Alice
Hello, Alice!

# Echo command
$ myapp echo Hello from CLI
Hello from CLI

# Info command
$ myapp info
Application: myapp
Version: 1.0.0
Build: Jul 27 2025 13:16:11

# Interactive TUI menu
$ myapp menu
# Opens an ncurses-based interactive menu
```

## 🛠️ Customization Guide

### 1. After Creating Your Repo

The template automatically:
- ✅ Replaces `myapp` with your repo name
- ✅ Updates all placeholders
- ✅ Removes template-specific files
- ✅ Commits the changes

Check the **Actions** tab to see progress.

### 2. Add Your Commands

Edit `src/main.c`:

```c
if (strcmp(command, "deploy") == 0) {
    printf("Deploying application...\n");
    // Your deployment logic
    return APP_SUCCESS;
}
```

### 3. Update Help Text

Edit `src/cli/help.c` to describe your commands.

### 4. Add Source Files

1. Create your `.c` file in `src/`
2. Add to `build.zig`:

```zig
const c_sources = [_][]const u8{
    // ... existing files ...
    "src/features/deploy.c",  // Your new file
};
```

## 🧪 Development

### Prerequisites

- **Zig** (master branch) - Install via [zvm](https://github.com/tristanisham/zvm)
- **C compiler** - For system libraries
- **NCurses** - For TUI support
  - Ubuntu/Debian: `sudo apt-get install libncurses-dev`
  - macOS: `brew install ncurses`
  - Fedora: `sudo dnf install ncurses-devel`

### Commands

```bash
# Build
zig build                    # Debug build
zig build -Doptimize=ReleaseSafe  # Release build

# Test
zig build test              # Run all tests

# Clean
zig build clean             # Remove build artifacts

# Format
zig fmt build.zig          # Format build file
```

### Configuration

Your app supports config from multiple sources:

1. **CLI arguments** (highest priority)
2. **Environment variables**
3. **Config file** (`~/.config/yourapp/config.json`)
4. **Defaults**

## 📚 Documentation

- 📖 [**Using This Template**](USING_THIS_TEMPLATE.md) - Detailed setup guide
- 🤝 [**Contributing**](CONTRIBUTING.md) - Contribution guidelines
- 📜 [**License**](LICENSE) - MIT License

## 🤔 Why This Stack?

- **C23** - Latest features: `typeof`, `_BitInt`, better type safety
- **Zig Build** - Superior to Make/CMake, built-in cross-compilation
- **Aro Compiler** - Better C23 support than most system compilers
- **Minimal Dependencies** - Just Zig and libc, no complex toolchains

## 🆘 Getting Help

<table>
<tr>
<td>

**Template Issues**

For problems with the template itself:
- Check [existing issues](https://github.com/yourusername/maxprofiles/issues)
- Create a new issue
- Read [template support](/.github/TEMPLATE_SUPPORT.md)

</td>
<td>

**Your Project Issues**

For issues with your generated project:
- Use your own repository's issues
- Check Zig [documentation](https://ziglang.org/documentation/)
- See C23 [reference](https://en.cppreference.com/w/c/23)

</td>
</tr>
</table>

## 🌟 Projects Using This Template

> Using this template? [Add your project!](https://github.com/yourusername/maxprofiles/edit/main/README.md)

- [Example CLI](https://github.com/example/cli) - Description
- Your project here!

## 📄 License

This template is MIT licensed. See [LICENSE](LICENSE) for details.

When you use this template, you can choose any license for your project.

---

<div align="center">

**Ready to build your CLI app?**

[![Use this template](https://img.shields.io/badge/Use%20this-template-success?style=for-the-badge&logo=github)](https://github.com/yourusername/maxprofiles/generate)

Made with ❤️ by the open source community

</div>