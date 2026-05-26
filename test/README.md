# Test Suite

This template has two layers of tests:

- `main.zig` keeps fast Zig smoke tests close to the build graph.
- The C Ghostty VT runner covers CLI contracts plus PTY-backed TUI flows when libghostty-vt is available.
- `test_*_scenarios.py` and `test_terminal_harness.py` are the Python fallback for platforms without Ghostty VT.

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

The default terminal-test backend is `auto`: it uses the C Ghostty VT runner
when `libghostty-vt` is available through `pkg-config` on POSIX hosts, otherwise
it falls back to the Python harness. Windows always uses the Python backend. The
Ghostty path does not invoke Python.

The Ghostty VT runner is split across `terminal_vt_*.c` files. It runs CLI
checks directly, runs TUI checks in a pseudo-terminal, feeds output through
libghostty-vt, snapshots the screen with Ghostty's formatter API, and drives
deterministic input plus resize actions.

The Python fallback uses `pexpect` plus `pyte` for TUI scenarios. The Nix dev
shell provides the Python fallback dependencies. For the Ghostty backend,
install a libghostty-vt build with the development Terminal and Formatter APIs
and make it visible through `pkg-config` or
`-Dghostty-vt-prefix=/path/to/ghostty`.
