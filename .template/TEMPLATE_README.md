# myapp

A ready-to-use C23 TUI + CLI starter.

## Features

- C23 command-line application skeleton built with Zig 0.16.0.
- Human-readable and JSON output modes for automation-friendly commands.
- Layered configuration from config files, environment, and CLI flags.
- Live OpenCLI contract from `myapp opencli`, checked against `opencli.json`.
- Optional ncurses/PDCurses TUI build with windows, menus, dialogs, and progress bars.
- Optional `tui-menu-lib` static library target for the reusable TUI menu.
- Small module layout that keeps CLI routing, core state, I/O, TUI, and utilities separate.
- Zig build steps for build, run, test, terminal-test, check, format, and cleanup.
- C23 terminal scenario harness for non-interactive CLI contracts and optional Ghostty VT-backed TUI tests.

## Requirements

- Zig 0.16.0.
- A system C toolchain for libc and optional platform libraries.
- ncurses development headers on Linux/macOS when building with `-Denable-tui=true`.
- PDCurses on Windows when building with `-Denable-tui=true`.
- Optional `libghostty-vt` development files for PTY-backed TUI scenario tests.

## Build

```bash
zig build
zig build -Doptimize=ReleaseSafe
zig build -Denable-tui=true
zig build tui-menu-lib
```

The default binary name is `myapp`. You can override it without editing source:

```bash
zig build -Dapp-name=myapp
```

## Run

```bash
./zig-out/bin/myapp --help
./zig-out/bin/myapp hello
./zig-out/bin/myapp hello Alice
./zig-out/bin/myapp echo Hello from C23
./zig-out/bin/myapp info
./zig-out/bin/myapp --json info
./zig-out/bin/myapp doctor
./zig-out/bin/myapp opencli
```

Build with TUI support before launching the interactive showcase:

```bash
zig build -Denable-tui=true
./zig-out/bin/myapp menu
```

## Test And Check

```bash
zig build test
zig build terminal-test
zig build -Denable-tui=true terminal-test
zig build check
zig build fmt-check
```

The Zig tests keep fast build-integrated smoke coverage and verify that
`myapp opencli` matches `opencli.json`. The terminal scenario tests exercise
non-interactive CLI contracts through a C23 runner on every host. When built
with `-Denable-tui=true` and `libghostty-vt` is available, they also drive the
ncurses menu through a pseudo-terminal.

PTY-backed TUI tests skip cleanly when `libghostty-vt` is not installed. Use
`-Dterminal-backend=ghostty` to require Ghostty VT and fail if it is missing.

## Project Layout

```text
.
|-- build.zig
|-- build.zig.zon
|-- opencli.json
|-- src/
|   |-- main.c
|   |-- cli/
|   |-- core/
|   |-- io/
|   |-- tui/
|   `-- utils/
|-- test/
|   |-- cli_contract_runner.c
|   |-- cli_contract_cases.c
|   |-- cli_contract_helpers.c
|   |-- terminal_vt_runner.c
|   |-- terminal_vt_scenarios.c
|   `-- terminal_vt_session.c
|-- docs/
`-- examples/
```

## Add A Command

Add the handler in a command source file:

```c
app_error app_cmd_status(const app_config_t *config, int argc, char **argv) {
  (void)argc;
  (void)argv;
  app_output(config, "status ok", false);
  return APP_SUCCESS;
}
```

Then update:

- `src/cli/commands.c` for command metadata and dispatch.
- `opencli.json` from the live `myapp opencli` output.
- `test/cli_contract_cases.c` for contract coverage.
- `test/terminal_vt_scenarios.c` for PTY/TUI behavior when needed.

## Customize The TUI

The reusable TUI helpers live under `src/tui/`:

- `tui.c` owns ncurses lifecycle, windows, bounded text, and dialogs.
- `tui_menu.c` and `tui_menu_model.c` own the reusable modal menu.
- `tui_progress.c` owns progress window rendering.
- `tui_app.c` wires examples together for the `menu` command.

Keep raw curses calls inside the TUI layer where practical. That keeps command handlers easy to test without a terminal.

## Configuration

Configuration precedence is:

1. Command-line flags.
2. Environment variables.
3. Configuration file.
4. Defaults.

Default config paths are:

- `~/.config/myapp/config.json` on Linux/macOS.
- `%USERPROFILE%\AppData\Local\myapp\config.json` on Windows.

## License

MIT. Update `LICENSE` if your generated project uses a different license.
