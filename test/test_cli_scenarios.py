"""Example end-to-end tests for non-interactive CLI behavior."""

from __future__ import annotations

import json
import unittest

import terminal_harness as terminal


class CliScenarioTests(unittest.TestCase):
    def test_help_is_human_readable(self) -> None:
        result = terminal.run_cli(["--help"])

        result.assert_success(self)
        result.assert_stdout_contains(self, "USAGE")
        result.assert_stdout_contains(self, "COMMANDS")
        result.assert_stdout_contains(self, "doctor")

    def test_json_info_is_versioned_machine_output(self) -> None:
        result = terminal.run_cli(["--json", "info"])

        result.assert_success(self)
        payload = json.loads(result.stdout)
        self.assertEqual("1.0", payload["format_version"])
        self.assertIn("features", payload)
        self.assertIsInstance(payload["features"].get("tui"), bool)

    def test_unknown_command_reports_actionable_error(self) -> None:
        result = terminal.run_cli(["not-a-command"])

        self.assertNotEqual(0, result.exit_code, msg=result._debug_message())
        result.assert_stderr_contains(self, "Unknown command: not-a-command")
        result.assert_stderr_contains(self, "--help")


if __name__ == "__main__":
    unittest.main(verbosity=2)
