# Testing CLI And TUI Behavior

This template includes two test layers so projects can test both ordinary CLI commands and interactive terminal UI flows.

## Tooling Direction

The terminal-testing landscape has a few useful families:

| Tool family | Examples | Strengths | Tradeoffs |
| --- | --- | --- | --- |
| Recording/rendering | `evp`, VHS | Great for docs, demos, and visual regression artifacts | Optimized for recordings, not assertions as the primary workflow |
| Headless terminal daemons/CLIs | `headless-terminal`, Phantom, Termscope | Real PTY plus parsed terminal screen, good waits, screenshots, sometimes cell/style data | Newer tools, often not packaged everywhere, may add daemon/build complexity |
| Library-first test drivers | Bombadil terminal experiments, Termless, `tui-driver`, `ptytest` | Nice assertion APIs inside host-language tests | Often tied to a specific host-language ecosystem |
| Classic command tests | Cram, Bats, Expect | Mature and easy to run in CI | Raw output matching can miss alternate-screen/cursor details |

The template now has one optional PTY terminal backend:

- The **Ghostty VT backend** is a C runner split across `test/terminal_vt_*.c`.
  It owns PTY/TUI scenario coverage when libghostty-vt is available. It runs the
  app in a real PTY, feeds output through
  [`libghostty-vt`](https://libghostty.tip.ghostty.org/index.html), snapshots the
  virtual terminal with Ghostty's formatter API, and drives deterministic input
  plus resize sequences. This follows the same shape as the Bombadil terminal
  experiment, but keeps the implementation in C and uses Ghostty's C API rather
  than Rust wrappers.

`zig build terminal-test` uses `-Dterminal-backend=auto` by default: auto selects
Ghostty VT when `pkg-config` can find libghostty-vt and its headers expose the
Terminal and Formatter APIs. Use `-Dterminal-backend=ghostty` to require Ghostty
VT on POSIX hosts. CLI contracts run through the Zig test binary on every host,
so machines without libghostty-vt still exercise non-interactive behavior
without a fallback scripting runtime.

## Commands

Fast Zig smoke tests:

```bash
zig build test
```

End-to-end terminal scenario tests:

```bash
zig build terminal-test
```

TUI-enabled terminal scenarios:

```bash
zig build -Denable-tui=true terminal-test
```

If you use the Nix dev shell, the required Zig and C tooling is already on
`PATH`:

```bash
nix develop
zig build -Denable-tui=true terminal-test
```

Outside Nix, install a libghostty-vt build with the development
[Terminal](https://libghostty.tip.ghostty.org/group__terminal.html) and
[Formatter](https://libghostty.tip.ghostty.org/group__formatter.html) APIs so
`pkg-config` can find `libghostty-vt.pc`.

If libghostty-vt is installed outside a standard linker path, pass its prefix:

```bash
zig build -Denable-tui=true terminal-test \
  -Dterminal-backend=ghostty \
  -Dghostty-vt-prefix=/path/to/ghostty
```

CLI scenarios run through `test/main.zig` on every backend. With Ghostty VT
selected, the C backend adds PTY/TUI coverage using libghostty-vt. Without
Ghostty VT, `zig build terminal-test` still runs the Zig CLI contract suite and
skips PTY/TUI coverage unless `-Dterminal-backend=ghostty` was requested.

CI runs non-interactive terminal scenarios on Linux, macOS, and Windows through
`zig build check`; these CLI contracts live in `test/main.zig` and run
regardless of whether Ghostty is installed. Linux CI also runs
`zig build -Denable-tui=true terminal-test` after the TUI build; that step adds
PTY-backed TUI coverage when libghostty-vt is available. macOS and Windows still
build the TUI binary and run the `--json info` smoke check, but they do not run
PTY-backed scenarios by default.

## Writing CLI Scenario Tests

Add durable CLI contract checks to `test/main.zig`. The Ghostty C runner is
intentionally scoped to PTY/TUI behavior so CLI contracts have one source of
truth.

Prefer stable contracts over incidental prose:

- Assert exit status.
- Assert JSON fields for automation-facing commands.
- Keep human-output assertions focused on durable words, not full paragraphs.
- Use `NO_COLOR=1` or `--plain` when colors are not under test.

## Writing TUI Scenario Tests

The Ghostty VT backend currently has fixed C scenarios for the template's demo
menu, including a deterministic input/resize smoke test. Add project-specific
Ghostty scenarios in `test/terminal_vt_scenarios.c` when you need cell-accurate
screen snapshots or resize coverage.

Keep PTY-backed TUI scenarios in `test/terminal_vt_scenarios.c`. Prefer small
step tables (`expect`, `send`, `resize`, `wait`) over long branch ladders so new
screens do not require a second harness.

## When To Reach For A Richer Tool

Keep the built-in Ghostty VT runner for most project tests. It already gives you
a real PTY, modern VT parsing, formatter-backed snapshots, scrollback support,
and terminal resize coverage without adding a daemon or Rust wrapper.

Consider an adapter around `headless-terminal`, Phantom, Termscope, Termless, or
an external fuzzer when you need one of these:

- PNG/SVG screenshots as review artifacts.
- Cell-level color/style assertions beyond plain text snapshots.
- Cross-emulator compatibility checks.
- Detached sessions that a human or agent can watch live.
- Long-running property-based exploration with many randomized action sequences.

Those are valuable capabilities, but they are heavier than most template users need on day one.
