"""Reusable helpers for end-to-end CLI and terminal UI tests.

The non-interactive helpers use only the Python standard library. PTY-backed
TUI tests additionally use pexpect to own a pseudo-terminal and pyte to keep a
headless VT screen model. The project flake provides both packages for local
development; tests that need them skip cleanly when they are not installed.
"""

from __future__ import annotations

from dataclasses import dataclass
import os
from pathlib import Path
import re
import subprocess
import time
from typing import Callable, Mapping, Sequence
import unittest

try:  # Optional: only required for PtySession.
    import pexpect  # type: ignore[import-not-found]
except ImportError:  # pragma: no cover - exercised when optional deps absent.
    pexpect = None  # type: ignore[assignment]

try:  # Optional: only required for PtySession.
    import pyte  # type: ignore[import-not-found]
except ImportError:  # pragma: no cover - exercised when optional deps absent.
    pyte = None  # type: ignore[assignment]

APP_BINARY_ENV = "APP_BINARY"
APP_TUI_ENABLED_ENV = "APP_TUI_ENABLED"


@dataclass(frozen=True)
class CommandResult:
    """Captured result from a non-interactive command run."""

    argv: Sequence[str]
    exit_code: int
    stdout: str
    stderr: str

    def assert_exit(self, case: unittest.TestCase, code: int) -> None:
        case.assertEqual(
            code,
            self.exit_code,
            msg=(
                f"expected exit {code}, got {self.exit_code}\n"
                f"argv: {self.argv!r}\n"
                f"stdout:\n{self.stdout}\n"
                f"stderr:\n{self.stderr}"
            ),
        )

    def assert_success(self, case: unittest.TestCase) -> None:
        self.assert_exit(case, 0)

    def assert_stdout_contains(self, case: unittest.TestCase, needle: str) -> None:
        case.assertIn(needle, self.stdout, msg=self._debug_message())

    def assert_stderr_contains(self, case: unittest.TestCase, needle: str) -> None:
        case.assertIn(needle, self.stderr, msg=self._debug_message())

    def assert_stdout_not_contains(self, case: unittest.TestCase, needle: str) -> None:
        case.assertNotIn(needle, self.stdout, msg=self._debug_message())

    def _debug_message(self) -> str:
        return (
            f"argv: {self.argv!r}\n"
            f"exit: {self.exit_code}\n"
            f"stdout:\n{self.stdout}\n"
            f"stderr:\n{self.stderr}"
        )


@dataclass(frozen=True)
class TerminalSnapshot:
    """Plain-text view of the current headless terminal screen."""

    text: str
    lines: Sequence[str]
    cursor_row: int
    cursor_col: int


class PtySession:
    """Drive a real process attached to a pseudo-terminal.

    This is intentionally small and boring: spawn the app, send vim-style key
    strings, wait for rendered text, and assert on the normalized screen. If you
    later need style-cell assertions or screenshots, this class is the adapter
    seam for a richer tool such as headless-terminal, Phantom, or Termless.
    """

    def __init__(
        self,
        argv: Sequence[str],
        *,
        cols: int = 80,
        rows: int = 24,
        cwd: str | Path | None = None,
        env: Mapping[str, str | None] | None = None,
        timeout: float = 5.0,
    ) -> None:
        if pexpect is None or pyte is None:
            raise RuntimeError(pty_unavailable_reason())

        if not argv:
            raise ValueError("argv must contain a command")

        child_env = os.environ.copy()
        child_env.update({"TERM": "xterm-256color"})
        child_env.pop("NO_COLOR", None)
        if env:
            for key, value in env.items():
                if value is None:
                    child_env.pop(key, None)
                else:
                    child_env[key] = value

        self.argv = [str(part) for part in argv]
        self.cols = cols
        self.rows = rows
        self.child = pexpect.spawn(
            self.argv[0],
            self.argv[1:],
            cwd=str(cwd or project_root()),
            env=child_env,
            encoding="utf-8",
            codec_errors="replace",
            dimensions=(rows, cols),
            timeout=timeout,
        )
        self.child.delaybeforesend = 0.02
        self.screen = pyte.Screen(cols, rows)
        self.stream = pyte.Stream(self.screen)
        self.transcript: list[str] = []
        self._closed = False

    def __enter__(self) -> "PtySession":
        return self

    def __exit__(self, exc_type, exc, tb) -> None:  # type: ignore[no-untyped-def]
        self.close()

    def send_keys(self, keys: str, *, settle: float = 0.05) -> None:
        """Send text with a small vim-style angle-bracket key syntax."""

        self.child.send(parse_keys(keys))
        if settle > 0:
            time.sleep(settle)
        self.drain()

    def drain(self, *, timeout: float = 0.0) -> None:
        """Read available child output into the VT screen model."""

        current_timeout = timeout
        while True:
            try:
                chunk = self.child.read_nonblocking(size=4096, timeout=current_timeout)
            except pexpect.TIMEOUT:  # type: ignore[union-attr]
                break
            except pexpect.EOF:  # type: ignore[union-attr]
                break

            if not chunk:
                break
            self.transcript.append(chunk)
            self.stream.feed(chunk)
            current_timeout = 0.0

    def snapshot(self) -> TerminalSnapshot:
        self.drain()
        lines = [line.rstrip() for line in self.screen.display]
        text = "\n".join(lines).rstrip()
        return TerminalSnapshot(
            text=text,
            lines=lines,
            cursor_row=self.screen.cursor.y + 1,
            cursor_col=self.screen.cursor.x + 1,
        )

    def expect_text(self, text: str, *, timeout: float = 5.0) -> TerminalSnapshot:
        return self.wait_until(
            lambda snap: text in snap.text,
            timeout=timeout,
            description=f"text {text!r} to appear",
        )

    def expect_regex(self, pattern: str, *, timeout: float = 5.0) -> TerminalSnapshot:
        regex = re.compile(pattern)
        return self.wait_until(
            lambda snap: regex.search(snap.text) is not None,
            timeout=timeout,
            description=f"pattern {pattern!r} to match",
        )

    def wait_until(
        self,
        predicate: Callable[[TerminalSnapshot], bool],
        *,
        timeout: float = 5.0,
        description: str = "condition",
    ) -> TerminalSnapshot:
        deadline = time.monotonic() + timeout
        last_snapshot = self.snapshot()

        while time.monotonic() < deadline:
            remaining = max(0.0, min(0.05, deadline - time.monotonic()))
            self.drain(timeout=remaining)
            last_snapshot = self.snapshot()
            if predicate(last_snapshot):
                return last_snapshot
            if not self.child.isalive():
                break

        if predicate(last_snapshot):
            return last_snapshot

        raise AssertionError(
            f"timed out waiting for {description}\n"
            f"argv: {self.argv!r}\n"
            f"screen:\n{last_snapshot.text}\n\n"
            f"transcript tail:\n{self.transcript_tail()}"
        )

    def wait_for_exit(self, *, timeout: float = 5.0) -> int | None:
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            self.drain(timeout=0.05)
            if not self.child.isalive():
                self.drain()
                return self.child.exitstatus

        raise AssertionError(
            f"timed out waiting for process exit\n"
            f"argv: {self.argv!r}\n"
            f"screen:\n{self.snapshot().text}\n\n"
            f"transcript tail:\n{self.transcript_tail()}"
        )

    def transcript_tail(self, limit: int = 4000) -> str:
        tail = "".join(self.transcript)
        if len(tail) <= limit:
            return tail
        return tail[-limit:]

    def close(self) -> None:
        if self._closed:
            return
        self._closed = True
        if self.child.isalive():
            try:
                self.child.sendcontrol("c")
                self.child.terminate(force=True)
            except Exception:  # pragma: no cover - cleanup best effort.
                pass


def project_root() -> Path:
    return Path(__file__).resolve().parents[1]


def binary_path() -> str:
    value = os.environ.get(APP_BINARY_ENV)
    if not value:
        raise RuntimeError(f"{APP_BINARY_ENV} is not set")
    return value


def tui_enabled() -> bool:
    return os.environ.get(APP_TUI_ENABLED_ENV) == "1"


def run_cli(
    args: Sequence[str],
    *,
    binary: str | Path | None = None,
    input_text: str | None = None,
    env: Mapping[str, str | None] | None = None,
    timeout: float = 5.0,
    cwd: str | Path | None = None,
) -> CommandResult:
    """Run the application without a TTY and capture stdout/stderr."""

    argv = [str(binary or binary_path()), *[str(arg) for arg in args]]
    child_env = os.environ.copy()
    child_env.update({"NO_COLOR": "1", "TERM": "dumb"})
    if env:
        for key, value in env.items():
            if value is None:
                child_env.pop(key, None)
            else:
                child_env[key] = value

    completed = subprocess.run(
        argv,
        input=input_text,
        text=True,
        capture_output=True,
        timeout=timeout,
        cwd=str(cwd or project_root()),
        env=child_env,
        check=False,
    )
    return CommandResult(
        argv=argv,
        exit_code=completed.returncode,
        stdout=completed.stdout,
        stderr=completed.stderr,
    )


def pty_available() -> bool:
    return os.name != "nt" and pexpect is not None and pyte is not None


def pty_unavailable_reason() -> str:
    if os.name == "nt":
        return "PTY terminal tests currently require a POSIX pseudo-terminal"

    missing = []
    if pexpect is None:
        missing.append("pexpect")
    if pyte is None:
        missing.append("pyte")
    if not missing:
        return "PTY dependencies are available"
    return (
        "PTY terminal tests require "
        + ", ".join(missing)
        + "; run inside `nix develop` or install them with Python."
    )


def parse_keys(text: str) -> str:
    """Parse a small headless-terminal/Vim-inspired key notation."""

    out: list[str] = []
    i = 0
    while i < len(text):
        if text[i] != "<":
            out.append(text[i])
            i += 1
            continue

        end = text.find(">", i + 1)
        if end == -1:
            out.append(text[i])
            i += 1
            continue

        token = text[i + 1 : end]
        out.append(_key_token(token))
        i = end + 1

    return "".join(out)


def _key_token(token: str) -> str:
    normalized = token.strip().lower()
    aliases = {
        "cr": "\r",
        "enter": "\r",
        "return": "\r",
        "esc": "\x1b",
        "escape": "\x1b",
        "tab": "\t",
        "s-tab": "\x1b[Z",
        "bs": "\x7f",
        "backspace": "\x7f",
        "del": "\x1b[3~",
        "delete": "\x1b[3~",
        "ins": "\x1b[2~",
        "insert": "\x1b[2~",
        "space": " ",
        "lt": "<",
        "up": "\x1b[A",
        "down": "\x1b[B",
        "right": "\x1b[C",
        "left": "\x1b[D",
        "home": "\x1b[H",
        "end": "\x1b[F",
        "pageup": "\x1b[5~",
        "pagedown": "\x1b[6~",
    }
    if normalized in aliases:
        return aliases[normalized]

    if normalized.startswith("c-") or normalized.startswith("ctrl-"):
        key = normalized.split("-", 1)[1]
        if len(key) == 1 and "a" <= key <= "z":
            return chr(ord(key) - ord("a") + 1)

    if normalized.startswith("m-") or normalized.startswith("a-") or normalized.startswith("alt-"):
        key = normalized.split("-", 1)[1]
        if len(key) == 1:
            return "\x1b" + key

    if normalized.startswith("f") and normalized[1:].isdigit():
        function_keys = {
            1: "\x1bOP",
            2: "\x1bOQ",
            3: "\x1bOR",
            4: "\x1bOS",
            5: "\x1b[15~",
            6: "\x1b[17~",
            7: "\x1b[18~",
            8: "\x1b[19~",
            9: "\x1b[20~",
            10: "\x1b[21~",
            11: "\x1b[23~",
            12: "\x1b[24~",
        }
        number = int(normalized[1:])
        if number in function_keys:
            return function_keys[number]

    raise ValueError(f"unknown key token <{token}>")
