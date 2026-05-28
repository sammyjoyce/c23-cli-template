# myapp

A ready-to-use C23 TUI + CLI starter.

## Features

- C23 command-line skeleton built with Zig 0.16.0.
- Human-readable and JSON output modes for automation-friendly commands.
- Layered configuration from config files, environment, and CLI flags.
- A live OpenCLI contract (`myapp opencli`) checked against `opencli.json`.
- Optional ncurses/PDCurses TUI with windows, menus, dialogs, and progress bars.
- Optional `tui-menu-lib` static library exposing the reusable TUI menu.
- A small module layout that keeps CLI routing, core state, I/O, TUI, and utilities separate.
- Build, run, test, terminal-test, check, format, and clean steps in `build.zig`.
- A C23 scenario harness for CLI contracts, plus optional Ghostty VT-backed TUI tests.

## Requirements

- Zig 0.16.0.
- A system C toolchain for libc and optional platform libraries.
- ncurses development headers (Linux/macOS) or PDCurses (Windows) when building with `-Denable-tui=true`.
- Optional `libghostty-vt` development files for PTY-backed TUI scenario tests.

## Build

```bash
zig build                          # debug
zig build -Doptimize=ReleaseSafe   # optimized
zig build -Denable-tui=true        # with the ncurses TUI
zig build tui-menu-lib             # the reusable TUI menu static library
```

The binary is named `myapp`; override it without editing source via `-Dapp-name=...`.

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
zig build -Denable-tui=true run -- menu
```

## Test and check

```bash
zig build test            # unit tests + CLI contract tests
zig build terminal-test   # the above, plus PTY/TUI scenarios when available
zig build check           # fmt-check + tests (the CI gate)
```

`zig build test` verifies that `myapp opencli` still matches `opencli.json`. PTY-backed TUI tests need `libghostty-vt`; they skip cleanly when it is absent, or pass `-Dterminal-backend=ghostty` to require them.

## Project layout

```text
.
|-- build.zig
|-- build.zig.zon
|-- opencli.json
|-- src/
|   |-- main.c
|   |-- cli/      command parsing, help, and the command table
|   |-- core/     configuration and typed errors
|   |-- io/       text and JSON output
|   |-- tui/      ncurses windows, menus, dialogs, progress (opt-in)
|   `-- utils/    logging, colors, secret zeroing
|-- test/
|-- docs/
`-- examples/
```

## Add a command

Write a handler with the standard signature:

```c
app_error app_cmd_status(const app_config_t *config, int argc, char **argv) {
  (void)argc;
  (void)argv;
  app_output("status ok", config, false);
  return APP_SUCCESS;
}
```

Then update `src/cli/commands.c` (metadata and dispatch), regenerate `opencli.json` from `myapp opencli`, and add coverage to `test/cli_contract_cases.c`. See `examples/adding-a-command.md`.

## Customize the TUI

The TUI helpers live under `src/tui/`: `tui.c` owns the ncurses lifecycle, windows, and dialogs; `tui_menu.c` / `tui_menu_model.c` own the reusable modal menu; `tui_progress.c` owns progress rendering; `tui_app.c` wires the `menu` command's showcase. Keep raw curses calls inside this layer so command handlers stay testable without a terminal. See `examples/custom-tui.md`.

## Configuration

Precedence is CLI flags > environment variables > config file > defaults. Default config paths:

- `~/.config/myapp/config.json` on Linux/macOS.
- `%USERPROFILE%\AppData\Local\myapp\config.json` on Windows.

## License

MIT. Update `LICENSE` if your project uses a different license.
