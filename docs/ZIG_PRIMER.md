# Zig Primer for C Developers

You do not need to know Zig to work on this project. Zig is only the build system: it
replaces Make or CMake, and it bundles the C compiler. This guide covers the handful of
commands and the `build.zig` structure you will actually touch.

- [Why Zig here?](#why-zig-here)
- [The commands you actually need](#the-commands-you-actually-need)
- [How this build.zig is organized](#how-this-buildzig-is-organized)
- [Build options](#build-options)
- [Build steps](#build-steps)
- [Adding a C file](#adding-a-c-file)
- [Cross-compiling](#cross-compiling)
- [Zig vs Make and CMake](#zig-vs-make-and-cmake)
- [Troubleshooting](#troubleshooting)
- [Resources](#resources)

## Why Zig here?

- **One toolchain, every target.** `zig cc` bundles Clang/LLVM plus the headers and libc for every platform, so cross-compilation is a flag, not a second toolchain.
- **The build script is just code.** `build.zig` is Zig, not a bespoke macro language, so logic like "only compile the TUI when a flag is set" is an ordinary `if`.
- **Caching and parallelism are automatic.** Incremental rebuilds and parallel compilation come for free.
- **It is still your C.** The sources are C23. Zig compiles and links them; it does not change how you write them.

## The commands you actually need

```bash
zig build                         # build + install the binary to zig-out/bin/
zig build run -- hello Alice      # build, then run with arguments after --
zig build test                    # CLI contract tests + in-process unit tests
zig build -Doptimize=ReleaseSafe  # optimized build
zig build -Denable-tui=true run -- menu   # build with the TUI and open the demo menu
```

A `justfile` wraps the common ones if you prefer: `just build`, `just check`, `just test-fast`, `just clean`. Run `just help` to list them.

## How this build.zig is organized

The script builds **one executable from a list of C files.** There is no Zig source in
the binary. This excerpt is simplified from the real `build.zig`; open that file for
the full source list and flag handling.

```zig
const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const app_name = b.option([]const u8, "app-name", "Application and binary name") orelse "myapp";
    const enable_tui = b.option(bool, "enable-tui", "Enable the ncurses/PDCurses TUI") orelse false;

    const exe = b.addExecutable(.{
        .name = app_name,
        .root_module = b.createModule(.{
            .root_source_file = null, // a C program: no Zig root file
            .target = target,
            .optimize = optimize,
            .link_libc = true,
        }),
    });
    exe.root_module.addIncludePath(b.path("src"));

    const base_sources = [_][]const u8{
        "src/main.c",
        "src/core/config.c",
        // ... full list lives in build.zig
    };
    // The real script also appends -DAPP_VERSION, -DAPP_NAME, -DAPP_GIT_COMMIT,
    // and -DENABLE_TUI=1 (when enabled) to these flags.
    const base_flags = [_][]const u8{ "-Wall", "-Wextra", "-std=c23", "-D_GNU_SOURCE" };
    exe.root_module.addCSourceFiles(.{ .files = &base_sources, .flags = &base_flags });

    if (enable_tui) {
        exe.root_module.addCSourceFiles(.{
            .files = &.{
                "src/tui/tui.c",
                "src/tui/tui_app.c",
                "src/tui/tui_menu.c",
                "src/tui/tui_menu_model.c",
                "src/tui/tui_progress.c",
            },
            .flags = &base_flags,
        });
        if (target.result.os.tag == .windows) {
            exe.root_module.linkSystemLibrary("pdcurses", .{});
        } else {
            exe.root_module.linkSystemLibrary("ncursesw", .{});
        }
    }

    b.installArtifact(exe);
}
```

The pieces to recognize:

- **`b`** is the build graph. You add artifacts (executables, libraries) and steps to it.
- **`b.createModule`** with `root_source_file = null` and `link_libc = true` is how a C program is declared in modern Zig. Source files attach to `exe.root_module`, not to `exe` directly.
- **`base_sources` / `base_flags`** are plain arrays. Adding a file means editing an array (see [Adding a C file](#adding-a-c-file)).
- **`b.option(...)`** declares the `-D...` flags listed below.

## Build options

Pass these as `-D<name>=<value>` on any `zig build` command.

| Option | Values (default) | Effect |
| --- | --- | --- |
| `-Doptimize=` | `Debug` (default), `ReleaseSafe`, `ReleaseFast`, `ReleaseSmall` | C optimization level (no Zig runtime is linked into this C-only binary) |
| `-Dtarget=` | e.g. `x86_64-windows`, `aarch64-macos` | Cross-compile target |
| `-Denable-tui=` | `true` / `false` (default `false`) | Compile the ncurses TUI and link curses |
| `-Dapp-name=` | string (default `myapp`) | Application and binary name |
| `-Dversion=` | string (default `0.1.0`) | Version baked into the binary |
| `-Dcurses-prefix=` | path | Override the ncurses/PDCurses install prefix |
| `-Dterminal-backend=` | `auto` (default) / `none` / `ghostty` | PTY/TUI terminal-test backend selection; see [TESTING.md](TESTING.md) |
| `-Dghostty-vt-prefix=` | path | Override the libghostty-vt install prefix |

## Build steps

| Command | What it does |
| --- | --- |
| `zig build` | Build and install the binary (the default step) |
| `zig build run -- ARGS` | Build, then run with `ARGS` |
| `zig build test` | CLI contract tests plus in-process unit tests |
| `zig build unit-test` | Only the in-process unit tests |
| `zig build terminal-test` | Unit and CLI tests plus PTY/TUI scenarios when TUI + backend are available |
| `zig build tui-menu-lib` | Build the reusable TUI menu static library and install its headers |
| `zig build fmt` / `fmt-check` | Format, or check formatting of, `build.zig`, `src`, and `test` |
| `zig build check` | Baseline gate: `fmt-check` + tests (what CI runs) |
| `zig build clean` | Remove `zig-out` and `.zig-cache` |

## Adding a C file

1. Drop the file under `src/`.
2. Add its path to the `base_sources` array in `build.zig` (or `tui_sources` if it is TUI-only):

   ```zig
   const base_sources = [_][]const u8{
       // ... existing files ...
       "src/features/deploy.c",
   };
   ```

3. Rebuild with `zig build`. Headers are found automatically because `src/` is on the include path.

## Cross-compiling

```bash
# Windows binary from any host
zig build -Dtarget=x86_64-windows -Doptimize=ReleaseSafe

# Apple Silicon macOS binary
zig build -Dtarget=aarch64-macos -Doptimize=ReleaseSafe
```

The default CLI cross-compiles with no extra setup. A cross **TUI** build (`-Denable-tui=true`) also needs the curses library for the target; point at it with `-Dcurses-prefix=`.

## Zig vs Make and CMake

| Concern | Zig | Make | CMake |
| --- | --- | --- | --- |
| Build language | Zig | Make syntax | CMake script |
| Cross-compilation | Built in | Manual toolchains | Toolchain files |
| Dependencies | `build.zig.zon` | None | FetchContent / ExternalProject |
| Platform detection | Automatic | Manual | Automatic |
| Caching and parallelism | Automatic | `make -j`, manual cache | Automatic |
| Learning curve | Moderate | Low | High |

A Makefile rule like this:

```makefile
myapp: main.c args.c
	gcc -std=c23 -Wall -O2 -o $@ $^ -lncursesw
```

becomes, in `build.zig`:

```zig
const exe = b.addExecutable(.{
    .name = "myapp",
    .root_module = b.createModule(.{
        .root_source_file = null,
        .target = target,
        .optimize = .ReleaseSafe,
        .link_libc = true,
    }),
});
exe.root_module.addCSourceFiles(.{
    .files = &.{ "main.c", "args.c" },
    .flags = &.{ "-std=c23", "-Wall" },
});
exe.root_module.linkSystemLibrary("ncursesw", .{});
b.installArtifact(exe);
```

## Troubleshooting

**`zig: command not found`**. Install Zig 0.16.0. The simplest route is [zvm](https://github.com/tristanisham/zvm): `zvm install 0.16.0 && zvm use 0.16.0`.

**ncurses/PDCurses headers or library not found.** Only happens with
`-Denable-tui=true`. Install the dev package (`apt install libncurses-dev`, `brew
install ncurses`, `dnf install ncurses-devel`), or point at a custom install with
`-Dcurses-prefix=/path`.

**libghostty-vt not found for terminal tests.** The PTY/TUI backend is optional. See
[TESTING.md](TESTING.md), or pass `-Dghostty-vt-prefix=/path`. Without it, auto mode
still runs `zig build test` and prints a PTY/TUI skip reason. Use
`-Dterminal-backend=ghostty` to require the backend, or `-Dterminal-backend=none` to
skip it explicitly.

**Stale build.** `zig build clean` removes `zig-out` and `.zig-cache`. (Equivalent: `rm -rf zig-out .zig-cache`.)

**Need to see what the compiler is doing.** `zig build --verbose` shows commands; `zig build --verbose-cc` shows the C compiler invocations.

## A real custom build step

Build logic is ordinary Zig, so `build.zig` can compute inputs. This project injects the current git hash as a `#define`, falling back to `"unknown"` outside a checkout:

```zig
const git_commit = blk: {
    const res = std.process.run(b.allocator, b.graph.io, .{
        .argv = &.{ "git", "rev-parse", "--short", "HEAD" },
    }) catch break :blk "unknown";
    // ... verify exit status ...
    break :blk b.dupe(std.mem.trim(u8, res.stdout, "\r\n"));
};
// added to the compile flags as: -DAPP_GIT_COMMIT="<hash>"
```

`myapp info` then reports that hash. Use the same pattern for any value you want baked into the binary at build time.

## Resources

- [Zig 0.16.0 Language Reference](https://ziglang.org/documentation/0.16.0/)
- [Zig Build System Guide](https://ziglang.org/learn/build-system/)
- [This project's build.zig](../build.zig) - the real script, the best reference
