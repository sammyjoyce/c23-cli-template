# Changelog

All notable changes to this template are documented here.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Changed

- TUI support is compiled in by default; pass `-Denable-tui=false` to build a
  headless-only binary.
- Bare `myapp` on a TTY now opens the interactive TUI menu, while command
  invocations continue to use the CLI command table.
- Command output defaults to versioned JSON when stdout is not a terminal;
  pass `--plain` to preserve human text under pipes or redirection.

### Added

- Experimental bare non-TTY headless protocol: read a JSON request from stdin
  and dispatch it through the existing command table.

## [0.1.0]

### Added

- Initial template: C23 sources under `src/`, Zig 0.16 build system, opt-in ncurses/PDCurses TUI behind `-Denable-tui=true`.
- Live OpenCLI contract. `myapp opencli` prints the spec, and `zig build test` fails if it drifts from `opencli.json`.
- Reusable `tui-menu-lib` static library target with installed headers.
- Three test layers: in-process unit tests (`zig build unit-test`), CLI contract tests, and Ghostty-VT-backed PTY/TUI scenarios.
- GitHub Actions CI on Linux, macOS, and Windows, plus `clang-tidy`, `cppcheck`, Gitleaks, OpenSSF Scorecard, SBOM generation, and pinned action versions.
- Template cleanup workflow plus `.template/setup.sh` and `replacer.sh` for customizing a generated project.
- Nix dev shell, devcontainer, pre-commit configuration (clang-format, markdownlint, conventional-commit), and Dependabot.

[Unreleased]: https://github.com/yourusername/yourproject/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/yourusername/yourproject/releases/tag/v0.1.0
