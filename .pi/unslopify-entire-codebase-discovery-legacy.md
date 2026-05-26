### Lane
- Name: Lane 7 deprecated, legacy, fallback, compatibility, migration leftovers
- Scope inspected: Entire tracked codebase under `/home/sammy/code/c23-cli-template`: `src/`, `build.zig`, `build.zig.zon`, tests, template cleanup files, CI/workflows, docs/examples, OpenCLI metadata, devcontainer, dependency automation, and scripts. Excluded from cleanup recommendations: `.git/`, generated/build output, ignored caches, and broad public API redesigns. Read-only inspection only; no files were modified.

### Evidence collected
- `git status --short` — showed `?? .pi/`; tracked tree has 75 files. Existing untracked `.pi/` means report/output artifacts are already outside tracked source.
- `bash -n .template/replacer.sh`, `bash -n .template/setup.sh`, `bash -n .devcontainer/post-create.sh`, `bash -n scripts/create-demo.sh` — shell syntax checks passed; functional cleanup problems below are behavioral/stale-path issues, not syntax errors.
- `build.zig:11-23`, `build.zig:75`, `build.zig:95`, and `git grep -n 'APP_GIT_COMMIT'` — build computes a git commit fallback and injects `-DAPP_GIT_COMMIT`, but the define is only referenced in `build.zig`.
- `build.zig:28` — `enable-tui` default is explicitly `false`.
- `.github/workflows/ci.yaml:78-103`, `.github/workflows/ci.yaml:106-117`, `.github/workflows/ci.yaml:158-159` — CI installs ncurses/PDCurses but runs only default `zig build install`, `zig build check`, `zig build test`, and `zig build -Doptimize=ReleaseSafe`, so TUI remains disabled.
- `README.md:70-74`, `CONTRIBUTING.md:166-170`, `docs/ZIG_PRIMER.md:123-124` — docs imply normal builds have TUI or document `enable-tui` default `true`, conflicting with `build.zig:28`.
- `.github/workflows/template-cleanup.yml:1-3`, `.github/workflows/template-cleanup.yml:6-31` — template cleanup workflow is manual `workflow_dispatch`, while `.template/README.md:44-49` says GitHub Actions automatically runs cleanup after template creation.
- `.github/workflows/template-cleanup.yml:41-47` installs `jq` and `sd`; `.template/setup.sh:45-48` requires `gum`, `sd`, `jq`, and `bash`; `.github/workflows/template-cleanup.yml:62-65` calls the interactive setup script anyway.
- `.github/workflows/template-cleanup.yml:52-58`, `.template/template-vars.json:37-42`, `.template/replacer.sh:95-99` — workflow exports `PROJECT_AUTHOR`, but the replacement schema/key consumed by `replacer.sh` is `AUTHOR_NAME`.
- `.template/setup.sh:100-104` — cleanup confirmation recursively calls `bash ./.template/setup.sh` instead of deleting template files.
- `.templatesyncignore:2` references `.github/workflows/template-sync.yml`; command check showed `.github/workflows/template-sync.yml` is missing.
- `.github/workflows/ci.yaml:241` checks for `USING_THIS_TEMPLATE.md`; command check showed `USING_THIS_TEMPLATE.md` is missing and no other tracked file references it.
- `.github/codeql/codeql-config.yml:1-24`, `README.md:9`, `README.md:32`, and `git grep -n -E 'codeql|CodeQL|github/codeql-action' -- .github README.md` — CodeQL config and README claims exist, but no CodeQL analysis workflow references the config; only Scorecard SARIF upload uses `github/codeql-action/upload-sarif`.
- `.github/workflows/ci.yaml:306-328`, `README.md:6`, and `git grep -n -E 'lcov|gcov|coverage|Codecov|codecov'` — coverage job uploads `lcov.info`, but no build/test step generates `lcov.info`.
- `.github/dependabot.yml:19-69` configures npm, pip, and cargo updates; command check showed no `package.json`, `requirements.txt`, or `Cargo.toml`.
- `.devcontainer/devcontainer.json:28`, `.devcontainer/devcontainer.json:41`, `.devcontainer/devcontainer.json:82` — devcontainer pins Zig `0.13.0`, configures VS Code C standard as `c11`, and mounts missing `.devcontainer/scripts`.
- `src/core/types.h:30-39`, `build.zig:61-66`, and `git grep` for `typeof_unqual`, `_BitInt`, and source `auto` usage — header advertises pre-C23 compatibility shims while the build always uses `-std=c23`; several compatibility macros are not used in tracked source.
- `src/io/input.h:25-31`, `src/io/input.c:160-165`, and `git grep app_read_input_from_stdin_async` — async input API is documented as C23-thread async/fallback behavior, but implementation just calls blocking stdin and has no tracked call sites.

### High-confidence candidates
- ID suggestion: `LEGACY-BUILD-UNUSED-GIT-COMMIT`
- Files/symbols: `build.zig` / `git_commit`, `APP_GIT_COMMIT` flag injection.
- Evidence: `build.zig:11-23` shells out to `git rev-parse --short HEAD` with fallback `"unknown"`; `build.zig:75` and `build.zig:95` inject `-DAPP_GIT_COMMIT`; `git grep -n 'APP_GIT_COMMIT'` only finds those two build flag lines.
- Why behavior should be preserved: No tracked C source, tests, docs, or metadata consume `APP_GIT_COMMIT`; removing the git lookup and define should not change runtime CLI output or build products except unused compiler flags and avoiding a stale fallback.
- Validation needed: `zig build test`; `zig build -Doptimize=ReleaseSafe`; smoke `./zig-out/bin/myapp --version` and `./zig-out/bin/myapp info` to confirm visible version/build output is unchanged.

- ID suggestion: `LEGACY-CI-TUI-DEPS-NOT-USED`
- Files/symbols: `.github/workflows/ci.yaml` / `test` job dependency install steps.
- Evidence: `build.zig:28` defaults `enable-tui` to `false`; `.github/workflows/ci.yaml:78-103` installs ncurses/PDCurses/vcpkg; `.github/workflows/ci.yaml:106-117` and `.github/workflows/ci.yaml:158-159` run builds/tests without `-Denable-tui=true`.
- Why behavior should be preserved: Current CI’s default build/test/release paths do not link or execute TUI code, so removing unused curses installation should preserve current non-TUI CI behavior and reduce stale platform setup. If TUI coverage is desired, add an explicit `zig build -Denable-tui=true` job instead of relying on inactive dependency install.
- Validation needed: `actionlint .github/workflows/ci.yaml` if available; then CI matrix or local equivalents `zig build`, `zig build test`, `zig build -Doptimize=ReleaseSafe`.

- ID suggestion: `LEGACY-TEMPLATE-CLEANUP-INTERACTIVE-RECURSION`
- Files/symbols: `.github/workflows/template-cleanup.yml`, `.template/setup.sh`.
- Evidence: `.github/workflows/template-cleanup.yml:41-47` installs `jq` and `sd` but not `gum`; `.template/setup.sh:45-48` requires `gum`; `.template/setup.sh:79-90` prompts interactively; `.github/workflows/template-cleanup.yml:62-65` calls that setup script in CI; `.template/setup.sh:100-104` recursively calls itself during “cleanup”.
- Why behavior should be preserved: The intended replacement behavior is already handled by `.template/replacer.sh`; the current cleanup path cannot reliably run non-interactively and does not actually remove template files. Removing or replacing the stale recursive cleanup path should preserve successful variable replacement while eliminating a broken legacy cleanup step.
- Validation needed: In a throwaway copy, run `.template/replacer.sh --dry-run -v` with representative environment values; run `bash -n` after edits; test the cleanup workflow script body in a disposable repo/worktree.

- ID suggestion: `LEGACY-TEMPLATE-AUTHOR-ENV-MISMATCH`
- Files/symbols: `.github/workflows/template-cleanup.yml` / workflow env, `.template/template-vars.json` / `AUTHOR_NAME`, `.template/replacer.sh` env lookup.
- Evidence: `.github/workflows/template-cleanup.yml:19-21` defines input `project_author`; `.github/workflows/template-cleanup.yml:52-58` exports `PROJECT_AUTHOR`; `.template/template-vars.json:37-42` defines `AUTHOR_NAME`; `.template/replacer.sh:95-99` only reads env vars matching normalized schema keys such as `AUTHOR_NAME`.
- Why behavior should be preserved: Mapping workflow input to `AUTHOR_NAME` instead of stale `PROJECT_AUTHOR` should make the documented author input take effect without changing replacement logic for other variables.
- Validation needed: `.template/replacer.sh --dry-run -v` with both `PROJECT_AUTHOR=Wrong` and `AUTHOR_NAME=Expected` to confirm current behavior; after edit, dry-run workflow env mapping and confirm `Your Name` / `Sam Joyce` replacement uses the workflow author.

- ID suggestion: `LEGACY-TEMPLATE-SYNC-ARTIFACT-PATHS`
- Files/symbols: `.templatesyncignore`, `.github/workflows/ci.yaml` / `validate-template` artifact check.
- Evidence: `.templatesyncignore:2` references `.github/workflows/template-sync.yml`, but that file is absent; `.github/workflows/ci.yaml:241` checks for `USING_THIS_TEMPLATE.md`, but that file is absent and `git grep` finds no other references.
- Why behavior should be preserved: Removing checks/ignore entries for files not present in this template should not change current build/runtime behavior. It reduces stale template-sync migration noise.
- Validation needed: `git ls-files .github/workflows/template-sync.yml USING_THIS_TEMPLATE.md`; `actionlint .github/workflows/ci.yaml` if CI check is edited.

- ID suggestion: `LEGACY-CODEQL-CONFIG-WITHOUT-WORKFLOW`
- Files/symbols: `.github/codeql/codeql-config.yml`, `README.md`.
- Evidence: `.github/codeql/codeql-config.yml:1-24` defines CodeQL config; `README.md:9` has a CodeQL badge and `README.md:32` claims CodeQL integration; `git grep` finds no CodeQL analysis workflow using `.github/codeql/codeql-config.yml`.
- Why behavior should be preserved: Removing the orphan config/badge/claim, or alternatively adding a real CodeQL workflow, should resolve stale security-scanning metadata. Removing only the unused config should not affect current CI because no workflow references it.
- Validation needed: `git grep -n -E 'codeql|CodeQL|github/codeql-action' -- .github README.md`; if removing docs/config, run markdown/pre-commit checks; if adding CodeQL, validate separately as a CI feature change.

- ID suggestion: `LEGACY-CODECOV-LCOV-NOT-GENERATED`
- Files/symbols: `.github/workflows/ci.yaml` / `coverage` job, `README.md` Codecov badge.
- Evidence: `.github/workflows/ci.yaml:319-327` runs `zig build test` and uploads `lcov.info`; repository grep found no lcov/gcov generation step; `README.md:6` advertises Codecov.
- Why behavior should be preserved: Current coverage upload is inert/stale because the configured file is not produced; removing or disabling the upload should preserve test behavior. If coverage is desired, add real coverage generation as a separate feature.
- Validation needed: `git grep -n -E 'lcov|gcov|coverage|Codecov|codecov'`; CI dry-run/actionlint; after cleanup, confirm `zig build test` remains covered by the normal `test` job.

- ID suggestion: `LEGACY-DEPENDABOT-UNUSED-ECOSYSTEMS`
- Files/symbols: `.github/dependabot.yml`.
- Evidence: `.github/dependabot.yml:19-69` configures npm, pip, and cargo ecosystems; command check found no `package.json`, `requirements.txt`, or `Cargo.toml` in the repo root.
- Why behavior should be preserved: Removing unused ecosystem blocks should preserve dependency updates for actual GitHub Actions while eliminating stale multi-language template leftovers.
- Validation needed: `git ls-files package.json package-lock.json requirements.txt pyproject.toml Cargo.toml Cargo.lock`; dependabot config validation if available.

- ID suggestion: `LEGACY-DEVCONTAINER-MISSING-SCRIPTS-MOUNT`
- Files/symbols: `.devcontainer/devcontainer.json`.
- Evidence: `.devcontainer/devcontainer.json:82` mounts `${localWorkspaceFolder}/.devcontainer/scripts`; command check showed `.devcontainer/scripts` is missing.
- Why behavior should be preserved: Removing a bind mount for a nonexistent directory should not affect project builds, and should make devcontainer setup less dependent on stale local scaffolding.
- Validation needed: Reopen/rebuild devcontainer or run a devcontainer config validation; keep `.devcontainer/post-create.sh` smoke `bash -n`.

### Risky or uncertain candidates
- Candidate: Reconcile TUI public contract and default behavior.
- Uncertainty: `build.zig:28` defaults TUI off, but `README.md:70-74`, `CONTRIBUTING.md:170`, `docs/ZIG_PRIMER.md:124`, and `opencli.json:160-170` advertise TUI/menu more broadly. Fix could be docs-only, OpenCLI contract change, or build default change.
- Evidence still needed: Maintainer decision on whether default builds should include TUI and whether `opencli.json` should describe optional/conditional commands.

- Candidate: Remove or reduce Aro dependency/compiler claims.
- Uncertainty: `build.zig:31-34` declares `aro_dep`; `build.zig:134-138` installs Aro headers; `build.zig.zon:6-8` pins Aro; docs/runtime strings claim “Aro compiler” (`README.md:20`, `README.md:247`, `src/cli/args.c:35`, `opencli.json:233`). Build script does not explicitly compile with Aro, but Zig’s own C frontend/tooling expectations may be intentional.
- Evidence still needed: Maintainer/toolchain decision on whether Aro is an intentional dependency or an old migration artifact; build/package validation without the dependency.

- Candidate: Remove pre-C23 compatibility shims from `src/core/types.h`.
- Uncertainty: `src/core/types.h:30-39` claims C11/C17 compatibility and defines fallback macros; `build.zig:61-66` compiles with `-std=c23`; tracked source does not use `typeof_unqual`, `_BitInt`, or source-level `auto`. However `src/**/*.h` are public-ish template headers, and downstream users may rely on them.
- Evidence still needed: Public API compatibility stance for template headers; compile matrix for direct `zig cc -std=c23` and any supported non-C23 compiler modes.

- Candidate: Remove unused compatibility/stub APIs `app_read_input_from_stdin_async` and `app_config_load_args`.
- Uncertainty: `src/io/input.h:25-31` promises async/fallback behavior, `src/io/input.c:160-165` just calls blocking stdin, and grep shows no call sites; `app_config_load_args` is also only declared/defined. These are public header symbols, so removing them may break downstream generated projects.
- Evidence still needed: API stability policy for starter-template headers and any examples/docs not covered by grep.

- Candidate: Enforce or remove unused template schema fields (`sources`, `validation`, `required`) and documented `camelCase`.
- Uncertainty: `.template/template-vars.json` carries schema-like metadata, and `.template/README.md:17-32` documents automatic detection and regex validation; `.template/replacer.sh:84-108` consumes env/defaults/placeholders and `.template/replacer.sh:145-153` implements transforms except `camelCase`. Enforcing validation or deleting schema fields changes template behavior/docs.
- Evidence still needed: Intended template variable system scope: simple replacement engine vs validated/detected schema.

- Candidate: Update devcontainer Zig/C standard versions.
- Uncertainty: `.devcontainer/devcontainer.json:28` pins Zig `0.13.0`, `.github/workflows/ci.yaml:19` uses Zig `master`, and `README.md:7`/`README.md:175` recommend master; `.devcontainer/devcontainer.json:41` sets IntelliSense C standard to `c11` for a C23 project. Toolchain bumps can expose unrelated build breakage.
- Evidence still needed: Supported Zig version policy and devcontainer validation on a clean container.

- Candidate: Normalize old `zig-cache` references to `.zig-cache`.
- Uncertainty: `.gitignore:3-4` keeps both `.zig-cache/` and `zig-cache/`; `build.zig:168-170` cleans `.zig-cache`; docs/config still mention `zig-cache` in places such as `CONTRIBUTING.md:194`, `docs/ZIG_PRIMER.md:182`, and `.github/codeql/codeql-config.yml:16`. Keeping both may be intentional compatibility for old Zig.
- Evidence still needed: Minimum supported Zig versions and whether old cache directory compatibility is still desired.

### Generated/vendor/public API concerns
- `.template/*`, `.templatesyncignore`, and `.github/workflows/template-cleanup.yml` are template-generation infrastructure; changes affect newly generated downstream repositories, so validate in a throwaway generated repo/worktree.
- `opencli.json` is a public CLI contract/metadata surface; removing or reclassifying `menu`, Aro compiler metadata, options, or examples should be reviewed as contract changes.
- `src/**/*.h` expose template C APIs/macros; removing compatibility shims or unused declarations may break downstream users even if tracked source has no call sites.
- `build.zig.zon` is package/build metadata; removing `aro` or changing package paths/versioning is higher risk than deleting an unused local helper.
- README badges and security/coverage claims are public project signals; cleanup is behavior-neutral for code but visible to users.
- No tracked vendor directories were found. Ignored/generated `.zig-cache` exists locally and should not be used as cleanup evidence beyond path compatibility.

### Out-of-scope findings
- Finding: `.template/TEMPLATE_README.md` and `.template/TEMPLATE_USAGE.md` contain corrupted repeated phrases such as “A modern A modern CLI...”.
- Why out of scope: This is Lane 8 AI-slop/docs corruption rather than deprecated/legacy cleanup.

- Finding: `test-runner.sh:1-4` is a stub that always exits 0 and has no `git grep` references.
- Why out of scope: This is primarily unused/stub code cleanup (Lane 3/Lane 8), not a compatibility or migration leftover unless a CI owner confirms it replaced a legacy runner.

- Finding: Configuration precedence docs do not match startup behavior around config/env/CLI parsing.
- Why out of scope: This is behavior/contract correctness and error-path work, already broader than Lane 7 stale-path cleanup.

- Finding: `.github/workflows/ci.yaml:253-259` increments `ARTIFACTS_FOUND` inside a `find ... | while read` pipeline subshell.
- Why out of scope: This is a shell correctness bug in CI validation, not specifically deprecated/legacy cleanup.

- Finding: `justfile` has placeholder `install`/`docs` recipes that only echo “not configured”.
- Why out of scope: This is noisy scaffolding/stub cleanup, closer to Lane 8.

### Recommended next checks
- Smallest static confirmation before edits: `git grep -n -E 'APP_GIT_COMMIT|template-sync.yml|USING_THIS_TEMPLATE|lcov.info|CodeQL|codeql|PROJECT_AUTHOR|AUTHOR_NAME|enable-tui|Aro|aro' -- .`
- Smallest validation for source/build cleanup: `zig build test`
- Smallest validation for workflow edits: `actionlint .github/workflows/ci.yaml .github/workflows/template-cleanup.yml`
- Smallest template validation: run `.template/replacer.sh --dry-run -v` in a throwaway copy with `PROJECT_NAME`, `AUTHOR_NAME`, `AUTHOR_EMAIL`, `GITHUB_USERNAME`, and `PROJECT_LICENSE` set.