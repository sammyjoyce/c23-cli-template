# Architecture Overview

A map of how a project generated from this template is put together: the layers, the real modules, how a command runs, and where to add your own code.

- [The mental model](#the-mental-model)
- [Module map](#module-map)
- [Request lifecycle](#request-lifecycle)
- [Build system](#build-system)
- [Platform differences](#platform-differences)
- [Security model](#security-model)
- [Where to add code](#where-to-add-code)

## The mental model

It is a small, layered C23 program. `main.c` parses arguments, resolves configuration,
selects the TUI, headless JSON, or a named CLI command, and runs command handlers
through one table. Handlers write text on TTYs or JSON under pipes through one I/O
layer and signal failure with a typed error code that becomes the process exit
status.

The interactive ncurses interface is a separate layer that compiles by default. Pass
`-Denable-tui=false` when you want a CLI/headless-only build with no curses
dependency.

```mermaid
graph TD
    USER[argv + stdin] --> MAIN[main.c]
    MAIN --> CLI["cli/ - parse args, dispatch"]
    CLI --> CMD["cli/commands_*.c - handlers"]
    CMD --> CORE["core/ - config, errors"]
    CMD --> IO["io/ - text + JSON output"]
    MAIN -. "bare TTY" .-> TUI["tui/ - ncurses"]
    CMD -. "menu, doctor --deep" .-> TUI
    CORE --> UTILS["utils/ - logging, colors, memory"]
    IO --> TERM[stdout / stderr]
    TUI --> CURSES[ncurses / pdcurses]
```

## Module map

Each directory under `src/` owns one concern. The functions below are representative entry points, not the full surface; read the matching header for the rest.

| Module | Files | Responsibility | Representative functions |
| --- | --- | --- | --- |
| `cli` | `args.c`, `help.c`, `commands.c`, `commands_*.c`, `opencli_contract.c` | Parse argv, apply global flags, find and dispatch commands, render help, expose the OpenCLI contract | `app_args_handle_immediate_exit()`, `app_commands()`, `app_command_find()`, `app_print_concise_help()` |
| `core` | `app_info.c`, `diagnostics.c`, `config.c`, `config_json.c`, `request_json.c`, `error.c`, `types.h` | Build/feature metadata, diagnostic checks, layered configuration, config/headless JSON readers, the flag table, and typed errors | `app_build_info()`, `app_feature_table()`, `app_diagnostics_collect()`, `app_config_create()`, `app_request_parse_json()`, `app_strerror()` |
| `io` | `input.c`, `output.c`, `terminal.c` | Read stdin/files; write human text and versioned JSON; answer basic curses-free terminal facts | `app_read_input_from_stdin()`, `app_output()`, `app_json_write_string()`, `app_terminal_is_interactive()` |
| `ui` | `action_item.c`, `text_layout.c` | Curses-free UI primitives. `text_layout.c` (text width/truncation/wrapping) is live and shared by the CLI and TUI renderers. `action_item.c` (selectable action descriptors) is a live shared seam: `app_actions_from_commands()` projects the CLI command table into curses-free descriptors, and the TUI's **Commands** screen (`tui/tui_app.c`) builds its menu rows from those descriptors via the adapter below — so this primitive is on the production path. | `app_text_width_utf8()`, `app_text_truncate_utf8_columns()`, `app_actions_from_commands()` |
| `tui` | `tui.c`, `tui_menu.c`, `tui_menu_adapter.c`, `tui_menu_model.c`, `tui_progress.c`, `tui_app.c` | ncurses lifecycle, modal menus, progress bars, and the demo showcase (compiled by default unless `-Denable-tui=false`). `tui_menu_adapter.c` converts each curses-free `app_action_item_t` into a `tui_menu_item_t`; the showcase's **Commands** screen uses it to render CLI command metadata as menu rows. | `tui_init()`, `tui_cleanup()`, `tui_show_menu()`, `tui_menu_item_from_action()`, `tui_progress_create()` |
| `utils` | `colors.c`, `logging.c`, `memory.c` | Cross-cutting helpers: color setup, leveled logging, secret zeroing | `app_log_init()`, `app_secret_zero()` |

The command table is the seam to extend. `commands.c` registers the built-in commands,
and each lives in its own file (`commands_basic.c` for `hello`/`echo`, plus
`commands_info.c`, `commands_doctor.c`, `commands_menu.c`, `commands_opencli.c`). See
[examples/adding-a-command.md](../examples/adding-a-command.md).

## Request lifecycle

1. `main()` initializes logging and creates an `app_config_t`.
2. The CLI layer reads argv. Immediate-exit options (`--help`, `--version`) are handled
   first by `app_args_handle_immediate_exit()`. Global flags (`--debug`, `--quiet`,
   `--verbose`, `--json`, `--plain`, `--no-color`, `--config`) update the config; the
   remaining tokens become the command name and its arguments.
3. Configuration is resolved by precedence: **CLI args > environment > config file > defaults**.
4. With no command, `main()` selects the front-end: bare TTY opens the TUI; bare
   non-TTY reads a request object with `app_request_parse_json()` and maps it onto
   the command table.
5. `app_command_find()` looks up the command. Its handler runs and writes output through `app_output()` / the `app_json_*` helpers.
6. On failure a handler returns an `app_error` value (see `core/error.c`). `app_strerror()` describes it, and the numeric code becomes the exit status. The public codes are listed in `opencli.json`.
7. Commands that need the terminal UI (`menu`, and `doctor --deep`) call `tui_init()` and always pair it with `tui_cleanup()`, including on interrupt.

The stable shape of this surface (commands, flags, exit codes, JSON envelopes) is documented in [CONTRACTS.md](CONTRACTS.md).

## Build system

`build.zig` compiles the C sources with Zig's bundled Clang. The base binary is the
file list in the `base_sources` array; the `tui_sources` are appended by default,
which also defines `ENABLE_TUI=1` and links `ncursesw` (or `pdcurses` on Windows).
Pass `-Denable-tui=false` to skip those sources. A separate `tui-menu-lib` step builds the reusable menu
primitive as a static library with installed headers.

For the build options, steps, and how to add a source file, see the [Zig Primer](ZIG_PRIMER.md).

## Platform differences

| Concern | Linux / macOS | Windows |
| --- | --- | --- |
| Terminal UI | ncurses (`ncursesw`) | pdcurses |
| Secret memory | `app_secret_zero()` helper (not wired by default) | `app_secret_zero()` helper (not wired by default) |
| Config path | `~/.config/myapp/config.json` | `%USERPROFILE%\AppData\Local\myapp\config.json` |
| Binary name | `myapp` | `myapp.exe` |

Cross-compilation is a `-Dtarget=` flag away because Zig ships every target's headers and libc; no second toolchain to install.

## Security model

The template's defenses are the ones actually wired into the code and CI, not compiler hardening flags. Those are left for you to opt into.

- **Secret-zeroing helper.** `src/utils/memory.h` exports `app_secret_zero()` for
  clearing sensitive buffers. The template ships it as a primitive but no production
  path calls it yet; invoke it where your code holds secrets. Memory locking
  (`mlock` / `VirtualLock`) is not wired in either.
- **Compiler warnings.** C sources compile with `-Wall -Wextra -std=c23`. `-Doptimize=` selects the optimization level; no Zig runtime is linked into the C-only binary.
- **Static analysis in CI.** GitHub Actions runs `clang-tidy` and `cppcheck` over the C sources on every change.
- **Supply chain in CI.** Gitleaks secret scanning, CodeQL, an OpenSSF Scorecard run, SBOM generation, and release gates are scaffolded in GitHub Actions.
  Keep third-party actions and downloaded tools pinned to immutable revisions in production projects.

Compiler-level hardening (`-fstack-protector-strong`, `-D_FORTIFY_SOURCE=2`, PIE/RELRO) is **not** enabled by default. If your threat model needs it, add the flags to `base_flags` in `build.zig`.

## Where to add code

| You want to… | Start here |
| --- | --- |
| Add a command | [examples/adding-a-command.md](../examples/adding-a-command.md), then register it in `src/cli/commands.c` |
| Add a config flag | `src/core/config.c` (the flag table) and `src/core/config_json.c` |
| Add a new exit code | `src/core/error.c`, then regenerate `opencli.json` (see [CONTRACTS.md](CONTRACTS.md)) |
| Build a TUI screen | [examples/custom-tui.md](../examples/custom-tui.md) and `src/tui/` |
| Add a source file to the build | the `base_sources` array in `build.zig` (see [ZIG_PRIMER.md](ZIG_PRIMER.md)) |
| Test any of the above | [TESTING.md](TESTING.md) |
