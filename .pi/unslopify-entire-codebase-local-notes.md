# Unslopify local notes — entire codebase

## Quick scan

- Repo: `/home/sammy/code/c23-cli-template`
- Branch/status at scan start: `## main...origin/main`, clean.
- Build system/language: C23 application built with Zig (`build.zig`, `build.zig.zon`), shell template scripts, GitHub Actions, JSON/OpenCLI metadata, docs/examples.
- No repo-local `AGENTS.md` or `CLAUDE.md` found in the project root; user-level Pi instructions apply.
- Obvious generated/vendor/build outputs to exclude: `.git/`, `.pi/`, `.zig-cache/`, `zig-out/`, `zig-pkg/` (Zig dependency cache generated during attempted validation and removed afterwards).

## Baseline validation results

Commands run before editing:

```text
zig version
# 0.16.0
# exit 0

zig fmt --check build.zig test/main.zig
# exit 0

bash -n test-runner.sh scripts/create-demo.sh .template/replacer.sh .template/setup.sh .devcontainer/post-create.sh
# exit 0

python3 - <<'PY'
import json
for path in ['opencli.json', '.template/template-vars.json']:
    with open(path) as f: json.load(f)
    print(f'ok {path}')
PY
# ok opencli.json
# ok .template/template-vars.json
# exit 0

actionlint .github/workflows/ci.yaml .github/workflows/template-cleanup.yml .github/workflows/scorecard.yml
# exit 1
# Existing baseline issues are shellcheck warnings/info in .github/workflows/ci.yaml, mostly SC2086, SC2129, SC2038.

zig build test
# exit 2
# build.zig:14:44: error: root source file struct 'process.Child' has no member named 'run'
# local Zig stdlib has std.process.run(gpa, io, .{ .argv = ... }) instead.
```

Notes:
- `clang-format`, `cppcheck`, `sd`, and `just` are not installed locally.
- `clang-tidy`, `jq`, and `actionlint` are installed.
- `.devcontainer/devcontainer.json` contains comments and is JSONC, so strict JSON parsing is not an applicable validator for that file.
- Attempted `zig build` commands created `zig-pkg/` and `.zig-cache/`; I removed those generated artifacts after each run.

## Discovery-agent status

- First discovery fanout was interrupted after several agents became noisy/stale. It still produced useful final reports:
  - `.pi/unslopify-entire-codebase-discovery-duplication.md`
  - `.pi/unslopify-entire-codebase-discovery-types.md`
- A second focused discovery restart also became noisy and was interrupted/paused; no final v2 discovery report was produced before curation. Treat its partial logs as non-authoritative. Local targeted searches below cover missing Lane 3/7/8 evidence.

## Local targeted evidence for lanes not covered by final discovery outputs

### Unused/stub files and symbols

- `test-runner.sh` is a four-line stub:
  - line 3: `# Simple test runner script`
  - line 4: `exit 0`
- `git grep -n 'test-runner\.sh' -- .` found no references.
- `src/core/config.c:202-207` implements `app_config_load_args` as a no-op stub (`(void)config; (void)argc; (void)argv; return APP_SUCCESS;`) while actual parsing is in `src/cli/args.c:15-82` and called by `src/main.c:55`. This is a public header/API concern, so do not implement without review.
- Low-reference exported/public symbols found by grep, but headers make them API surfaces: `app_buffer_t`, `app_read_input_from_stdin_async`, `app_log_get_level`, `app_strerror`, `tui_beep`, `tui_flash`, `tui_is_initialized`. Downgrade/removal needs human review.
- `src/main.c` includes `io/input.h`, `io/output.h`, `utils/colors.h`, and `utils/memory.h` but does not appear to call their symbols directly in the main command path. Include cleanup may be safe but requires compilation once build validation works.

### AI slop / corrupted docs / placeholder implementations

- `.template/TEMPLATE_README.md` is heavily corrupted with repeated replacement fragments, e.g. line 3 starts `A A modern A modern CLI...`; lines 7, 12, 52, 54, 66, 72, 86-93 show similar corruption (`commA modern CLI application`, `A modern CLI application.zig`).
- `.template/TEMPLATE_USAGE.md` is heavily corrupted, e.g. line 3 says `replacement A modern CLI application works A modern CLI application how...`; many headings/variable names are corrupted (`PROJECT_NA A modern...`, `ACTIONS`, `README`, etc.).
- `.template/TEMPLATE_SUPPORT.md` is readable and referenced from README.
- `.template/README.md` documents these template docs as intended surfaces; README links `.template/TEMPLATE_USAGE.md` and `.template/TEMPLATE_SUPPORT.md`.
- `src/core/config.c:174-175`: placeholder comment says JSON parsing would happen in a real implementation but currently only logs file load. Implementing JSON parsing is out of scope; comment cleanup alone may hide a real limitation, so downgrade unless docs are made honest.
- `src/io/input.c:162-163` and `src/io/input.h:26-30`: async stdin is a C23-only placeholder wrapper around synchronous read. Public API/behavior concern; do not remove without human review.
- `src/io/output.c:67` says pretty-printing would be implemented if needed while `app_output_json(..., bool pretty)` ignores `pretty`. Public API behavior concern; do not remove flag without review.
- `src/main.c:136` TUI demo message says file operations would be implemented here; this is demo content, not a core cleanup target.

### Template cleanup/workflow leftovers

- `.template/setup.sh:102-104` recursively calls `bash ./.template/setup.sh` inside the cleanup confirmation block instead of deleting template files. This is a likely bug, but changing cleanup behavior can affect downstream generated repositories, so downgrade unless approved.
- `.github/workflows/template-cleanup.yml:54-55` passes `PROJECT_URL` and `PROJECT_AUTHOR`, but `.template/template-vars.json` / `.template/replacer.sh` expect `AUTHOR_NAME`; no `PROJECT_AUTHOR` schema variable exists. This is likely template workflow drift, but it is an external integration/workflow change and should be human/oracle-reviewed.
- `.template/template-vars.json` documents `sources`, `validation`, `required` fields, while scripts only consume fallback/transform/defaults. Enforcing them would change behavior and is out of scope for this pass.

### Contract/config drift

- `build.zig` and `test/main.zig` use `std.process.Child.run`, but local Zig 0.16.0 stdlib exposes `std.process.run(gpa, io, .{ .argv = ... })`; `zig build test` currently fails before compiling the project.
- `.devcontainer/devcontainer.json` pins Zig `0.13.0`, while CI uses `ZIG_VERSION: master` and README asks for Zig master. Updating devcontainer is environment-contract drift; likely needs human review.
- `build.zig:28` defaults TUI off, while docs/OpenCLI often advertise menu/TUI broadly. Contract decision needed; avoid changing build default or CLI docs in this pass.

## Candidate decision sketch for curator

Potential Implement now (if oracle agrees):

1. `DRY-BUILD-AND-ZIG-RUNNER`: update `build.zig` to remove duplicate C flag assembly and use current `std.process.run` API so validation can run on local Zig 0.16; optionally keep the same git-commit fallback semantics.
2. `DRY-ZIG-TEST-RUNNER`: update `test/main.zig` to use one helper for child process runs and current `std.process.run(testing.allocator, testing.io, ...)`; behavior remains test-only.
3. `TYPE-FWDDECL-SINGLE-SOURCE`: remove redundant `app_config_t`/`tui_progress_t` typedefs where included headers already provide them.
4. `TYPE-TUI-PROGRESS-DRAW-NONCONST`: make private `tui_progress_draw` use mutable parameters/locals instead of casting away const.
5. `SLOP-TEMPLATE-DOCS`: replace the corrupted `.template/TEMPLATE_README.md` and `.template/TEMPLATE_USAGE.md` with concise, accurate docs or remove them if README links are also adjusted. Docs-only, no runtime behavior.
6. `UNUSED-TEST-RUNNER-STUB`: delete unreferenced `test-runner.sh` or replace it with a real wrapper to `zig build test`. Deletion is safer as cleanup if no references exist, but root scripts are a public surface; oracle should decide.

Needs human review / downgrade:

- Remove public stubs/API (`app_config_load_args`, async input wrapper, JSON pretty flag, low-reference exported TUI/logging/error helpers).
- Change template cleanup workflow/setup behavior (`.template/setup.sh`, `.github/workflows/template-cleanup.yml`) because it touches downstream repo automation and external integration behavior.
- Reconcile OpenCLI/docs/runtime command/version/TUI availability contracts.
- Enforce template variable validation/sources/required schema.
- Broad comment stripping across headers/source.
