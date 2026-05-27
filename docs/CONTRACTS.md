# Public Contracts

This template is an opinionated reference app built on two reusable seams:

- a machine-readable CLI contract
- a small C TUI menu primitive

Keep mechanism in these seams. Keep local workflow policy in the generated app.

## CLI Contract

`opencli.json` is the checked-in CLI contract. The live binary prints the same
contract with:

```bash
myapp opencli
```

`zig build test` fails when `myapp opencli` and `opencli.json` drift, so command
and flag metadata must be updated in the C tables before the spec changes.

Supported automation surface:

- global options before the command
- command names, command arguments, command-specific options, and examples in
  `opencli.json`
- public exit codes listed in `opencli.json`
- `--json` responses that include `format_version`
- stdout for command output and stderr for errors or diagnostics
- config precedence: CLI args > environment > config file > defaults

Private or unstable surface:

- exact help text, examples prose, spacing, and colors
- log line wording and timing
- internal `app_error` values that are not listed as public exit codes
- source file layout and private helper functions

## TUI Menu Contract

`zig build tui-menu-lib` builds the narrow reusable TUI primitive and installs:

```text
zig-out/lib/libtui-menu.a
zig-out/include/c23-cli-template/core/error.h
zig-out/include/c23-cli-template/core/types.h
zig-out/include/c23-cli-template/tui/tui.h
zig-out/include/c23-cli-template/tui/tui_menu.h
zig-out/include/c23-cli-template/tui/tui_progress.h
```

Supported menu surface:

- `tui_init()` / `tui_cleanup()` lifecycle from `tui.h`
- `tui_show_menu()` from `tui_menu.h`
- `tui_menu_config_t`, `tui_menu_item_t`, and `tui_menu_result_t`
- documented pointer lifetime rules in `tui_menu.h`
- separators, disabled items, mnemonics, search, numeric jumps, resize handling,
  and interrupt handling

Private or unstable surface:

- `tui_menu_internal.h`
- `tui_menu_state_t` internals
- cell-by-cell rendering details
- exact footer/help text inside the alternate screen
- terminal test snapshots, except where a test names a specific invariant

## Not Yet

Do not add a plugin API, stable ABI promise, headless service, or broad TUI
framework until multiple generated projects need the same unsupported behavior.
Prefer a CLI/spec addition or a small library function before any in-process
extension system.
