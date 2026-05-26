# Test Suite

This template has two layers of tests:

- `main.zig` keeps fast Zig smoke tests close to the build graph.
- `test_*_scenarios.py` files are end-to-end terminal scenarios for project users.
- `test_terminal_harness.py` covers the reusable Python harness itself.

Run the default Zig suite:

```bash
zig build test
```

Run the Python terminal scenarios against the built binary:

```bash
zig build terminal-test
```

Run the same scenarios against a TUI-enabled build:

```bash
zig build -Denable-tui=true terminal-test
```

The non-interactive CLI scenarios use only Python's standard library.

TUI scenarios use `pexpect` plus `pyte` to run the app in a pseudo-terminal and
assert on a headless VT screen. The Nix dev shell provides these dependencies;
outside Nix, install them with your preferred Python package manager.
