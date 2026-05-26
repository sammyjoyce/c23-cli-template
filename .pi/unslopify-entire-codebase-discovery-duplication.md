### Lane
- Name: Lane 1 obvious duplication / DRY cleanup
- Scope inspected: Entire repository under `/home/sammy/code/c23-cli-template`, read-only: C/C headers in `src/`, Zig build/tests, GitHub workflows, pre-commit/renovate/dependabot/devcontainer config, template scripts under `.template/`, demo script, docs/examples/README/OpenCLI metadata. Excluded from cleanup recommendations: `.git/`, build outputs/cache (not present in inspected tree), and generated/vendor-style surfaces unless explicitly called out as risky.

### Evidence collected
- `git status --short` — no output; worktree was clean before inspection.
- `find` count summary — 75 files excluding `.git`; inspected categories include 24 C/H files, 2 Zig files, 6 shell/just files, 3 workflow YAML files, and 18 Markdown/docs files.
- Static duplicate-line scan summary — notable exact repeats: `test/main.zig` has 8 `std.process.Child.run` blocks and 8 paired stdout/stderr defers; `src/tui/tui.c`/TUI code has 5 centered-window `x/y` calculations and 3 dialog create/destroy sequences; `.github/workflows/ci.yaml` repeats checkout setup 9 times and has an exact placeholder `find ... grep` command repeated twice; `.template/replacer.sh` repeats the same `to_words`/empty check pattern in transform helpers.
- `build.zig:70-87` and `build.zig:90-97` — common C flags plus `APP_VERSION`, `APP_NAME`, `APP_GIT_COMMIT`, `APP_BUILD_DATE`, and `ENABLE_TUI` flag assembly is duplicated between base sources and TUI sources.
- `build.zig:175-187` — `fmt` and `fmt-check` repeat identical `.paths = &.{ "build.zig", "src", "test" }` setup with only `.check` differing.
- `src/core/config.c:292-323` — `app_config_set_program_name`, `app_config_set_command`, and `app_config_set_config_file` repeat the same `if (config && value) { free(existing); existing = strdup(value); }` pattern.
- `src/core/config.c:64-79` — `app_config_destroy` manually guards `free()` for three singleton strings and each command arg; this is minor duplication but behavior-neutral to simplify because `free(NULL)` is valid.
- `test/main.zig:48-145` — each CLI test repeats `std.process.Child.run`, allocator assignment, argv construction, and stdout/stderr deallocation; command/assertions differ.
- `.github/workflows/ci.yaml:225` and `.github/workflows/ci.yaml:229` — identical long `find . ... -exec grep -l ...` expression is run once for counting and again for printing placeholder files.
- `.template/replacer.sh:119-128` — `to_snake` and `to_kebab` differ only in delimiter (`_` vs `-`) after identical `to_words` and empty-result handling.
- `src/tui/tui.c:381-419`, `src/tui/tui.c:427-475`, and `src/tui/tui.c:484-519` — `tui_show_message`, `tui_confirm`, and `tui_input_dialog` repeat centered dialog sizing, `tui_create_window`, border/title setup, refresh, destroy, `touchwin(stdscr)`, and `refresh()`.
- `src/main.c:73-94`, `src/cli/help.c:25-38`, `src/cli/help.c:73-100`, `opencli.json:108-172`, `README.md:102-127` — command/option names and descriptions are repeated across runtime dispatch, concise help, verbose help, OpenCLI contract, and docs. This is real duplication but crosses public CLI/docs surfaces.
- `src/core/config.c:145-178` and `src/io/input.c:109-157` — both open/read whole files into secure memory, but error logging, size checks, and return types differ; not a safe obvious DRY change without behavior review.
- `.template/TEMPLATE_README.md:3`, `.template/TEMPLATE_README.md:7`, `.template/TEMPLATE_USAGE.md:20-28` — repeated/corrupted phrase fragments such as `A modern A modern CLI...`; noted as AI-slop/docs corruption rather than Lane 1 behavior-preserving DRY.
- No build/test validation was run; commands such as `zig build test` would create/update `zig-out`/`.zig-cache`, so validation is left to the parent after selecting edits.

### High-confidence candidates
For each candidate:
- ID suggestion: DRY-BUILD-C-FLAGS
- Files/symbols: `build.zig` / `build` function, base/TUI C source flag assembly and format path setup.
- Evidence: `build.zig:70-87` and `build.zig:90-97` duplicate flag construction; `build.zig:175-187` duplicates fmt paths.
- Why behavior should be preserved: A small local helper or shared constant can construct the same ordered flag slices and reuse the same path list without changing source files, compiler flags, build steps, or public CLI behavior. TUI gating remains controlled by `enable_tui`.
- Validation needed: `zig build -Doptimize=Debug`, `zig build -Doptimize=ReleaseSafe`, `zig build fmt-check`, and if ncurses/PDCurses is available, `zig build -Denable-tui=true`.

- ID suggestion: DRY-CONFIG-STRING-SETTERS
- Files/symbols: `src/core/config.c` / `app_config_set_program_name`, `app_config_set_command`, `app_config_set_config_file`; optionally `app_config_destroy` free calls.
- Evidence: `src/core/config.c:292-323` repeats the same guard/free/`strdup` assignment pattern for three struct fields; `src/core/config.c:64-79` repeats guarded frees.
- Why behavior should be preserved: A `static` helper taking `char **slot` and `const char *value` can exactly preserve current semantics, including ignoring `NULL` config/value and keeping the existing `strdup`-failure behavior unless deliberately changed. No header/API changes are needed.
- Validation needed: `zig build test`; smoke-test `./zig-out/bin/myapp --help`, `./zig-out/bin/myapp hello Alice`, and `./zig-out/bin/myapp -c /tmp/nonexistent info` after a build to confirm argument/config setter behavior is unchanged.

- ID suggestion: DRY-ZIG-TEST-RUNNER
- Files/symbols: `test/main.zig` / `testHelp`, `testVersion`, `testCommands`, `testInvalidCommand`.
- Evidence: `test/main.zig:48-145` repeats `std.process.Child.run`, `.allocator = allocator`, `argv` arrays beginning with `./zig-out/bin/myapp`, and stdout/stderr defers for every test case.
- Why behavior should be preserved: A test-only helper can run `myapp` with supplied args and return the same `std.process.Child.RunResult` to existing assertions. Product code and CLI behavior are untouched.
- Validation needed: `zig build test`; if helper changes allocation ownership, run under Zig test allocator leak checks (default for `std.testing.allocator`).

- ID suggestion: DRY-CI-PLACEHOLDER-FIND
- Files/symbols: `.github/workflows/ci.yaml` / `validate-template` job, `Check for template placeholders` step.
- Evidence: `.github/workflows/ci.yaml:225` and `.github/workflows/ci.yaml:229` repeat the same long `find`/`grep -l` expression; only count-vs-print differs.
- Why behavior should be preserved: Capturing the placeholder file list once and deriving both count and output from it avoids duplicate traversal while preserving the same include/exclude patterns and failure condition.
- Validation needed: `actionlint .github/workflows/ci.yaml` if available; otherwise run a shell-only extraction of the step command in a throwaway copy or use `bash -n` on the extracted script body.

- ID suggestion: DRY-TEMPLATE-TRANSFORM-DELIMITER
- Files/symbols: `.template/replacer.sh` / `to_snake`, `to_kebab`.
- Evidence: `.template/replacer.sh:119-128` has identical body structure except output delimiter.
- Why behavior should be preserved: A private helper such as `to_delimited "$value" "_"|"-"` can preserve current `to_words` normalization and empty-string behavior while reducing transform drift. This affects only template replacement internals.
- Validation needed: `.template/replacer.sh --dry-run -v` with representative `PROJECT_NAME='My CLI App'` and compare reported transformed values for `PROJECT_NAME_SNAKE`/`PROJECT_NAME_KEBAB` before and after.

### Risky or uncertain candidates
- Candidate: Extract a shared TUI centered-dialog/window helper for `tui_show_message`, `tui_confirm`, and `tui_input_dialog`.
- Uncertainty: The duplication is clear, but TUI behavior is interactive and not covered by automated tests; subtle changes to window dimensions, refresh order, title drawing, or return-on-allocation-failure behavior could be user-visible.
- Evidence still needed: Manual or scripted terminal validation for `myapp menu` with `-Denable-tui=true`; ideally a small ncurses smoke test for dialog creation/teardown.

- Candidate: Centralize command/option metadata used by `src/main.c`, `src/cli/help.c`, `opencli.json`, README examples, and tests.
- Uncertainty: This crosses runtime behavior, public help text, OpenCLI metadata, and documentation. A table-driven command registry could be useful but is a design change rather than low-risk DRY cleanup.
- Evidence still needed: Owner decision on canonical source of truth and compatibility requirements for `opencli.json` and help output text.

- Candidate: Reuse `app_read_input_from_file` inside `app_config_load_file`.
- Uncertainty: Both read whole files, but current behavior differs on size limits, log levels/messages, errno handling, return type, and whether missing default config is success. Refactoring could change error paths.
- Evidence still needed: Tests around config file loading, large config files, unreadable files, and expected logs/errors.

- Candidate: Use YAML anchors/composite steps for repeated checkout/setup-Zig workflow steps.
- Uncertainty: Repeated checkout/setup is obvious in `.github/workflows/ci.yaml`, but workflow readability and GitHub Actions YAML-anchor support/tooling expectations may vary; changing many jobs is more CI-design than local cleanup.
- Evidence still needed: `actionlint` baseline and maintainer preference for anchors vs repetition in workflows.

### Generated/vendor/public API concerns
- `.template/*` files and `.github/workflows/template-cleanup.yml` are template automation; small changes can affect newly generated downstream repositories, so validate with dry-run/template-cleanup flows.
- `opencli.json` is a documented CLI contract/public metadata surface; do not silently reshape command/option definitions as a DRY cleanup.
- `src/**/*.h` headers expose C APIs; high-confidence candidates above avoid changing prototypes or exported structs/enums.
- `README.md`, `docs/`, `examples/`, and `docs/demos/README.md` contain public documentation and generated/demo-oriented content; avoid broad deduplication that changes examples or template promises.
- No vendored dependency directories or lockfiles were inspected as cleanup targets. `build.zig.zon` is dependency/build metadata and should be treated cautiously.

### Out-of-scope findings
- Finding: `.template/setup.sh:102-104` calls `bash ./.template/setup.sh` during cleanup, which appears recursive rather than removing template files.
- Why out of scope: This is likely a behavior bug in template cleanup, not an obvious duplication/DRY cleanup.

- Finding: `.github/workflows/template-cleanup.yml:55` exports `PROJECT_AUTHOR`, while `.template/replacer.sh` consumes normalized `AUTHOR_NAME` from `template-vars.json`.
- Why out of scope: This is workflow/template integration correctness, not Lane 1 duplication.

- Finding: `.template/TEMPLATE_README.md` and `.template/TEMPLATE_USAGE.md` contain repeated corrupted phrases (`A modern A modern CLI...`, broken words such as `commA modern...`).
- Why out of scope: This is Lane 8 AI-slop/docs corruption cleanup, not behavior-preserving DRY.

- Finding: `test-runner.sh:1-4` is a stub that always exits 0.
- Why out of scope: This is unused/stub validation behavior, not duplication cleanup.

### Recommended next checks
- Before edits, run the smallest baseline that exercises the high-confidence source/test/build candidates: `zig build test`.
- If editing `build.zig`, also run `zig build -Doptimize=ReleaseSafe` and, where ncurses/PDCurses is installed, `zig build -Denable-tui=true`.
- For the CI candidate, run `actionlint .github/workflows/ci.yaml` if available, or extract the `validate-template` shell body and run `bash -n` on it.
