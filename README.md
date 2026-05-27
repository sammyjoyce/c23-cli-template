# C23 TUI + CLI Starter Template - Zig + ncurses

[![GitHub Release](https://img.shields.io/github/v/release/sammyjoyce/c23-cli-template?style=for-the-badge)](https://github.com/sammyjoyce/c23-cli-template)
[![License](https://img.shields.io/github/license/sammyjoyce/c23-cli-template?style=for-the-badge)](https://github.com/sammyjoyce/c23-cli-template/blob/main/LICENSE)
[![CI Status](https://img.shields.io/github/actions/workflow/status/sammyjoyce/c23-cli-template/ci.yaml?style=for-the-badge&label=CI)](https://github.com/sammyjoyce/c23-cli-template/actions/workflows/ci.yaml)
[![Zig](https://img.shields.io/badge/Zig-0.16.0-F7A41D?style=for-the-badge&logo=zig)](https://ziglang.org/)
[![Build](https://img.shields.io/github/actions/workflow/status/sammyjoyce/c23-cli-template/ci.yaml?style=for-the-badge&label=Build)](https://github.com/sammyjoyce/c23-cli-template/actions/workflows/ci.yaml)
[![OpenSSF Scorecard](https://img.shields.io/ossf-scorecard/github.com/sammyjoyce/c23-cli-template?style=for-the-badge&label=OpenSSF%20Scorecard)](https://securityscorecards.dev/viewer/?uri=github.com/sammyjoyce/c23-cli-template)

## A ready-to-use C23 starter for command-line tools and terminal UIs

[Use this template](https://github.com/sammyjoyce/c23-cli-template/generate) • [View Demo](https://github.com/sammyjoyce/c23-cli-template) • [Report Bug](https://github.com/sammyjoyce/c23-cli-template/issues)

---

## ✨ Features

- 🚀 **Modern C23** - Latest C standard through Zig's bundled C toolchain
- ⚡ **Zig Build System** - Fast, reliable builds with cross-compilation
- 🏗️ **Well-Structured** - Organized project layout ready for growth
- 🧪 **Testing Included** - C23 CLI contract tests plus PTY terminal scenario tests for CLI/TUI flows
- 🎨 **Smart CLI** - Colored output, help text, argument parsing
- 🖼️ **TUI Support** - ncurses/PDCurses integration for interactive terminal UIs
- 🔧 **Configuration** - Layered config system (file → env → args)
- 📦 **Minimal Dependencies** - Zig and libc by default; curses only for TUI builds
- 🤖 **CI/CD Ready** - GitHub Actions workflow included
- ⚡ **Caching** - Speeds up builds by caching Zig dependencies and build artifacts
- 🔒 **Release Gating** - Ensures releases only happen on tags in main branch
- 🛡️ **Security Scanning** - Gitleaks, OpenSSF Scorecard, and SBOM generation
- 📦 **Artifact Management** - Unique artifact naming to avoid collisions
- 🏷️ **Version Pinning** - Pinned GitHub Actions versions for reproducibility
- 🚫 **Concurrency Control** - Cancels redundant CI runs on same branch
- 📊 **Dynamic Binary Naming** - Extracts binary name from build.zig.zon
- 🧹 **Template Cleanup** - Scripted cleanup of template-specific files and placeholders
- 📚 **OpenCLI Compliant** - Standardized CLI behavior
- 🔄 **Dependency Updates** - Automated updates with Dependabot/Renovate
- 📝 **Markdown Linting** - Documentation checks in local tooling and CI
- 🐳 **Devcontainer Support** - Consistent development environments
- 📋 **Comprehensive Documentation** - Detailed guides and examples

## 🎯 Quick Start

### Create Your Project

### Option 1: GitHub UI

1. Click ["Use this template"](https://github.com/sammyjoyce/c23-cli-template/generate)
2. Name your repository
3. Click "Create repository"

### Option 2: GitHub CLI

```bash
gh repo create my-cli \
  --template sammyjoyce/c23-cli-template \
  --public \
  --clone
```

### Build & Run

```bash
# Clone your new repo
git clone https://github.com/YOU/YOUR-REPO
cd YOUR-REPO

# Build the default CLI starter
zig build -Doptimize=ReleaseSafe

# Build with the optional ncurses/PDCurses TUI
zig build -Doptimize=ReleaseSafe -Denable-tui=true

# Run
./zig-out/bin/YOUR-REPO --help
```

## 📖 What's Included

### Project Structure

```text
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
│   ├── tui/                # ncurses windows, menus, dialogs, progress bars
│   └── utils/              # Utilities
├── test/                   # C23 CLI tests + Ghostty VT terminal scenarios
│   ├── cli_contract_runner.c # C23 CLI contract tests
│   ├── cli_contract_helpers.c # Shared C integration-test helpers
│   └── terminal_vt_*       # C PTY/TUI scenario harness
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
Version: 0.1.0
Build: reproducible

$ myapp --json info
{"format_version":"1.0", ...}

$ myapp doctor
myapp doctor
  binary        ok (myapp 0.1.0)

# Interactive TUI showcase
$ zig build -Denable-tui=true run -- menu
# Opens ncurses menus, dialogs, panels, and progress bars
```

## 🛠️ Customization Guide

### 1. After Creating Your Repo

After creating your repository, run the template cleanup workflow or local setup script to:

- ✅ Replaces `myapp` with your project name
- ✅ Updates all references and metadata
- ✅ Preserves template structure
- ✅ Removes template-specific files
- ✅ Commit the changes

Check the **Actions** tab to see progress.

### 2. CI Runner Selection

Generated repositories default to GitHub-hosted runners so the cleanup workflow and first CI run work without extra infrastructure.
To opt into Namespace or another self-hosted fleet, configure `CI_LINUX_RUNNER`, `CI_MACOS_RUNNER`, and `CI_WINDOWS_RUNNER` as described in [Using This Template](.template/TEMPLATE_USAGE.md#ci-runner-selection).

### 3. Add Your Commands

Edit `src/main.c`:

```c
if (strcmp(command, "deploy") == 0) {
    printf("Deploying application...\n");
    // Your deployment logic
    return APP_SUCCESS;
}
```

### 4. Update Help Text

Edit `src/cli/help.c` to describe your commands.

### 5. Add Source Files

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

- **Zig 0.16.0** (current stable release) - Install via [zvm](https://github.com/tristanisham/zvm)
- **C compiler** - For system libraries
- **NCurses** - Optional; required only for `-Denable-tui=true`
  - Ubuntu/Debian: `sudo apt-get install libncurses-dev`
  - macOS: `brew install ncurses`
  - Fedora: `sudo dnf install ncurses-devel`
- **[libghostty-vt](https://libghostty.tip.ghostty.org/index.html) tip/development API** - Optional outside Nix; enables the C Ghostty VT backend for PTY-backed terminal tests

### Development Environment

This template provides several tools to enhance your development experience:

- **Devcontainer Support** - Pre-configured development environment with all dependencies
- **Nix Dev Shell** - Includes Zig, C tooling, nixpkgs `libghostty-vt` for terminal tests, and markdown lint tooling
- **CI Quality Checks** - Automated build, test, lint, security, and release checks

### Commands

```bash
# Build
zig build                    # Debug build
zig build -Doptimize=ReleaseSafe  # Release build
zig build -Denable-tui=true  # Build with the TUI showcase

# Test
zig build test              # Run fast C23 CLI contract tests
zig build terminal-test     # Run terminal scenarios with the selected backend
zig build -Denable-tui=true terminal-test  # Run TUI scenarios through Ghostty VT when available
zig build -Dterminal-backend=ghostty terminal-test  # Require the C Ghostty VT test backend

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

Config files are flat JSON objects with boolean keys for `debug`, `quiet`,
`verbose`, `no_color`, `json_output`, and `plain_output`.

## 📚 Documentation

### Getting Started

- 📖 [**Using This Template**](/.template/TEMPLATE_USAGE.md) - Detailed setup guide
- 🚀 [**Quick Start Guide**](#-quick-start) - Get up and running quickly
- 🔧 [**Prerequisites**](#prerequisites) - Platform-specific requirements

### Developer Resources

- 🏗️ [**Architecture Overview**](docs/ARCHITECTURE.md) - System design and module structure
- ⚡ [**Zig Primer for C Developers**](docs/ZIG_PRIMER.md) - Understanding the build system
- 🧪 [**Testing CLI And TUI Behavior**](docs/TESTING.md) - End-to-end terminal scenario tests
- 🤝 [**Contributing Guide**](CONTRIBUTING.md) - How to contribute to the project
- 🧪 [**Advanced Usage Examples**](examples/advanced-usage.md) - Piping, scripting, and integration

### Examples & Demos

- 📝 [**Adding Commands**](examples/adding-a-command.md) - Extend the CLI
- 🎨 [**Custom TUI Components**](examples/custom-tui.md) - Build interactive interfaces
- ⚙️ [**Configuration Guide**](examples/config.json) - Config file examples

- 🎬 [**Demo Gallery**](docs/demos/README.md) - Animated demonstrations

### Project Information

- 🛡️ [**Security Policy**](SECURITY.md) - Reporting vulnerabilities
- 📋 [**Code of Conduct**](CODE_OF_CONDUCT.md) - Community guidelines
- 📝 [**Changelog**](CHANGELOG.md) - Version history
- 📜 [**License**](LICENSE) - MIT License

## 🤔 Why This Stack?

- **C23** - Latest features: `typeof`, `_BitInt`, better type safety
- **Zig Build** - Superior to Make/CMake, built-in cross-compilation
- **ncurses/PDCurses** - Proven terminal UI primitives with a small wrapper API
- **Minimal Dependencies** - Zig and libc by default, with curses behind `-Denable-tui=true`

## 🆘 Getting Help

### Template Issues

For problems with the template itself:

- Check [existing issues](https://github.com/sammyjoyce/c23-cli-template/issues)
- Create a new issue
- Read [template support](/.template/TEMPLATE_SUPPORT.md)

### Your Project Issues

For issues with your generated project:

- Use your own repository's issues
- Check Zig [documentation](https://ziglang.org/documentation/)
- See C23 [reference](https://en.cppreference.com/w/c/23)

## 🌟 Projects Using This Template

> Using this template? [Add your project!](https://github.com/sammyjoyce/c23-cli-template/edit/main/README.md)

- [Example CLI](https://github.com/example/cli) - Description
- Your project here!

## 📄 License

This template is MIT licensed. See [LICENSE](LICENSE) for details.

When you use this template, you can choose any license for your project.

---

**Ready to build your CLI app?**

[![Use this template](https://img.shields.io/badge/Use%20this-template-success?style=for-the-badge&logo=github)](https://github.com/sammyjoyce/c23-cli-template/generate)

Made with ❤️ by the open source community
