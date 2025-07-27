# Architecture Overview

This document provides a comprehensive overview of the C23 CLI Template architecture, designed to help new contributors understand the codebase structure and build system.

## Table of Contents

- [High-Level Architecture](#high-level-architecture)
- [Module Structure](#module-structure)
- [Build System](#build-system)
- [Data Flow](#data-flow)
- [Platform Abstraction](#platform-abstraction)
- [Security Architecture](#security-architecture)

## High-Level Architecture

```mermaid
graph TB
    subgraph "User Space"
        USER[User Input]
        TERM[Terminal Output]
    end
    
    subgraph "Application Layer"
        MAIN[main.c<br/>Entry Point]
        CLI[CLI Module<br/>args.c/help.c]
        CORE[Core Module<br/>config.c/error.c]
        TUI[TUI Module<br/>tui.c/progress.c]
        IO[I/O Module<br/>input.c/output.c]
    end
    
    subgraph "Utility Layer"
        UTILS[Utils Module<br/>colors.c/logging.c/memory.c]
    end
    
    subgraph "Platform Layer"
        NCURSES[ncurses<br/>Linux/macOS]
        PDCURSES[pdcurses<br/>Windows]
        LIBC[Platform libc]
    end
    
    USER --> MAIN
    MAIN --> CLI
    CLI --> CORE
    CLI --> TUI
    CORE --> IO
    TUI --> IO
    IO --> UTILS
    TUI --> NCURSES
    TUI --> PDCURSES
    UTILS --> LIBC
    IO --> TERM
```

## Module Structure

### Core Modules

```mermaid
graph LR
    subgraph "src/cli"
        ARGS[args.c<br/>Argument Parsing]
        HELP[help.c<br/>Help Generation]
    end
    
    subgraph "src/core"
        CONFIG[config.c<br/>Configuration]
        ERROR[error.c<br/>Error Handling]
        TYPES[types.h<br/>Core Types]
    end
    
    subgraph "src/io"
        INPUT[input.c<br/>User Input]
        OUTPUT[output.c<br/>Formatted Output]
    end
    
    subgraph "src/tui"
        TUIDEF[tui.c<br/>TUI Framework]
        PROGRESS[tui_progress.c<br/>Progress Bars]
    end
    
    subgraph "src/utils"
        COLORS[colors.c<br/>Color Definitions]
        LOGGING[logging.c<br/>Logging System]
        MEMORY[memory.c<br/>Secure Memory]
    end
    
    ARGS --> CONFIG
    ARGS --> ERROR
    TUIDEF --> COLORS
    TUIDEF --> OUTPUT
    CONFIG --> LOGGING
    INPUT --> MEMORY
```

### Module Responsibilities

| Module | Purpose | Key Functions |
|--------|---------|---------------|
| **cli** | Command-line argument parsing and help generation | `parse_args()`, `print_help()` |
| **core** | Core application logic and configuration | `load_config()`, `handle_error()` |
| **io** | Input/output operations with formatting | `read_input()`, `write_output()` |
| **tui** | Terminal UI components and rendering | `init_tui()`, `show_progress()` |
| **utils** | Cross-cutting utilities and helpers | `secure_malloc()`, `log_message()` |

## Build System

### Zig Build Pipeline

```mermaid
graph TD
    subgraph "Build Inputs"
        BUILDZIG[build.zig<br/>Build Script]
        BUILDZON[build.zig.zon<br/>Dependencies]
        CSRC[C Source Files<br/>src/**/*.c]
        CHDR[C Headers<br/>src/**/*.h]
    end
    
    subgraph "Zig Build Process"
        PARSE[Parse build.zig]
        DEPS[Fetch Dependencies<br/>aro, etc.]
        COMPILE[Compile C23 Code<br/>via Zig CC]
        LINK[Link Binary<br/>+ Platform Libs]
        TEST[Run Tests<br/>test/main.zig]
    end
    
    subgraph "Build Outputs"
        BIN[zig-out/bin/myapp<br/>Executable]
        TESTOUT[Test Results]
        CACHE[zig-cache/<br/>Build Cache]
    end
    
    BUILDZIG --> PARSE
    BUILDZON --> DEPS
    CSRC --> COMPILE
    CHDR --> COMPILE
    PARSE --> COMPILE
    DEPS --> COMPILE
    COMPILE --> LINK
    LINK --> BIN
    PARSE --> TEST
    TEST --> TESTOUT
    COMPILE --> CACHE
```

### Build Commands

```bash
# Development build
zig build

# Run tests
zig build test

# Release build
zig build -Doptimize=ReleaseSafe

# Install to prefix
zig build install --prefix ~/.local

# Check without building
zig build check
```

## Data Flow

### Command Processing Flow

```mermaid
sequenceDiagram
    participant User
    participant Main
    participant CLI
    participant Core
    participant TUI
    participant IO
    
    User->>Main: ./myapp command args
    Main->>CLI: parse_args(argc, argv)
    CLI->>Core: validate_command()
    
    alt Valid Command
        Core->>TUI: init_tui()
        TUI->>IO: setup_terminal()
        Core->>Core: execute_command()
        Core->>TUI: update_display()
        TUI->>IO: render_output()
        IO->>User: Display Result
    else Invalid Command
        Core->>CLI: get_help_text()
        CLI->>IO: print_error()
        IO->>User: Show Error + Help
    end
    
    Main->>TUI: cleanup_tui()
    Main->>User: Exit(status)
```

## Platform Abstraction

### Cross-Platform Strategy

```mermaid
graph TD
    subgraph "Application Code"
        APP[Platform-Agnostic Code]
    end
    
    subgraph "Abstraction Layer"
        ABS[Platform Abstractions<br/>#ifdef guards]
    end
    
    subgraph "Linux/macOS"
        UNIX[POSIX APIs]
        NC[ncurses]
        MLOCK[mlock()]
    end
    
    subgraph "Windows"
        WIN[Win32 APIs]
        PDC[pdcurses]
        VLOCK[VirtualLock()]
    end
    
    APP --> ABS
    ABS -->|__linux__ or __APPLE__| UNIX
    ABS -->|_WIN32| WIN
    UNIX --> NC
    WIN --> PDC
    UNIX --> MLOCK
    WIN --> VLOCK
```

### Platform-Specific Features

| Feature | Linux/macOS | Windows |
|---------|-------------|---------|
| **Terminal UI** | ncurses | pdcurses |
| **Secure Memory** | mlock/munlock | VirtualLock/VirtualUnlock |
| **Config Path** | ~/.config/myapp/ | %APPDATA%\myapp\ |
| **Path Separator** | / | \ |
| **Binary Extension** | (none) | .exe |

## Security Architecture

### Security Layers

```mermaid
graph TB
    subgraph "Input Validation"
        ARGVAL[Argument Validation]
        INPUTSAN[Input Sanitization]
        BUFCHECK[Buffer Overflow Protection]
    end
    
    subgraph "Memory Security"
        SECMEM[Secure Allocation<br/>mlock/VirtualLock]
        ZEROMEM[Zero on Free<br/>secure_zero()]
        CANARY[Stack Canaries<br/>-fstack-protector]
    end
    
    subgraph "Build Security"
        RELRO[RELRO<br/>Read-Only Relocations]
        NX[NX/DEP<br/>No-Execute]
        PIE[PIE/ASLR<br/>Position Independent]
    end
    
    ARGVAL --> SECMEM
    INPUTSAN --> SECMEM
    BUFCHECK --> ZEROMEM
    SECMEM --> RELRO
    ZEROMEM --> NX
    CANARY --> PIE
```

### Security Features

1. **Input Validation**
   - All command-line arguments are validated
   - Buffer sizes are checked before operations
   - String operations use safe variants

2. **Memory Protection**
   - Sensitive data locked in memory (where supported)
   - Memory zeroed before deallocation
   - Stack protection enabled by default

3. **Build Hardening**
   - Position-independent executables (PIE)
   - Stack canaries enabled
   - Fortify source macros used

## Development Workflow

### Typical Development Cycle

```mermaid
graph LR
    subgraph "Development"
        EDIT[Edit Code]
        BUILD[zig build]
        TEST[zig build test]
        RUN[./zig-out/bin/myapp]
    end
    
    subgraph "Quality Checks"
        FORMAT[clang-format]
        LINT[clang-tidy]
        PRECOMMIT[pre-commit]
    end
    
    subgraph "CI/CD"
        PUSH[git push]
        CI[GitHub Actions]
        RELEASE[Release Build]
    end
    
    EDIT --> BUILD
    BUILD --> TEST
    TEST --> RUN
    RUN --> EDIT
    
    EDIT --> FORMAT
    FORMAT --> LINT
    LINT --> PRECOMMIT
    PRECOMMIT --> PUSH
    
    PUSH --> CI
    CI --> RELEASE
```

## Next Steps

- See [CONTRIBUTING.md](../CONTRIBUTING.md) for detailed build instructions
- Check [examples/](../examples/) for usage examples
- Review [build.zig](../build.zig) for build configuration details