# Testing CLI And TUI Behavior

This template includes two test layers so projects can test both ordinary CLI commands and interactive terminal UI flows.

## Tooling Direction

The terminal-testing landscape has a few useful families:

| Tool family | Examples | Strengths | Tradeoffs |
| --- | --- | --- | --- |
| Recording/rendering | `evp`, VHS | Great for docs, demos, and visual regression artifacts | Optimized for recordings, not assertions as the primary workflow |
| Headless terminal daemons/CLIs | `headless-terminal`, Phantom, Termscope | Real PTY plus parsed terminal screen, good waits, screenshots, sometimes cell/style data | Newer tools, often not packaged everywhere, may add daemon/build complexity |
| Library-first test drivers | Termless, `tui-driver`, `ptytest` | Nice assertion APIs inside host-language tests | Usually tied to Node, Go, Python, or Rust ecosystems |
| Classic command tests | Cram, Bats, Expect, pexpect | Mature and easy to run in CI | Raw output matching can miss alternate-screen/cursor details |

The template starts with a small Python harness built on **pexpect** and **pyte**:

- pexpect provides a real pseudo-terminal, so ncurses/PDCurses apps believe they are connected to a TTY.
- pyte parses the output into a normalized VT screen, so tests assert on what a user sees instead of raw escape bytes.
- Both are available in nixpkgs and are wired into `flake.nix` for `nix develop`.
- The harness is intentionally an adapter seam. If a project later needs richer
  assertions such as styles, screenshots, or cross-emulator comparisons, replace
  `PtySession` with a richer adapter without rewriting every test.

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

If you use the Nix dev shell, the Python dependencies are already on `PATH`:

```bash
nix develop
zig build -Denable-tui=true terminal-test
```

Outside Nix, install the optional PTY dependencies with your usual Python tooling:

```bash
python3 -m pip install pexpect pyte
```

The non-interactive CLI scenarios and harness unit tests still run without those
packages. PTY-backed TUI scenarios skip with a clear message when the optional
packages are missing or when the binary was not built with `-Denable-tui=true`.

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

Add or edit `test/test_tui_scenarios.py`:

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

Keep the built-in harness for most project tests. Consider an adapter around `headless-terminal`, Phantom, Termscope, or Termless when you need one of these:

- PNG/SVG screenshots as review artifacts.
- Cell-level color/style assertions.
- Scrollback assertions instead of only the visible screen.
- Cross-emulator compatibility checks.
- Detached sessions that a human or agent can watch live.

Those are valuable capabilities, but they are heavier than most template users need on day one.
