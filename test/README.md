# Test Suite

This template has two layers of tests:

- `main.zig` keeps fast Zig smoke tests close to the build graph.
- `test_*_scenarios.py` files are end-to-end terminal scenarios for project users.
- `test_terminal_harness.py` covers the reusable Python harness itself.

Run the default Zig suite:

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

`zig build terminal-test` always runs the Python-discovered CLI scenario and
harness unit tests. The default PTY backend is `auto`: it uses the C Ghostty VT
runner when `libghostty-vt` is available through `pkg-config`, otherwise it
falls back to the Python harness.

The Ghostty VT runner lives in `terminal_vt_runner.c`. It runs the app in a
pseudo-terminal, feeds output through libghostty-vt, snapshots the screen with
Ghostty's formatter API, and drives deterministic input plus resize actions.

The Python fallback uses `pexpect` plus `pyte` for TUI scenarios. When Ghostty is
selected, Python TUI scenarios skip and the C runner provides PTY coverage. The
Nix dev shell provides the Python fallback dependencies. For the Ghostty backend,
install a libghostty-vt build with the development Terminal and Formatter APIs
and make it visible through `pkg-config` or
`-Dghostty-vt-prefix=/path/to/ghostty`.
