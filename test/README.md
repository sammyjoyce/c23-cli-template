# Test Suite

This template has two layers of tests:

- `cli_contract_runner.c` keeps fast C23 smoke and CLI contract tests close to
  the build graph.
- The C Ghostty VT runner covers PTY-backed TUI flows when libghostty-vt is available.

Run the default CLI contract suite:

```bash
zig build test
```

Run the terminal scenarios against the built binary:

```bash
zig build terminal-test
```

Run the same scenarios against a TUI-enabled build:

```bash
zig build -Denable-tui=true terminal-test
```

The default terminal-test backend is `auto`: it uses the C Ghostty VT runner
when `libghostty-vt` is available through `pkg-config` on POSIX hosts. CLI
contracts always run through `cli_contract_runner.c`, so hosts without
libghostty-vt still exercise non-interactive behavior without a fallback
scripting runtime.

The Ghostty VT runner is split across `terminal_vt_*.c` files. It runs TUI
checks in a pseudo-terminal, feeds output through libghostty-vt, snapshots the
screen with Ghostty's formatter API, and drives deterministic input plus resize
actions. For the Ghostty backend, install a libghostty-vt build with the
development Terminal and Formatter APIs and make it visible through `pkg-config` or
`-Dghostty-vt-prefix=/path/to/ghostty`.
