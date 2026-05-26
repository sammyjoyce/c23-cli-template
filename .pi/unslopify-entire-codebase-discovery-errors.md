### Lane
- Name: Lane 6 — error hiding, swallowed failures, unsafe fallbacks, and redundant wrappers
- Scope inspected: Entire codebase under `/home/sammy/code/c23-cli-template` excluding `.git`; focused on C error/config/io/memory/logging/TUI paths, Zig build/test fallbacks, shell scripts, workflows, and public docs/contracts referenced by those paths.

### Evidence collected
- `git status --short` — reported `?? .pi/`; no tracked source/workflow changes were present before writing this discovery report.
- `find . -path './.git' -prune -o -type f -print | wc -l` — 76 files excluding `.git`; inspected C/H files under `src/`, `build.zig`, `test/main.zig`, shell scripts, GitHub workflows, template automation, README/OpenCLI metadata, and existing `.pi` discovery context.
- `bash -n test-runner.sh scripts/create-demo.sh .template/setup.sh .template/replacer.sh .devcontainer/post-create.sh` — syntax check succeeded; shell findings below are behavioral rather than parse failures.
- `src/core/types.h:42-50` — `APP_NODISCARD` exists specifically to catch ignored error/allocation results when compiling as C23.
- `src/core/config.h:34-38` — `app_config_load_file`, `app_config_load_env`, and `app_config_load_args` are marked `APP_NODISCARD`.
- `src/main.c:51-52` — `initialize_app` explicitly casts `app_config_load_file(*config, NULL)` and `app_config_load_env(*config)` to `(void)`, bypassing the nodiscard contract.
- `src/core/config.c:140-142` — missing default config is intentionally `APP_SUCCESS`, so absence can stay non-fatal while real file/env errors are still checked.
- `src/core/config.c:146-171` — `fopen` failure logs no `strerror(errno)`; `fseek`, `ftell`, the second `fseek`, and `fclose` return values are ignored; `ftell` can return `-1` before `app_secure_malloc(size + 1)`.
- `src/core/config.c:292-321` — `strdup` results in config setters are unchecked; `app_config_add_command_arg` increments `command_arg_count` in the assignment expression even if `strdup(arg)` returns `NULL`.
- `src/cli/args.c:20`, `src/cli/args.c:66`, `src/cli/args.c:81-85` — argument parsing depends on those void setters, so allocation failure can be hidden while `app_parse_args` still returns `APP_SUCCESS` at `src/cli/args.c:89`.
- `build.zig:13-23` — git commit discovery uses `std.process.Child.run(...) catch break :blk "unknown"`, falls back to `"unknown"` on empty/oversized stdout, and does not inspect the child exit term or stderr before using stdout.
- `test/main.zig:11-15` — when `./zig-out/bin/myapp` is missing, the test builds the application and then `return`s before running any CLI assertions.
- `.github/workflows/ci.yaml:319-328` — coverage job runs only `zig build test`, then uploads `lcov.info` with `fail_ci_if_error: false`; no step in this job creates `lcov.info`.
- `.template/replacer.sh:144-154` — unknown template transforms fall through to raw output via `*) printf '%s\n' "$value" ;;`.
- `.template/README.md:25-32` and `.template/template-vars.json:7,14,21,28,41,48,54,60,67` — template docs promise `camelCase` and regex validation, while the replacement script has no `camel_case` branch and does not enforce `validation` values.
- `.template/replacer.sh:314-316` — binary-file detection suppresses grep errors with `2>/dev/null` and skips the file on any non-zero result, conflating binary files with read errors.
- `src/utils/memory.c:70-73` — `mlock` failure logs only byte count and ulimit hint, not `errno`; allocation continues unlocked.
- `src/utils/memory.c:90`, `src/utils/memory.c:107`, `src/utils/memory.c:121` — `munlock` return values are ignored in realloc/free paths.
- `src/utils/logging.c:49-63` — invalid `APP_LOG_LEVEL` silently resets to `LOG_LEVEL_ERROR` by design comment, with no user-visible warning.
- `src/utils/logging.c:125-129` — `gettimeofday`, `localtime_r`, and `strftime` return values are ignored before formatting log timestamps.
- `src/io/output.c:29-32`, `src/io/output.c:50`, `src/io/output.c:68` — `fprintf`, `vsnprintf`, and `printf` return values are ignored; public output functions return `void`.
- `src/io/input.c:123-154` — file input logs `fstat` errors, but ignores `fclose` results and reports short `fread` without distinguishing EOF, `ferror`, or `errno`.
- `src/tui/tui.c:31-40`, `src/tui/tui.c:77-86`, `src/tui/tui.c:116`, `src/tui/tui.c:179`, `src/tui/tui.c:264-272`, `src/tui/tui.c:522-527` — many ncurses calls with return/error status are unchecked.
- `test-runner.sh:1-4` — top-level test runner is a stub that always `exit 0`.
- `scripts/create-demo.sh:220`, `scripts/create-demo.sh:225`, `scripts/create-demo.sh:230` — demo commands intentionally use `|| true` inside the “Error Handling Demo” recording.
- `.github/workflows/template-cleanup.yml:41-47` installs `jq` and `sd`; `.github/workflows/template-cleanup.yml:62-65` then runs `.template/setup.sh`, while `.template/setup.sh:45-48` requires `gum` and `.template/setup.sh:102-104` recursively calls itself on cleanup confirmation.
- `README.md:209-216` and `opencli.json:222-227` document configuration precedence as CLI args > env > config file > defaults; current `src/main.c:51-55` loads default config/env before parsing CLI flags.

### High-confidence candidates
- ID suggestion: ERR-CONFIG-NODISCARD-CHECKS
- Files/symbols: `src/main.c` / `initialize_app`, `src/core/config.h` / `app_config_load_file`, `app_config_load_env`.
- Evidence: `src/core/config.h:34-38` marks config loaders `APP_NODISCARD`; `src/main.c:51-52` explicitly discards both results; `src/core/config.c:140-142` already preserves “no default config file” as `APP_SUCCESS`.
- Why behavior should be preserved: Keep absent default config non-fatal, but stop hiding actual `APP_ERROR_IO`, `APP_ERROR_MEMORY`, or invalid-arg returns. Normal runs with no config or a readable config should behave the same.
- Validation needed: `zig build test`; targeted CLI runs with no config, `APP_CONFIG_PATH` unset, and a readable config file. Add a negative test for a config path that exists at discovery time but cannot be read/opened.

- ID suggestion: ERR-CONFIG-FILE-IO-CHECKS
- Files/symbols: `src/core/config.c` / `find_config_file`, `app_config_load_file`.
- Evidence: `src/core/config.c:91-123` and `src/core/config.c:135` return/use unchecked `strdup`; `src/core/config.c:146-171` ignores `fseek`, `ftell`, second `fseek`, and `fclose`; `src/core/config.c:148` omits `strerror(errno)`.
- Why behavior should be preserved: Successful file loads and missing-default-config behavior remain unchanged; rare file/seek/close/allocation failures become explicit existing error codes instead of falling into misleading memory errors or silent cleanup.
- Validation needed: `zig build test`; focused tests for empty config, regular config, directory-as-config, seek failure if feasible, and read/close failure via a fixture or fault injection.

- ID suggestion: ERR-CONFIG-SETTER-STRDUP-CONSISTENCY
- Files/symbols: `src/core/config.c` setters, `src/cli/args.c` parser call sites.
- Evidence: `src/core/config.c:292-321` frees/replaces strings with unchecked `strdup`; `src/core/config.c:310-313` increments argument count while storing the unchecked duplicate; `src/cli/args.c:20,66,81-85` relies on those setters before returning success at `src/cli/args.c:89`.
- Why behavior should be preserved: No public signature change is required for a minimal cleanup: duplicate into a temporary, log on allocation failure, leave previous fields/count unchanged on failure, and keep successful parse behavior identical.
- Validation needed: `zig build test`; allocator/fault-injection test if available, or a small unit harness around setters to confirm count does not increase when duplication fails.

- ID suggestion: ERR-ZIG-GIT-COMMIT-FALLBACK
- Files/symbols: `build.zig` / `git_commit` block.
- Evidence: `build.zig:13-23` falls back to `"unknown"` on spawn errors, empty stdout, or oversized output without checking `child_res.term` or stderr.
- Why behavior should be preserved: Successful `git rev-parse --short HEAD` still injects the same commit string; failure cases can still produce `APP_GIT_COMMIT="unknown"` while making the fallback condition explicit and freeing child output buffers.
- Validation needed: `zig build`; a simulated no-git/non-repository build in a throwaway checkout to confirm the build still succeeds with `APP_GIT_COMMIT="unknown"`.

- ID suggestion: ERR-ZIG-TEST-BUILD-THEN-RUN
- Files/symbols: `test/main.zig` / top-level `application test suite`.
- Evidence: `test/main.zig:11-15` catches missing binary, calls `runBuild`, prints success, and returns before `testHelp`, `testVersion`, `testCommands`, or `testInvalidCommand` run at `test/main.zig:21-25`.
- Why behavior should be preserved: The official `build.zig:160-164` test step already depends on install, so `zig build test` behavior should stay the same. Standalone first-run tests stop reporting a green pass after only building.
- Validation needed: `zig build test`; in a disposable working tree, remove `zig-out` and run `zig test test/main.zig` to confirm tests continue after building.

- ID suggestion: ERR-CI-COVERAGE-UPLOAD-GUARD
- Files/symbols: `.github/workflows/ci.yaml` / `coverage` job.
- Evidence: `.github/workflows/ci.yaml:319-328` runs tests but does not generate `lcov.info`, then uploads `lcov.info` with `fail_ci_if_error: false`.
- Why behavior should be preserved: CI can remain non-blocking when coverage is intentionally absent by adding an explicit file-existence guard/skip message, or coverage can be generated before upload. Either avoids hiding a missing artifact behind Codecov’s non-failing mode.
- Validation needed: `actionlint .github/workflows/ci.yaml` if available; inspect a CI run summary to verify the coverage step is either skipped with a clear reason or uploads a real file.

- ID suggestion: ERR-TEMPLATE-TRANSFORM-VALIDATION
- Files/symbols: `.template/replacer.sh` / `apply_transform`, `.template/template-vars.json`, `.template/README.md`.
- Evidence: `.template/replacer.sh:144-154` silently returns the raw value for unknown transforms; `.template/README.md:25-32` advertises `camelCase` and regex validation; `.template/template-vars.json` carries validation regexes on multiple variables.
- Why behavior should be preserved: Existing `snake_case`, `kebab_case`, and `pascal_case` transforms keep identical output. Unknown future transforms and invalid values become explicit errors or warnings instead of silently producing unreplaced/wrong template output.
- Validation needed: `.template/replacer.sh --dry-run -v` with representative `PROJECT_NAME`, `AUTHOR_NAME`, and `GITHUB_USERNAME`; in a temporary copy, add a bogus transform and verify it fails or reports clearly.

- ID suggestion: ERR-MEM-MUNLOCK-DIAGNOSTICS
- Files/symbols: `src/utils/memory.c` / `app_secure_malloc`, `app_secure_realloc`, `app_secure_free`.
- Evidence: `src/utils/memory.c:70-73` logs `mlock` failure without `errno`; `src/utils/memory.c:90`, `src/utils/memory.c:107`, and `src/utils/memory.c:121` ignore `munlock` return values.
- Why behavior should be preserved: Allocation/free semantics can remain unchanged while adding `errno` context and best-effort `munlock` warnings. This avoids changing the current “continue if locking fails” policy.
- Validation needed: `zig build test`; manual run with `APP_LOG_LEVEL=WARNING` and a low `ulimit -l` to confirm diagnostics remain understandable and not excessively noisy.

### Risky or uncertain candidates
- Candidate: Honor `--config` loading and documented configuration precedence.
- Uncertainty: `src/main.c:51-55` loads default config/env before CLI parsing, while `src/cli/args.c:60-66` only stores `--config` after that. Fixing this likely changes real startup behavior and must be aligned with README/OpenCLI contract.
- Evidence still needed: Tests defining expected precedence for default config, `APP_CONFIG_PATH`, `--config PATH`, env vars, and missing/unreadable explicit paths.

- Candidate: Change output/logging functions to report write/format failures.
- Uncertainty: `src/io/output.h:90-103` exposes `void` output functions, and `src/utils/logging.h:48-49` exposes a `void` logger. Returning errors would be a public C API change; logging failures are also hard to report safely from the logger itself.
- Evidence still needed: Downstream API expectations and tests for broken pipe/full disk/stderr failures.

- Candidate: Warn or fail on invalid `APP_LOG_LEVEL`.
- Uncertainty: `src/utils/logging.c:58-63` documents the current silent fallback as graceful degradation; changing it may add stderr noise or fail workflows that currently tolerate misconfiguration.
- Evidence still needed: Maintainer decision on whether invalid env should be fatal, warning-only, or documented as silent.

- Candidate: Treat `mlock` failure as allocation failure for “secure” memory.
- Uncertainty: `src/utils/memory.c:70-73` continues after `mlock` failure, and these secure allocators are used for ordinary input/config/TUI buffers. Failing on lock errors could break common systems with low `ulimit -l`.
- Evidence still needed: Threat model for “secure” memory in this template and which callers truly handle secrets.

- Candidate: Check/propagate all ncurses return values in TUI drawing paths.
- Uncertainty: `src/tui/tui.c` exposes a public TUI API and many ncurses operations can fail on small/unusual terminals. Propagating every draw failure may alter interactive behavior more than a cleanup should.
- Evidence still needed: Manual TUI test matrix, expected behavior for no color, small terminal, redirected stdin/stdout, and Windows PDCurses.

- Candidate: Remove or rename fallback wrappers such as `app_read_input_from_stdin_async` and `app_config_load_args`.
- Uncertainty: `src/io/input.c:161-164` is synchronous despite async naming, and `src/core/config.c:202-208` is a no-op wrapper returning success, but both are declared in public headers.
- Evidence still needed: Search in downstream/generated projects or documented API commitments before deleting or changing signatures.

- Candidate: Replace `test-runner.sh` with a real test command.
- Uncertainty: `test-runner.sh:1-4` always exits 0 and no repo-local references were found, but as a top-level script it may be an external hook surface.
- Evidence still needed: Owner intent for this script; if kept, expected command should likely be `zig build test` or `just test`.

- Candidate: Repair template cleanup workflow/script integration.
- Uncertainty: `.github/workflows/template-cleanup.yml:62-65` invokes `.template/setup.sh`; `.template/setup.sh:45-48` requires `gum`; `.template/setup.sh:102-104` recursively calls itself on cleanup. This affects generated repositories and is broader than local error-path cleanup.
- Evidence still needed: Intended cleanup script, generated-repo dry run, and confirmation whether interactive `gum` setup should ever run in CI.

### Generated/vendor/public API concerns
- `src/**/*.h` are public C headers; changing return types for config setters, output functions, logging, TUI helpers, or async input is higher risk than adding internal checks/logging.
- `opencli.json` and README configuration/CLI sections are public contracts; config precedence or exit-code changes should update those surfaces in the same change.
- `.template/*` and `.github/workflows/template-cleanup.yml` are generator/template automation; validate changes with `--dry-run` and a generated-repository workflow scenario.
- `build.zig`/`build.zig.zon` are build/package surfaces; fallback behavior affects reproducibility and fresh-clone builds.
- `.pi/*` is an untracked local reporting/artifact area, not product source.
- No committed vendor/generated source directories were found in the current tree scan excluding `.git`; build outputs are ignored by `.gitignore` and were not inspected as source.

### Out-of-scope findings
- Finding: `.template/TEMPLATE_USAGE.md:100-175` contains visibly corrupted repeated “A modern CLI application” text.
- Why out of scope: This is Lane 8 AI-slop/docs corruption, not Lane 6 error-path cleanup.

- Finding: `README.md` quick-start text says plain `zig build -Doptimize=ReleaseSafe` builds with TUI, while `build.zig:27-29` disables TUI by default.
- Why out of scope: This is documentation/contract drift; fixing it may accompany a build-option decision but is not itself swallowed-error cleanup.

- Finding: `scripts/create-demo.sh:139`, `scripts/create-demo.sh:199-211`, and `scripts/create-demo.sh:220-225` reference demo commands (`progress`, `config`, `process`, `validate`) not implemented by `src/main.c:73-217`.
- Why out of scope: Demo freshness/feature stub cleanup is Lane 8 or feature work; the `|| true` instances at `scripts/create-demo.sh:220-230` are intentional for recording an error-handling demo.

- Finding: `src/core/config.c:173-176` reads config file contents but does not parse JSON.
- Why out of scope: This is a placeholder implementation/feature gap; implementing config parsing is broader than behavior-preserving error cleanup.

### Recommended next checks
- `rg -n --glob '!/.git/**' --glob '!/.pi/**' '(\(void\)|catch break|fail_ci_if_error|continue-on-error|\|\| true|2>/dev/null|fseek|ftell|fclose|strdup\(|munlock|APP_LOG_LEVEL|pretty)' src build.zig test .github scripts .template test-runner.sh`
- Then validate chosen low-risk changes with the smallest relevant commands: `zig build test`, `bash -n test-runner.sh scripts/create-demo.sh .template/setup.sh .template/replacer.sh .devcontainer/post-create.sh`, and `actionlint .github/workflows/ci.yaml .github/workflows/template-cleanup.yml` if available.
