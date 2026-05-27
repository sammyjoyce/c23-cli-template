# Changelog

All notable changes to this template will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- Initial template structure with a C23 codebase driven by Zig 0.16.
- Layered configuration (defaults -> config file -> env -> CLI flags).
- Table-driven CLI dispatch with `hello`, `echo`, `info`, `doctor`, and
  `menu` commands.
- Optional ncurses/PDCurses TUI build with reusable windows, menus, dialogs,
  and progress bars.
- JSON output mode plus human-readable formatting helpers.
- CLI contract tests (`zig build test`), in-process unit tests
  (`zig build unit-test`), and optional Ghostty-backed PTY tests
  (`zig build terminal-test`).
- Template scaffolding (`.template/`, `.github/workflows/template-cleanup.yml`)
  with interactive and non-interactive setup.

[Unreleased]: https://github.com/yourusername/myapp
