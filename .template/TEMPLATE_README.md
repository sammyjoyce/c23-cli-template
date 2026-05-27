# myapp

A ready-to-use C23 TUI + CLI starter.

## Features

- C23 command-line application skeleton built with Zig 0.16.0.
- Human-readable and JSON output modes for automation-friendly commands.
- Layered configuration from config files, environment, and CLI flags.
- Optional ncurses/PDCurses TUI build with windows, menus, dialogs, and progress bars.
- Small module layout that keeps CLI routing, core state, I/O, TUI, and utilities separate.
- Zig build steps for build, run, test, terminal-test, check, format, and cleanup.
- End-to-end terminal scenario harness for non-interactive CLI tests and optional PTY-backed TUI tests.

## Requirements

- Zig 0.16.0.
- A system C toolchain for libc and optional platform libraries.
- ncurses development headers on Linux/macOS when building with `-Denable-tui=true`.
- PDCurses on Windows when building with `-Denable-tui=true`.
- Ghostty VT library for PTY-backed terminal scenario tests (optional; tests skip without it).

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

The Zig tests keep fast build-integrated smoke coverage. The terminal scenario tests exercise non-interactive CLI contracts and, when built with `-Denable-tui=true`, drive the ncurses menu through a pseudo-terminal.

PTY-backed TUI tests use the Ghostty VT library. They skip cleanly when Ghostty is not available.

## Project Layout

```text
.
|-- build.zig
|-- build.zig.zon
|-- opencli.json
|-- src/
|   |-- main.c
|   |-- cli/            # arg parsing, help, and per-command modules
|   |-- core/           # config, errors, shared types
|   |-- io/             # input/output and JSON helpers
|   |-- tui/            # optional ncurses layer
|   `-- utils/          # logging, colors, optional secrets helpers
|-- test/
|   |-- cli_contract.h
|   |-- cli_contract_cases.c     # individual test cases
|   |-- cli_contract_helpers.c   # subprocess + temp file helpers
|   |-- cli_contract_runner.c    # TAP entry point
|   |-- unit_runner.c            # in-process unit tests
|   |-- terminal_vt.h            # optional Ghostty-backed PTY harness
|   |-- terminal_vt_common.c
|   |-- terminal_vt_runner.c
|   |-- terminal_vt_scenarios.c
|   `-- terminal_vt_session.c
|-- docs/
`-- examples/
```

## Add A Command

1. Write the handler in a new file under `src/cli/`, e.g. `commands_status.c`:

   ```c
   app_error app_cmd_status(const app_config_t *config, int argc, char **argv) {
     (void)argc; (void)argv;
     app_output(config, "status ok", false);
     return APP_SUCCESS;
   }
   ```

2. Register it in `src/cli/commands.c` (forward declaration + one row in
   `g_app_commands`).
3. Add the new source path to `build.zig`'s `base_sources` array.
4. (Optional) Add an entry to `test/cli_contract_cases.c` to lock the
   contract in place.

Help text comes from the command summary; no changes to `src/cli/help.c`
are needed.

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
