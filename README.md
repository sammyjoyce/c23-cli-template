# C23 TUI + CLI Starter Template - Zig + ncurses

[![GitHub Release](https://img.shields.io/github/v/release/sammyjoyce/c23-cli-template?style=for-the-badge)](https://github.com/sammyjoyce/c23-cli-template)
[![License](https://img.shields.io/github/license/sammyjoyce/c23-cli-template?style=for-the-badge)](https://github.com/sammyjoyce/c23-cli-template/blob/main/LICENSE)
[![CI Status](https://img.shields.io/github/actions/workflow/status/sammyjoyce/c23-cli-template/ci.yaml?style=for-the-badge&label=CI)](https://github.com/sammyjoyce/c23-cli-template/actions/workflows/ci.yaml)
[![Zig](https://img.shields.io/badge/Zig-0.16.0-F7A41D?style=for-the-badge&logo=zig)](https://ziglang.org/)
[![OpenSSF Scorecard](https://img.shields.io/ossf-scorecard/github.com/sammyjoyce/c23-cli-template?style=for-the-badge&label=OpenSSF%20Scorecard)](https://securityscorecards.dev/viewer/?uri=github.com/sammyjoyce/c23-cli-template)

A ready-to-use C23 starter for command-line tools and terminal UIs. Click **Use this
template**, run the cleanup script, and you have a cross-compiling C project with
argument parsing, a default ncurses TUI, end-to-end tests, and a GitHub
Actions CI/CD scaffold.

[Use this template](https://github.com/sammyjoyce/c23-cli-template/generate) • [Read the setup guide](.template/TEMPLATE_USAGE.md) • [Report a template issue](https://github.com/sammyjoyce/c23-cli-template/issues)

---

## Highlights

- **Modern C23, no compiler wrangling** - The latest C standard through Zig's bundled toolchain.
- **CLI and TUI in one** - Argument parsing and colored output, plus a default ncurses/PDCurses interface (`-Denable-tui=false` disables it).
- **Agent-ready headless mode** - Bare non-TTY launches read a JSON request from stdin, and piped command output defaults to versioned JSON.
- **Fast, cross-compiling builds** - The Zig build system replaces Make/CMake and targets other platforms out of the box.
- **Tested end to end** - Three test layers: in-process unit tests, C23 CLI contract tests, and PTY-driven terminal scenarios for real CLI/TUI behavior.
- **CI/CD scaffold included** - GitHub Actions for builds, tests, linting, release artifacts, and optional security checks.
- **OpenCLI-style contract** - A checked-in machine-readable CLI contract your tool can print on demand with `myapp opencli`.

## Quick Start

### Create your project

#### Option 1: GitHub UI

1. Click ["Use this template"](https://github.com/sammyjoyce/c23-cli-template/generate)
2. Name your repository
3. Click "Create repository"

#### Option 2: GitHub CLI

```bash
gh repo create my-cli \
  --template sammyjoyce/c23-cli-template \
  --public \
  --clone
```

### Build and run

```bash
# Clone your new repo
git clone https://github.com/YOU/YOUR-REPO
cd YOUR-REPO

# Build the default CLI + TUI starter
zig build -Doptimize=ReleaseSafe

# Build without the ncurses/PDCurses TUI
zig build -Doptimize=ReleaseSafe -Denable-tui=false

# Run (the default binary is named `myapp`; override with `-Dapp-name=`)
./zig-out/bin/myapp --help
```

## Example Commands

The template ships with working commands so you can confirm the build immediately:

```bash
# Greeting command
$ myapp hello
Hello, World!      # on a TTY
# {"format_version":"1.0","message":"Hello, World!"} when stdout is piped

$ myapp hello Alice
Hello, Alice!

# Echo command
$ myapp echo Hello from CLI
Hello from CLI

# Info command
$ myapp info
Application: myapp
Version: 0.1.0
Build: omitted

$ myapp --json info
{"format_version":"1.0", ...}

$ myapp doctor
myapp doctor
  binary        ok (myapp 0.1.0)

$ myapp opencli
{
  "opencli": "0.1",
  ...
}

# Interactive TUI showcase
$ myapp
# Opens ncurses menus, dialogs, panels, and progress bars

# Headless request/response mode
$ printf '{"command":"hello","args":["Alice"]}' | myapp
{"format_version":"1.0","message":"Hello, Alice!"}
```

## Project Layout

See [docs/ARCHITECTURE.md#module-map](docs/ARCHITECTURE.md#module-map) for what each directory under `src/` owns, plus the representative public functions in each module.

## Customize It

### 1. After creating your repo

Run the template cleanup workflow or local setup script. It will:

- Replace `myapp` with your project name
- Update all references and metadata
- Preserve the template structure
- Remove template-specific files
- Commit the changes

Check the **Actions** tab to see progress.

### 2. CI runner selection

Generated repositories default to GitHub-hosted runners so the cleanup workflow and first CI run work without extra infrastructure.
To opt into Namespace or another self-hosted fleet, configure `CI_LINUX_RUNNER`, `CI_MACOS_RUNNER`, and `CI_WINDOWS_RUNNER` as described in [Using This Template](.template/TEMPLATE_USAGE.md#ci-runner-selection).

### 3. Add a command

Commands are table-driven. Write a handler, register it in `src/cli/commands.c`,
regenerate `opencli.json`, add a contract test, and add the new file to
`base_sources` in `build.zig`. The full five-step flow is in
[examples/adding-a-command.md](examples/adding-a-command.md).

Help text and the OpenCLI contract update automatically from the command table;
you do not edit `help.c` by hand.

For TUI screens, see [examples/custom-tui.md](examples/custom-tui.md).

## Develop

### Prerequisites

Default builds need:

- **Zig 0.16.0** - the version pinned by this template; install via [zvm](https://github.com/tristanisham/zvm) or your package manager
- **A system C toolchain** - for libc and platform libraries
- **Curses development files** - the TUI is compiled in by default; pass `-Denable-tui=false` for CLI/headless-only builds.

- Ubuntu/Debian: `sudo apt-get install pkg-config libncurses-dev`
- macOS: `brew install pkg-config ncurses`
- Fedora: `sudo dnf install pkg-config ncurses-devel`
- Windows: `vcpkg install pdcurses:x64-windows`

Optional PTY-backed TUI scenarios need [libghostty-vt](https://libghostty.tip.ghostty.org/index.html) development files discoverable through `pkg-config`, or the Nix dev shell.

### Development environment

This template provides several tools to enhance your development experience:

- **Devcontainer Support** - Pre-configured non-Nix development environment with Zig and platform packages
- **Optional Nix Dev Shell** - Convenience shell with Zig, C tooling, curses, Ghostty VT, and markdown lint tooling
- **CI Quality Checks** - Automated build, test, lint, security, and release checks without requiring Nix by default

### Commands

```bash
# Build
zig build                                  # Debug build
zig build -Doptimize=ReleaseSafe           # Release build
zig build -Denable-tui=false               # Build without the ncurses/PDCurses TUI
zig build -Dcurses-prefix="$(brew --prefix ncurses)"  # macOS/Homebrew TUI
zig build tui-menu-lib                     # Build the reusable TUI menu static library

# Test
zig build unit-test                        # In-process unit tests
zig build test                             # Unit tests + CLI contract tests
zig build terminal-test                    # Unit + CLI tests; PTY/TUI skipped unless TUI + backend are available
zig build -Dterminal-backend=ghostty terminal-test  # Require Ghostty VT
zig build -Dterminal-backend=none terminal-test  # Never run PTY/TUI scenarios
zig build check                            # fmt-check + tests (the CI gate)

# Format
zig build fmt                              # Format build.zig (Zig formatter; C uses clang-format via pre-commit + CI)

# Clean
zig build clean                            # Remove zig-out and .zig-cache
```

### Configuration

Your app supports config from multiple sources:

1. **CLI arguments** (highest priority)
2. **Environment variables**
3. **Config file** (`~/.config/yourapp/config.json`)
4. **Defaults**

Config files are flat JSON objects with boolean keys for `debug`, `quiet`,
`verbose`, `no_color`, `json_output`, and `plain_output`.

## Everything Included

The template wires up far more than the starter code. The full inventory:

### Language and build

- **Modern C23** - Latest C standard through Zig's bundled C toolchain
- **Zig Build System** - Fast, reliable builds with cross-compilation
- **Configurable Dependencies** - Zig/libc plus curses by default; `-Denable-tui=false` removes the TUI dependency
- **Configurable binary name** - Set via `-Dapp-name=` (default `myapp`)

### CLI and TUI

- **Smart CLI** - Colored output, help text, argument parsing
- **TUI Support** - ncurses/PDCurses integration for interactive terminal UIs
- **Reusable TUI Menu** - Optional `tui-menu-lib` target for downstream C apps
- **Configuration** - Layered config system (file → env → args)
- **OpenCLI-style contract** - Standardized CLI metadata under `myapp opencli`
- **Live CLI Contract** - `myapp opencli` prints the checked-in OpenCLI spec

### Testing and quality

- **Testing Included** - In-process unit tests, C23 CLI contract tests, and optional PTY terminal scenarios for CLI/TUI flows
- **Markdown Linting** - Documentation checks in local tooling and CI

### CI/CD and releases

- **CI/CD Ready** - GitHub Actions workflow included
- **Caching** - Speeds up builds by caching Zig dependencies
- **Concurrency Control** - Cancels redundant CI runs on same branch
- **Release Gating** - Ensures releases only happen on version tags after required quality jobs pass
- **Artifact Management** - Unique artifact naming to avoid collisions
- **Security Scanning** - Gitleaks, CodeQL, OpenSSF Scorecard, and SBOM generation scaffolds
- **Dependency Updates** - Automated updates with Dependabot/Renovate

### Project and developer experience

- **Well-Structured** - Organized project layout ready for growth
- **Template Cleanup** - Scripted cleanup of template-specific files and placeholders
- **Devcontainer Support** - Consistent development environments
- **Comprehensive Documentation** - Detailed guides and examples

## Why this stack?

- **C23** - Current standard mode with `nullptr`, attributes, and other portable language improvements
- **Zig Build** - Superior to Make/CMake, built-in cross-compilation
- **ncurses/PDCurses** - Proven terminal UI primitives with a small wrapper API
- **Configurable Dependencies** - Zig/libc plus curses by default; `-Denable-tui=false` builds without the TUI dependency

## Documentation

Start with [**Using This Template**](.template/TEMPLATE_USAGE.md) for the full setup guide.

### Developer resources

- [**Architecture Overview**](docs/ARCHITECTURE.md) - System design and module structure
- [**Public Contracts**](docs/CONTRACTS.md) - Supported CLI and TUI seams
- [**Zig Primer for C Developers**](docs/ZIG_PRIMER.md) - Understanding the build system
- [**Testing CLI And TUI Behavior**](docs/TESTING.md) - End-to-end terminal scenario tests
- [**Contributing Guide**](CONTRIBUTING.md) - How to contribute to the project
- [**Advanced Usage Examples**](examples/advanced-usage.md) - Piping, scripting, and integration

### Examples and demos

- [**Adding Commands**](examples/adding-a-command.md) - Extend the CLI
- [**Custom TUI Components**](examples/custom-tui.md) - Build interactive interfaces
- [**Configuration Guide**](examples/config.json) - Config file examples
- [**Demo Gallery**](docs/demos/README.md) - Animated demonstrations

### Project information

- [**Security Policy**](SECURITY.md) - Reporting vulnerabilities
- [**Code of Conduct**](CODE_OF_CONDUCT.md) - Community guidelines
- [**Changelog**](CHANGELOG.md) - Version history
- [**License**](LICENSE) - MIT License

## Getting Help

### Template issues

For problems with the template itself:

- Check [existing issues](https://github.com/sammyjoyce/c23-cli-template/issues)
- Create a new issue
- Read [template support](.template/TEMPLATE_SUPPORT.md)

### Your project issues

For issues with your generated project:

- Use your own repository's issues
- Check Zig [documentation](https://ziglang.org/documentation/)
- See C23 [reference](https://en.cppreference.com/w/c/23)

## License

This template is MIT licensed. See [LICENSE](LICENSE) for details.

When you use this template, you can choose any license for your project.

---

**Ready to build your CLI app?**

[![Use this template](https://img.shields.io/badge/Use%20this-template-success?style=for-the-badge&logo=github)](https://github.com/sammyjoyce/c23-cli-template/generate)
