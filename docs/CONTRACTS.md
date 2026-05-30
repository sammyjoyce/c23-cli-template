# Public Contracts

This template is an opinionated reference app built on two reusable seams: a
machine-readable CLI contract and a small C TUI menu primitive. A "contract" here is a
stability promise. The surfaces listed as *supported* are safe to build automation or
downstream code on; everything else (help wording, colors, file layout, internal
helpers) can change without notice.

Keep mechanism in these seams. Keep local workflow policy in the generated app.

## CLI contract

`opencli.json` is the checked-in CLI contract, and the binary prints the same document:

```bash
myapp opencli
```

`zig build test` fails when `myapp opencli` and `opencli.json` drift, so command and flag metadata must change in the C tables *before* the spec does. The canonical sources:

| Source | Owns |
| --- | --- |
| `src/cli/opencli_contract.c` | OpenCLI info, conventions, root command arguments, extra examples, and metadata |
| `src/cli/commands.c` | `--help`/`--version` metadata, command names and summaries, global value options such as `--config`, command arguments and options, examples, terminal requirements |
| `src/core/config.c` | Global flag metadata |
| `src/core/error.c` | Public exit codes and descriptions |

After editing those tables, regenerate the artifact and re-run the tests:

```bash
zig build run -- opencli > opencli.json
zig build test
```

**Supported (stable to depend on):**

- global options before the command
- command names, arguments, options, and examples in `opencli.json`
- public exit codes in `opencli.json`
- `--json` responses that include `format_version`
- JSON output as the default whenever stdout is not a terminal; pass `--plain`
  before the command to keep human text under pipes or redirection
- `myapp opencli` and `myapp --json opencli` both write the same schema document because the contract is already JSON
- stdout for command output, stderr for errors and diagnostics
- config precedence: CLI args > environment > config file > defaults

## Dispatch modes

The binary chooses its front-end from argv shape and terminal attachment:

| Invocation | Condition | Behavior |
| --- | --- | --- |
| `myapp` | stdin and stdout are TTYs | Launch the interactive TUI menu. |
| `myapp <command> ...` | any terminal shape | Dispatch the named CLI command through the command table. |
| `myapp` | no interactive TTY | Read a headless JSON request object from stdin and dispatch it. |

`--help` and `--version` are immediate-exit flags and keep their human-readable
stdout behavior even under pipes.

### Headless JSON request protocol

Bare non-TTY invocation is an experimental, versioned machine surface for agents
and CI wrappers that prefer stdin/stdout over shell-token construction. The
request shape is:

```json
{
  "command": "hello",
  "args": ["Alice"],
  "flags": {
    "debug": false,
    "quiet": false,
    "verbose": false,
    "json_output": true,
    "plain_output": false,
    "no_color": false
  }
}
```

Only `command` is required. `args` defaults to an empty array. `flags` uses the
same JSON keys as config files and currently accepts booleans only. Unknown flag
names are rejected.

Responses are whatever the dispatched command writes in JSON mode and therefore
include the same `format_version` convention as `--json` command output. Parse
and dispatch errors are written to stderr as JSON messages and use the public
exit codes from `src/core/error.c`. Empty stdin is a `APP_ERROR_MISSING_ARG`
failure.

**Private (may change without notice):**

- exact help text, example prose, spacing, and colors
- log wording and timing
- internal `app_error` values not listed as public exit codes
- source layout and private helper functions

## TUI menu contract

`zig build tui-menu-lib` builds the narrow reusable primitive and installs:

```text
zig-out/lib/libtui-menu.a
zig-out/include/c23-cli-template/core/error.h
zig-out/include/c23-cli-template/core/types.h
zig-out/include/c23-cli-template/tui/tui.h
zig-out/include/c23-cli-template/tui/tui_menu.h
zig-out/include/c23-cli-template/tui/tui_progress.h
```

**Supported:**

- the `tui_init()` / `tui_cleanup()` lifecycle from `tui.h`
- `tui_show_menu()` from `tui_menu.h`
- `tui_menu_config_t`, `tui_menu_item_t`, and `tui_menu_result_t`
- the pointer-lifetime rules documented in `tui_menu.h`
- separators, disabled items, mnemonics, search, numeric jumps, resize handling, and interrupt handling

**Private:**

- `tui_menu_internal.h` and `tui_menu_state_t` internals
- cell-by-cell rendering details
- exact footer/help text inside the alternate screen
- terminal-test snapshots, except where a test names a specific invariant

## Not yet

Do not add a plugin API, stable ABI promise, long-running headless service, or
broad TUI framework until multiple generated projects need the same unsupported
behavior. Prefer a CLI/spec addition or a small library function before any
in-process extension system.

## See also

- [ARCHITECTURE.md](ARCHITECTURE.md) - where these seams sit in the codebase
- [TESTING.md](TESTING.md) - the tests that enforce the CLI contract
