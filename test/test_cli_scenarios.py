"""Example end-to-end tests for non-interactive CLI behavior."""

from __future__ import annotations

import json
from pathlib import Path
import tempfile
import unittest

import terminal_harness as terminal


class CliScenarioTests(unittest.TestCase):
    def test_help_is_human_readable(self) -> None:
        result = terminal.run_cli(["--help"])

        terminal.assert_success(self, result)
        terminal.assert_stdout_contains(self, result, "USAGE")
        terminal.assert_stdout_contains(self, result, "COMMANDS")
        terminal.assert_stdout_contains(self, result, "doctor")

    def test_version_starts_successfully(self) -> None:
        result = terminal.run_cli(["--version"])

        terminal.assert_success(self, result)
        self.assertGreater(len(result.stdout), 0, msg=terminal.command_debug_message(result))

    def test_builtin_commands_render_expected_output(self) -> None:
        hello = terminal.run_cli(["hello"])
        terminal.assert_success(self, hello)
        terminal.assert_stdout_contains(self, hello, "Hello, World!")

        named_hello = terminal.run_cli(["hello", "Alice"])
        terminal.assert_success(self, named_hello)
        terminal.assert_stdout_contains(self, named_hello, "Hello, Alice!")

        echo = terminal.run_cli(["echo", "test", "message"])
        terminal.assert_success(self, echo)
        terminal.assert_stdout_contains(self, echo, "test message")

        info = terminal.run_cli(["info"])
        terminal.assert_success(self, info)
        terminal.assert_stdout_contains(self, info, "Application:")
        terminal.assert_stdout_contains(self, info, "Version:")

    def test_json_info_is_versioned_machine_output(self) -> None:
        result = terminal.run_cli(["--json", "info"])

        terminal.assert_success(self, result)
        payload = json.loads(result.stdout)
        self.assertEqual("1.0", payload["format_version"])
        self.assertIn("features", payload)
        self.assertIsInstance(payload["features"].get("tui"), bool)

    def test_quiet_json_commands_suppress_stdout(self) -> None:
        info = terminal.run_cli(["--quiet", "--json", "info"])
        terminal.assert_success(self, info)
        self.assertEqual("", info.stdout, msg=terminal.command_debug_message(info))

        doctor = terminal.run_cli(["--quiet", "--json", "doctor"])
        terminal.assert_success(self, doctor)
        self.assertEqual("", doctor.stdout, msg=terminal.command_debug_message(doctor))

    def test_doctor_reports_binary_state(self) -> None:
        result = terminal.run_cli(["doctor"])

        terminal.assert_success(self, result)
        terminal.assert_stdout_contains(self, result, "doctor")
        terminal.assert_stdout_contains(self, result, "binary")

    def test_plain_mode_disables_forced_color(self) -> None:
        result = terminal.run_cli(["--plain", "doctor"], env={"FORCE_COLOR": "1"})

        terminal.assert_success(self, result)
        terminal.assert_stdout_contains(self, result, "color_output")
        terminal.assert_stdout_contains(self, result, "disabled for this output")

    def test_command_arguments_are_not_global_config_flags(self) -> None:
        result = terminal.run_cli(["echo", "-c", "/definitely/not/a/config.json"])

        terminal.assert_success(self, result)
        terminal.assert_stdout_contains(self, result, "-c /definitely/not/a/config.json")

    def test_explicit_config_file_failures_are_visible(self) -> None:
        result = terminal.run_cli(["--config", "/definitely/not/a/config.json", "hello"])

        self.assertNotEqual(0, result.exit_code, msg=terminal.command_debug_message(result))
        terminal.assert_stderr_contains(self, result, "failed to load config")
        terminal.assert_stderr_contains(self, result, "/definitely/not/a/config.json")

    def test_verbose_mode_emits_diagnostics_on_stderr(self) -> None:
        result = terminal.run_cli(["--verbose", "hello"])

        terminal.assert_success(self, result)
        terminal.assert_stdout_contains(self, result, "Hello, World!")
        terminal.assert_stderr_contains(self, result, "[INFO]")

    def test_invalid_default_config_does_not_leak_partial_settings(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            config_path = Path(tmp) / "config.json"
            config_path.write_text('{"quiet":true,"ignored":{"nested":true}}', encoding="utf-8")

            result = terminal.run_cli(["hello"], env={"APP_CONFIG_PATH": str(config_path)})

        terminal.assert_success(self, result)
        terminal.assert_stdout_contains(self, result, "Hello, World!")

    def test_valid_flat_config_skips_unknown_scalar_keys(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            config_path = Path(tmp) / "config.json"
            config_path.write_text('{"ignored":"debug","quiet":true}', encoding="utf-8")

            result = terminal.run_cli(["--config", str(config_path), "hello"])

        terminal.assert_success(self, result)
        self.assertEqual("", result.stdout, msg=terminal.command_debug_message(result))

    def test_unknown_command_reports_actionable_error(self) -> None:
        result = terminal.run_cli(["not-a-command"])

        self.assertNotEqual(0, result.exit_code, msg=terminal.command_debug_message(result))
        terminal.assert_stderr_contains(self, result, "Unknown command: not-a-command")
        terminal.assert_stderr_contains(self, result, "--help")


if __name__ == "__main__":
    unittest.main(verbosity=2)
