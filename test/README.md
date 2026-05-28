# Test Suite

Three test layers cover unit logic, CLI contracts, and interactive terminal flows.

| Layer | How it runs | What it asserts | Lives in |
| --- | --- | --- | --- |
| Unit tests | In-process, linked against the real sources | Logic inside `config`, `error`, `tui_menu_model`, and other modules | `unit_*.c` |
| CLI contract tests | The built binary as a subprocess | Exit codes, JSON fields, durable output, `myapp opencli` matching `opencli.json` | `cli_contract_*.c` |
| PTY/TUI scenarios | The binary in a real PTY via libghostty-vt | Rendered screen snapshots, input and resize handling | `terminal_vt_*.c` |

## Running them

```bash
zig build unit-test       # just the in-process unit tests
zig build test            # unit tests + CLI contract tests
zig build terminal-test   # the above + PTY/TUI scenarios when available
```

`zig build terminal-test` defaults to `-Dterminal-backend=auto`, which selects the
Ghostty VT backend when libghostty-vt is available through `pkg-config` on POSIX hosts.
The Nix dev shell wires this up automatically. Without libghostty-vt the CLI portion
still runs; pass `-Dterminal-backend=ghostty` to require the PTY backend, or
`-Dghostty-vt-prefix=/path` to override the install prefix.

See [docs/TESTING.md](../docs/TESTING.md) for the full guide, including how to write each layer.
