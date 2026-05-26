### Lane
- Name: Lane 3+7 — unused code/exports/dependencies/scripts/entry points and deprecated/legacy/fallback/migration leftovers
- Scope inspected: Tracked files under `src/`, `test/`, `build.zig`, `build.zig.zon`, `justfile`, `opencli.json`, `.github/`, `.devcontainer/`, `.template/`, `scripts/`, `docs/`, `examples/`, `README.md`, and `test-runner.sh`; excluded `.git`, `.pi`, `zig-out`, `.zig-cache`, temp dirs. Read-only against project/source files.

### Evidence collected
- `git status --short` — only `?? .pi/` was dirty before discovery; no tracked project edits made.
- `test-runner.sh:1-4` plus `git grep -n test-runner.sh -- <scoped paths>` returned no references — tracked no-op script exits success and is not called by scoped project files.
- `src/main.c:34`, `src/main.c:222` plus `git grep app_thread_id` only returns those two lines — thread-local variable exists only to be marked unused.
- `build.zig:28`, `.github/workflows/ci.yaml:78-85`, `.github/workflows/ci.yaml:88-118` — CI installs ncurses/PDCurses, but workflow build/test invocations shown there do not pass `-Denable-tui=true`; `git grep -n -- -Denable-tui .github/workflows/ci.yaml` returned no workflow hits.
- `justfile:47-54` plus `git grep -n -E 'just (install|docs)|^install:|^docs:' -- <scoped paths>` only hit `justfile` — `install`/`docs` recipes are documented only by `justfile` and only echo placeholder text.
- `.devcontainer/devcontainer.json:80-83` plus `git ls-files .devcontainer` showed only `.devcontainer/devcontainer.json` and `.devcontainer/post-create.sh` — mount references absent `.devcontainer/scripts`.
- `build.zig:55-56`, `src/io/input.c:24`, `src/io/input.c:109`, `src/io/input.c:161`, `src/io/output.c:15`, `src/io/output.c:36`, `src/io/output.c:56`; scoped `git grep` found input/output symbols only in their `.c/.h` definitions except `app_output_format` calling `app_output` — I/O modules are built but not used by commands.
- `src/cli/args.c:46-67`, `src/main.c:73-90`, `src/io/output.c:21-33`, `opencli.json:62-106` — CLI parses/advertises output/config options while implemented commands print directly with `printf`.
- `src/core/config.c:173-176`, `src/main.c:51-55`, `src/cli/args.c:60-67` — default config load happens before arg parsing, `--config` only stores a path, and file content is read but not parsed.
- `src/main.c:73-94`, `opencli.json:108-166`, `scripts/create-demo.sh:139`, `scripts/create-demo.sh:199-225`, `docs/demos/README.md:31-33`, `docs/demos/README.md:58-84` — implemented commands are `hello`, `echo`, `info`, and optional `menu`, while demo assets call `progress`, `config`, `process`, and `validate`.
- `.github/workflows/template-cleanup.yml:6-8`, `.template/README.md:44-49` — workflow is manual-only while template docs say GitHub Actions automatically runs cleanup.
- `.github/workflows/template-cleanup.yml:41-65`, `.template/setup.sh:45-46`, `.template/setup.sh:100-105` — workflow installs `jq`/`sd`, then calls interactive `setup.sh`, which requires `gum` and recursively calls itself when cleanup is confirmed.
- `.github/workflows/template-cleanup.yml:52-58`, `.template/template-vars.json:37-41`, `.template/setup.sh:83-84` — workflow exports `PROJECT_AUTHOR`, but template variable/scripts expect `AUTHOR_NAME`.
- `build.zig:31-34`, `build.zig:134-139`, `build.zig.zon:5-9`, `README.md:20`, `README.md:247`, `opencli.json:233` — `aro` dependency is fetched and its headers installed, but scoped build code only claims “Aro compiler”; no compile step wires Aro as the compiler.

### High-confidence candidates
- ID suggestion: `unused-test-runner-stub`
  - Files/symbols: `test-runner.sh`
  - Evidence: `test-runner.sh:1-4` is just a shebang/comment/`exit 0`; scoped `git grep` found no callers or docs references.
  - Why behavior should be preserved: Current behavior is a false-positive success path; documented test entry points are `zig build test`/`just test`, not this script.
  - Validation needed: `git grep -n test-runner.sh -- <scoped paths>` stays empty; run `zig build test` after removal if parent edits.

- ID suggestion: `unused-thread-local-main`
  - Files/symbols: `src/main.c` `app_thread_id`
  - Evidence: `src/main.c:34` defines `static _Thread_local int app_thread_id`; `src/main.c:222` only casts it to void; no other scoped references.
  - Why behavior should be preserved: Variable has no reads/writes and exists solely to suppress its own unused warning.
  - Validation needed: `zig build check` or at least `zig build` after deletion.

- ID suggestion: `unused-ci-tui-dependency-installs`
  - Files/symbols: `.github/workflows/ci.yaml` ncurses/PDCurses install steps
  - Evidence: `build.zig:28` defaults `enable_tui` to false; `.github/workflows/ci.yaml:78-104` installs ncurses/PDCurses; `.github/workflows/ci.yaml:106-118` runs `zig build install/check/test` without `-Denable-tui=true`; no workflow `-Denable-tui` hits.
  - Why behavior should be preserved: Default CI does not build/link the TUI sources, so default build/test behavior should not require curses packages.
  - Validation needed: GitHub Actions test matrix, or locally inspect `zig build --help`/run `zig build check` without ncurses installed in a clean environment.

- ID suggestion: `placeholder-just-recipes`
  - Files/symbols: `justfile` recipes `install`, `docs`
  - Evidence: `justfile:47-54` only echoes “No additional installation needed” / “Documentation generation not configured”; scoped grep found no `just install` or `just docs` references outside the recipe definitions.
  - Why behavior should be preserved: Removing or hiding no-op placeholder recipes avoids advertising unsupported workflows while leaving real build/test/check recipes untouched.
  - Validation needed: `just --list` / `just help` after edit; `git grep -n -E 'just (install|docs)' -- <scoped paths>`.

- ID suggestion: `stale-devcontainer-scripts-mount`
  - Files/symbols: `.devcontainer/devcontainer.json` mount of `.devcontainer/scripts`
  - Evidence: `.devcontainer/devcontainer.json:80-83` binds `.devcontainer/scripts`; `git ls-files .devcontainer` lists only `devcontainer.json` and `post-create.sh`.
  - Why behavior should be preserved: Removing a mount to an absent tracked directory should not change project setup other than eliminating stale container configuration.
  - Validation needed: Open/rebuild devcontainer, or run `devcontainer up` if available.

### Risky or uncertain candidates
- Candidate: Built-but-unused I/O modules and public functions (`src/io/input.c`, `src/io/output.c`, `app_read_input_*`, `app_output*`).
  - Uncertainty: These are header-declared, non-static APIs and docs describe an I/O module; removing them could break template users or planned examples.
  - Evidence still needed: Decide whether project headers are internal only; if yes, run a full scoped symbol-use check and remove modules/includes/build entries together. If no, wire commands to use `app_output*` instead of deleting.

- Candidate: Vestigial output/config CLI options (`--quiet`, `--verbose`, `--json`, `--plain`, `--config`) and config file path handling.
  - Uncertainty: They are part of `opencli.json`, help text, README, and examples, so cleanup by deletion changes public CLI contract; cleanup by implementation is larger than this lane.
  - Evidence still needed: Owner decision on contract: keep and implement output/config semantics, or remove options/docs/OpenCLI entries as template leftovers.

- Candidate: Demo and advanced examples for unsupported commands (`progress`, `config`, `process`, `validate`, and many generic commands in `examples/advanced-usage.md`).
  - Uncertainty: Some docs may be intentionally aspirational template examples rather than current binary behavior.
  - Evidence still needed: Decide whether examples must be executable against current `myapp`; if yes, trim `scripts/create-demo.sh`, `docs/demos/README.md`, and `examples/advanced-usage.md` to implemented commands or mark future/custom-command examples explicitly.

- Candidate: Template cleanup workflow/script path is stale or broken (`template-cleanup.yml`, `.template/setup.sh`).
  - Uncertainty: This is public template onboarding automation; removing or rewriting it changes new-repository setup behavior.
  - Evidence still needed: Dry-run the workflow in a disposable template clone; decide if cleanup should be manual-only, automatic, or split into non-interactive cleanup vs interactive setup.

- Candidate: Aro dependency appears legacy/misrepresented.
  - Uncertainty: `aro_dep` is used to install Aro headers, so it is not purely unused; docs/CLI say “Aro compiler,” but build does not visibly use it as the C compiler.
  - Evidence still needed: Confirm intended compiler path with Zig/Aro owner; inspect generated compile command in a disposable build before removing dependency/docs.

- Candidate: `opencli.json` advertises `menu` unconditionally while `build.zig:28` disables TUI by default and `src/main.c:93-94` compiles `menu` only under `ENABLE_TUI`.
  - Uncertainty: OpenCLI may be intended to describe optional feature builds, not the default binary.
  - Evidence still needed: Decide whether OpenCLI should model default build only or include feature-gated commands with explicit metadata.

### Generated/vendor/public API concerns
- Public/template surfaces are heavily involved: `opencli.json`, `README.md`, docs/examples, `.github/template.yml`, `.template/*`, workflow files, `justfile`, and all non-static C symbols declared in headers.
- No generated/vendor/build-output files were inspected as candidates. `build.zig.zon` is package metadata and should be treated like a high-risk dependency/manifest file.
- Removing header-declared C functions (`app_output*`, `app_read_input_*`, config/log/TUI helpers) is not low-risk unless maintainers confirm the template has no downstream API compatibility expectations.

### Out-of-scope findings
- Finding: `.template/TEMPLATE_USAGE.md` / `.template/TEMPLATE_README.md` contain obvious corrupted repeated replacement text (for example grep hit `.template/TEMPLATE_USAGE.md:118-119`).
  - Why out of scope: This is primarily lane 8 AI slop/bad placeholder cleanup, not lane 3+7 unused/legacy discovery.
- Finding: Root files referenced by CI/docs but outside the requested inspection list (`.pre-commit-config.yaml`, `renovate.json`, `.markdownlint.json`, `.templatesyncignore`, etc.).
  - Why out of scope: User constrained inspection to specific paths/root files; root dotfiles other than requested paths were not reviewed for cleanup candidacy.
- Finding: Potential weak/error-hiding behavior such as `(void)app_config_load_file(...)` swallowing load errors.
  - Why out of scope: Primarily lane 6 error handling unless addressed as part of the config-feature legacy decision above.

### Recommended next checks
- Run the smallest combined static check before editing: `git grep -n -E 'test-runner.sh|app_thread_id|app_read_input_|app_output|app_config_load_args|myapp (progress|config|process|validate)|-Denable-tui' -- src test build.zig build.zig.zon justfile opencli.json .github .devcontainer .template scripts docs examples README.md test-runner.sh`
- After any cleanup edit, validate with `zig build check`; for CI/devcontainer-only edits, also review the relevant GitHub Actions/devcontainer path in a disposable environment.
