"""Minimal PTY smoke coverage for the Python terminal-test fallback.

The full TUI scenario suite lives in ``terminal_vt_scenarios.c`` for the Ghostty
VT backend. This file only verifies that the fallback harness can open and close
the demo menu when Ghostty is unavailable or the Python backend is forced.
"""

from __future__ import annotations

import unittest

import terminal_harness as terminal


@unittest.skipUnless(terminal.tui_enabled(), "rebuild with -Denable-tui=true to run TUI scenarios")
@unittest.skipUnless(terminal.pty_available(), terminal.pty_unavailable_reason())
class PythonFallbackTuiSmokeTests(unittest.TestCase):
    def test_menu_opens_and_exits_cleanly(self) -> None:
        with terminal.PtySession(
            [terminal.binary_path(), "menu"],
            cols=80,
            rows=24,
            timeout=8,
        ) as session:
            session.expect_text("Starter Showcase", timeout=5)
            session.send_keys("q")
            session.expect_text("Return to the shell?", timeout=5)
            session.send_keys("y")
            self.assertEqual(0, session.wait_for_exit(timeout=5))


if __name__ == "__main__":
    unittest.main(verbosity=2)
