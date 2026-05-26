"""Minimal CLI smoke coverage for the Python terminal-test fallback.

The canonical CLI contract suite lives in ``terminal_vt_scenarios.c`` for the
Ghostty VT backend. Keep this fallback intentionally small so projects do not
maintain two parallel scenario corpora.
"""

from __future__ import annotations

import json
import unittest

import terminal_harness as terminal


class PythonFallbackCliSmokeTests(unittest.TestCase):
    def test_binary_starts_successfully(self) -> None:
        result = terminal.run_cli(["--version"])

        terminal.assert_success(self, result)
        self.assertGreater(len(result.stdout), 0, msg=terminal.command_debug_message(result))

    def test_json_info_is_parseable(self) -> None:
        result = terminal.run_cli(["--json", "info"])

        terminal.assert_success(self, result)
        payload = json.loads(result.stdout)
        self.assertEqual("1.0", payload["format_version"])
        self.assertIsInstance(payload.get("features", {}).get("tui"), bool)


if __name__ == "__main__":
    unittest.main(verbosity=2)
