#!/usr/bin/env python3
"""Run the Python terminal scenario suite.

This script is called by `zig build terminal-test`, but it can also be run
manually while iterating on tests:

    python3 test/run_terminal_tests.py --binary ./zig-out/bin/myapp
"""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import sys
import unittest

import terminal_harness as terminal


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--binary",
        default=os.environ.get(terminal.APP_BINARY_ENV, "./zig-out/bin/myapp"),
        help="path to the binary under test",
    )
    parser.add_argument(
        "--tui-enabled",
        choices=("0", "1", "auto"),
        default=os.environ.get(terminal.APP_TUI_ENABLED_ENV, "0"),
        help="whether the binary was built with -Denable-tui=true",
    )
    parser.add_argument(
        "--pattern",
        default="test_*.py",
        help="unittest discovery pattern",
    )
    return parser.parse_args()


def resolve_binary_path(value: str) -> Path:
    path = Path(value).expanduser()
    if path.is_absolute():
        return path

    cwd_candidate = (Path.cwd() / path).resolve()
    if cwd_candidate.exists():
        return cwd_candidate

    return (terminal.project_root() / path).resolve()


def detect_tui_enabled(binary: Path) -> bool:
    result = terminal.run_cli(["--json", "info"], binary=binary)
    if result.exit_code != 0:
        return False
    try:
        payload = json.loads(result.stdout)
    except json.JSONDecodeError:
        return False
    return bool(payload.get("features", {}).get("tui"))


def main() -> int:
    args = parse_args()
    binary = resolve_binary_path(args.binary)
    if not binary.is_file():
        print(f"terminal tests: binary does not exist: {binary}", file=sys.stderr)
        return 2

    os.environ[terminal.APP_BINARY_ENV] = str(binary)
    if args.tui_enabled == "auto":
        os.environ[terminal.APP_TUI_ENABLED_ENV] = "1" if detect_tui_enabled(binary) else "0"
    else:
        os.environ[terminal.APP_TUI_ENABLED_ENV] = args.tui_enabled

    test_dir = Path(__file__).resolve().parent
    loader = unittest.defaultTestLoader
    suite = loader.discover(str(test_dir), pattern=args.pattern, top_level_dir=str(test_dir))
    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)
    return 0 if result.wasSuccessful() else 1


if __name__ == "__main__":
    raise SystemExit(main())
