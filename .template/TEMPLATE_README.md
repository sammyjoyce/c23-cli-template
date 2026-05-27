# myapp

A ready-to-use C23 TUI + CLI starter.

## Features

- C23 command-line application skeleton built with Zig 0.16.0.
- Human-readable and JSON output modes for automation-friendly commands.
- Layered configuration from config files, environment, and CLI flags.
- Optional ncurses/PDCurses TUI build with windows, menus, dialogs, and progress bars.
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

The Zig tests keep fast build-integrated smoke coverage. The terminal scenario tests exercise non-interactive CLI contracts through a C23 runner on every host.
When built with `-Denable-tui=true` and `libghostty-vt` is available, they also drive the ncurses menu through a pseudo-terminal.

PTY-backed TUI tests skip cleanly when `libghostty-vt` is not installed. Use `-Dterminal-backend=ghostty` to require Ghostty VT and fail if it is missing.

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
|   |-- test_harness.c
|   |-- test_harness.h
|   |-- terminal_vt_runner.c
|   |-- terminal_vt_scenarios.c
|   `-- terminal_vt_session.c
|-- docs/
`-- examples/
```

## Add A Command

Add the route in `src/main.c`:

```c
if (strcmp(command, "status") == 0) {
  app_output(config, "status ok", false);
  return APP_SUCCESS;
}
```

Then update:

- `src/cli/help.c` for user-facing help.
- `opencli.json` for machine-readable CLI metadata.
- `test/cli_contract_runner.c` for fast CLI contract and smoke coverage.
- `test/terminal_vt_scenarios.c` for PTY-backed TUI behavior when the command affects the menu flow.

## Customize The TUI

The reusable TUI helpers live under `src/tui/`:

- `tui.c` owns ncurses lifecycle, windows, bounded text, dialogs, and menus.
- `tui_progress.c` owns progress window rendering.
- `tui_demo.c` wires examples together for the `menu` command.

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
