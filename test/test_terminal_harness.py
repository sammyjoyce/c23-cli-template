"""Unit tests for the reusable terminal test harness."""

from __future__ import annotations

import sys
import unittest

import terminal_harness as terminal


class KeyParserTests(unittest.TestCase):
    def test_literal_text_and_named_keys(self) -> None:
        self.assertEqual("open\r", terminal.parse_keys("open<CR>"))
        self.assertEqual("<\x1b[A\x1b[B", terminal.parse_keys("<lt><Up><Down>"))

    def test_modifier_and_function_keys(self) -> None:
        self.assertEqual("\x03", terminal.parse_keys("<C-c>"))
        self.assertEqual("\x1bx", terminal.parse_keys("<M-x>"))
        self.assertEqual("\x1bOP\x1b[24~", terminal.parse_keys("<F1><F12>"))

    def test_unclosed_angle_is_literal_text(self) -> None:
        self.assertEqual("a <broken", terminal.parse_keys("a <broken"))

    def test_unknown_token_fails_fast(self) -> None:
        with self.assertRaisesRegex(ValueError, r"unknown key token <wat>"):
            terminal.parse_keys("<wat>")


class RunCliTests(unittest.TestCase):
    def test_run_cli_captures_exit_status_and_streams(self) -> None:
        script = (
            "import sys; "
            "print('stdout-value'); "
            "print('stderr-value', file=sys.stderr); "
            "raise SystemExit(7)"
        )

        result = terminal.run_cli(["-c", script], binary=sys.executable)

        self.assertEqual(7, result.exit_code, msg=result._debug_message())
        result.assert_stdout_contains(self, "stdout-value")
        result.assert_stderr_contains(self, "stderr-value")


if __name__ == "__main__":
    unittest.main(verbosity=2)
