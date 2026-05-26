"""Example PTY-backed tests for the optional ncurses TUI."""

from __future__ import annotations

import unittest

import terminal_harness as terminal


def select_menu_item(session: terminal.PtySession, index: int) -> None:
    """Select a main-menu item by zero-based index from the default selection."""

    session.send_keys("j" * index + "<CR>")


@unittest.skipUnless(terminal.tui_enabled(), "rebuild with -Denable-tui=true to run TUI scenarios")
@unittest.skipUnless(terminal.pty_available(), terminal.pty_unavailable_reason())
class TuiScenarioTests(unittest.TestCase):
    def test_all_demo_screens_render_and_exit_cleanly(self) -> None:
        with terminal.PtySession(
            [terminal.binary_path(), "menu"],
            cols=80,
            rows=24,
            timeout=8,
        ) as session:
            session.expect_text("Starter Showcase", timeout=5)
            for label in (
                "Overview",
                "System Information",
                "Input Dialog",
                "Progress Pattern",
                "Layout Pattern",
                "Exit",
            ):
                session.expect_text(label, timeout=1)

            select_menu_item(session, 0)
            session.expect_text("Starter Overview", timeout=5)
            session.expect_text("C23 modules", timeout=1)
            session.send_keys("x")
            session.expect_text("Starter Showcase", timeout=5)

            select_menu_item(session, 1)
            session.expect_text("System Information", timeout=5)
            session.expect_text("Application:", timeout=1)
            session.expect_text("Terminal Size:", timeout=1)
            session.expect_text("Colors Supported:", timeout=1)
            session.send_keys("x")
            session.expect_text("Starter Showcase", timeout=5)

            select_menu_item(session, 2)
            session.expect_text("Input Dialog", timeout=5)
            session.expect_text("Enter a display name:", timeout=1)
            session.send_keys("Ada Lovelace<CR>")
            session.expect_text("Input Captured", timeout=5)
            session.expect_text("Hello, Ada Lovelace.", timeout=1)
            session.send_keys("x")
            session.expect_text("Starter Showcase", timeout=5)

            select_menu_item(session, 3)
            session.expect_text("Progress Complete", timeout=8)
            session.expect_text("window lifecycle", timeout=1)
            session.send_keys("x")
            session.expect_text("Starter Showcase", timeout=5)

            select_menu_item(session, 4)
            session.expect_text("Layout Pattern", timeout=5)
            session.expect_text("Composable terminal UI", timeout=1)
            session.expect_text("Enter/Esc closes this panel", timeout=1)
            session.send_keys("<Esc>")
            session.expect_text("Starter Showcase", timeout=5)

            session.send_keys("q")
            session.expect_text("Return to the shell?", timeout=5)
            session.send_keys("n")
            session.expect_text("Starter Showcase", timeout=5)

            select_menu_item(session, 5)
            session.expect_text("Return to the shell?", timeout=5)
            session.send_keys("y")
            self.assertEqual(0, session.wait_for_exit(timeout=5))


if __name__ == "__main__":
    unittest.main(verbosity=2)
