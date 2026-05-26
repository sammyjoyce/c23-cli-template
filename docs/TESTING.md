# Testing CLI And TUI Behavior

This template includes two test layers so projects can test both ordinary CLI commands and interactive terminal UI flows.

## Tooling Direction

The terminal-testing landscape has a few useful families:

| Tool family | Examples | Strengths | Tradeoffs |
| --- | --- | --- | --- |
| Recording/rendering | `evp`, VHS | Great for docs, demos, and visual regression artifacts | Optimized for recordings, not assertions as the primary workflow |
| Headless terminal daemons/CLIs | `headless-terminal`, Phantom, Termscope | Real PTY plus parsed terminal screen, good waits, screenshots, sometimes cell/style data | Newer tools, often not packaged everywhere, may add daemon/build complexity |
| Library-first test drivers | Bombadil terminal experiments, Termless, `tui-driver`, `ptytest` | Nice assertion APIs inside host-language tests | Often tied to Node, Go, Python, or Rust ecosystems |
| Classic command tests | Cram, Bats, Expect, pexpect | Mature and easy to run in CI | Raw output matching can miss alternate-screen/cursor details |

The template now has two PTY backends for terminal UI coverage:

- The **Ghostty VT backend** is a C runner in `test/terminal_vt_runner.c`. It runs
  the app in a real PTY, feeds output through
  [`libghostty-vt`](https://libghostty.tip.ghostty.org/index.html), snapshots the
  virtual terminal with Ghostty's formatter API, and drives deterministic input
  plus resize sequences. This follows the same shape as the Bombadil terminal
  experiment, but keeps the implementation in C and uses Ghostty's C API rather
  than Rust wrappers.
- The **Python fallback backend** keeps the original `pexpect` + `pyte` harness
  for machines that do not have libghostty-vt installed. It is also the portable
  fallback on Windows.

`zig build terminal-test` always runs the Python-discovered non-interactive CLI
scenarios and harness unit tests. It uses `-Dterminal-backend=auto` by default
for PTY coverage: auto selects Ghostty VT when `pkg-config` can find
libghostty-vt and its headers expose the Terminal and Formatter APIs; otherwise
it falls back to Python. Use `-Dterminal-backend=ghostty` to require Ghostty VT,
or `-Dterminal-backend=python` to force the fallback.

## Commands

Fast Zig smoke tests:

```bash
zig build test
```

End-to-end terminal scenario tests plus harness unit tests:

```bash
zig build terminal-test
```

TUI-enabled terminal scenarios:

```bash
zig build -Denable-tui=true terminal-test
```

If you use the Nix dev shell, the Python fallback dependencies are already on
`PATH`:

```bash
nix develop
zig build -Denable-tui=true terminal-test
```

Outside Nix, either install a libghostty-vt build with the development
[Terminal](https://libghostty.tip.ghostty.org/group__terminal.html) and
[Formatter](https://libghostty.tip.ghostty.org/group__formatter.html) APIs so
`pkg-config` can find `libghostty-vt.pc`, or install the optional Python fallback
dependencies:

```bash
python3 -m pip install pexpect pyte
```

If libghostty-vt is installed outside a standard linker path, pass its prefix:

```bash
zig build -Denable-tui=true terminal-test \
  -Dterminal-backend=ghostty \
  -Dghostty-vt-prefix=/path/to/ghostty
```

The non-interactive CLI scenarios and harness unit tests still run without
optional Python packages. When Ghostty VT is selected, the Python TUI scenarios
skip and the C runner provides PTY coverage. With the Python backend,
PTY-backed TUI scenarios skip with a clear message when the fallback
dependencies are missing or the binary was not built with `-Denable-tui=true`.

CI runs non-interactive terminal scenarios on Linux, macOS, and Windows through
`zig build check`. The PTY-backed TUI regression suite is Linux-gated in CI:
Linux installs `pexpect` and `pyte` and runs
`zig build -Denable-tui=true terminal-test` after the TUI build. macOS and
Windows still build the TUI binary and run the `--json info` smoke check, but
they do not run PTY-backed scenarios.

## Writing CLI Scenario Tests

Add or edit `test/test_*_scenarios.py`:

```python
import unittest
import terminal_harness as terminal


class DeployTests(unittest.TestCase):
    def test_dry_run_is_machine_readable(self):
        result = terminal.run_cli(["--json", "deploy", "--dry-run"])

        terminal.assert_success(self, result)
        terminal.assert_stdout_contains(self, result, '"format_version"')
```

Prefer stable contracts over incidental prose:

- Assert exit status.
- Assert JSON fields for automation-facing commands.
- Keep human-output assertions focused on durable words, not full paragraphs.
- Use `NO_COLOR=1` or `--plain` when colors are not under test.

## Writing TUI Scenario Tests

The Ghostty VT backend currently has fixed C scenarios for the template's demo
menu, including a deterministic input/resize smoke test. Add project-specific
Ghostty scenarios in `test/terminal_vt_runner.c` when you need cell-accurate
screen snapshots or resize coverage.

The Python fallback remains useful for quick project-level examples. Add or edit
`test/test_tui_scenarios.py`:

```python
import unittest
import terminal_harness as terminal


@unittest.skipUnless(terminal.tui_enabled(), "build with -Denable-tui=true")
@unittest.skipUnless(terminal.pty_available(), terminal.pty_unavailable_reason())
class MenuTests(unittest.TestCase):
    def test_menu_opens_help_panel(self):
        with terminal.PtySession([terminal.binary_path(), "menu"], cols=80, rows=24) as session:
            session.expect_text("Starter Showcase")
            session.send_keys("<CR>")
            session.expect_text("Starter Overview")
```

The key parser supports literal text plus common angle-bracket keys:

```text
<CR> <Enter> <Esc> <Tab> <BS> <Space> <Del> <Ins>
<Up> <Down> <Left> <Right> <Home> <End> <PageUp> <PageDown>
<F1> ... <F12>  <C-c>  <M-x>  <A-x>  <S-Tab>  <lt>
```

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
