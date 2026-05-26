# Unslopify Curator Output

## Scope
- Task: Curate the pre-implementation Unslopify cleanup plan for the entire codebase in `/home/sammy/code/c23-cli-template`; findings written to `/home/sammy/code/c23-cli-template/.pi/unslopify-entire-codebase-curator.md` and workflow state written to `/home/sammy/code/c23-cli-template/.pi/unslopify-entire-codebase.md`.
- Non-goals: No implementation/source edits; no architecture redesign; no public CLI/OpenCLI/docs contract reshaping; no workflow/template-cleanup behavior changes; no dependency additions; no auth/crypto/security-policy changes.
- Exclusions: `.git/`, `.pi/` except the requested state/artifact files, `.zig-cache/`, `zig-out/`, `zig-pkg/`, generated/vendor dependency trees, build outputs, and public/generated surfaces unless reviewed (`opencli.json`, broad docs/examples, exported headers/API, `.template/*`, GitHub Actions external workflows).

## Repository clues
- Language/framework: C23 CLI template with Zig build orchestration, Aro dependency metadata, optional ncurses/PDCurses TUI, shell template scripts, GitHub Actions, OpenCLI JSON metadata, Markdown docs/examples.
- Package/build system: `build.zig`/`build.zig.zon`; main commands are `zig build`, `zig build test`, `zig build fmt-check`, optional `zig build -Denable-tui=true`; shell scripts under `.template/`, `scripts/`, and `.devcontainer/`.
- Likely validation commands: `zig fmt --check build.zig test/main.zig`; `zig build test` after build-script compatibility is resolved; `zig build -Doptimize=ReleaseSafe`; `zig build -Denable-tui=true` where ncurses/PDCurses is available; targeted `zig cc -std=c2x ... -c` compile smoke for impacted C files; `bash -n test-runner.sh scripts/create-demo.sh .template/replacer.sh .template/setup.sh .devcontainer/post-create.sh`; JSON parse for `opencli.json` and `.template/template-vars.json`; `actionlint` for workflows if workflow edits are approved.
- Baseline validation: Local notes report Zig 0.16.0. `zig fmt --check build.zig test/main.zig`, shell `bash -n`, and JSON parse checks pass. `actionlint` exits 1 with existing warnings/info in `.github/workflows/ci.yaml` (mostly SC2086, SC2129, SC2038). `zig build test` exits 2 before compiling project code because `build.zig:14` uses removed `std.process.Child.run`; local Zig 0.16 exposes the newer `std.process.run(gpa, io, .{ .argv = ... })` shape. `clang-format`, `cppcheck`, `sd`, and `just` are unavailable locally per notes.

## Discovery digest
- High-confidence evidence:
  - `src/core/config.c:292-323` repeats the same owned-string setter pattern in `app_config_set_program_name`, `app_config_set_command`, and `app_config_set_config_file`; a private helper can preserve current NULL-ignore and `strdup` failure behavior.
  - `src/core/types.h:18` is the canonical `app_config_t` typedef, while `src/core/config.h:15,21`, `src/cli/args.h:13,17`, and `src/io/output.h:15,19` include `types.h` and redeclare the same typedef.
  - `src/tui/tui.h:18-21` duplicates `tui_progress_t`; `src/tui/tui_progress.h:14` is the dedicated progress typedef and `src/tui/tui.h:101-102` already includes that header.
  - `src/tui/tui_progress.c:20-28` and `src/tui/tui_progress.c:64` cast away const from `window`; callers at `src/tui/tui_progress.c:100,109` pass mutable `tui_progress_t *`, so a private mutable helper is behavior-preserving.
- Risky or uncertain evidence:
  - `build.zig:14` and `test/main.zig:32,48,65,83,96,109,122,140` use `std.process.Child.run`; updating to current `std.process.run` would unblock local Zig 0.16 validation but is compatibility-risk because `.devcontainer/devcontainer.json:27-29` pins Zig 0.13.0 and `.github/workflows/ci.yaml:18-19` uses `ZIG_VERSION: master`.
  - `build.zig:70-87`, `build.zig:90-97`, and `build.zig:175-187` have clear build-script duplication, but build-script validation is currently blocked by the Zig API issue.
  - `src/core/config.h:40-46`, `src/core/config.c:219-224`, and `src/main.c:249-250` support a const-correct command-args getter, but it changes an exported header signature.
  - `src/tui/tui.h:51-52,85-87`, `src/tui/tui.c:281-333`, and `src/main.c:103-109` support read-only TUI menu items, but that is a public TUI API change.
  - `.github/workflows/ci.yaml:225,229`, `.template/replacer.sh:119-128`, `.template/setup.sh:102-104`, and `.github/workflows/template-cleanup.yml:54-65` show workflow/template cleanup candidates; they affect downstream/external automation and are downgraded.
  - `.template/TEMPLATE_README.md:3,7,12,52,54,66,72,86-93` and `.template/TEMPLATE_USAGE.md:20-28,49-63,68-75` are heavily corrupted docs, but rewriting them is broad public template documentation cleanup.
- Out-of-scope evidence:
  - Centralizing command/help/OpenCLI/README metadata crosses runtime dispatch, tests, public help text, docs, and `opencli.json` public contract.
  - Reusing `app_read_input_from_file` inside `app_config_load_file` would change error logging, size handling, return behavior, and missing-config semantics.
  - Reconciling exit-code schemas, runtime/package versions, TUI default/advertising, template variable validation/sources/required enforcement, and devcontainer/CI/README Zig-version drift requires owner policy decisions.
  - Removing public stubs or low-reference exported APIs (`app_config_load_args`, async stdin wrapper, JSON `pretty` flag, logging/error/TUI helpers) changes public/API behavior.
- Conflicts or gaps:
  - Discovery restart agents were noisy/interrupted; only the two final discovery files plus local notes are treated as authoritative. Non-final `.pi` artifacts are ignored.
  - Full `zig build test` cannot be used as a clean local gate until the Zig build-run API issue is approved/resolved or validation runs on a compatible Zig version.
  - TUI findings lack automated/interactive validation and may require ncurses/PDCurses not available in the local baseline.
  - `actionlint` has existing `ci.yaml` warnings; workflow edits need a before/after baseline rather than treating current warnings as introduced failures.

## Cleanup ledger
| ID | Lane | Candidate change | Files/symbols | Evidence | Risk | Validation | Decision |
| --- | --- | --- | --- | --- | --- | --- | --- |
| UZ-BLD-001 | Build compatibility | Replace removed `std.process.Child.run` calls with current Zig process-run API. | `build.zig` git commit probe; `test/main.zig` child process runs. | `build.zig:14`; `test/main.zig:32,48,65,83,96,109,122,140`; baseline `zig build test` exits 2 on Zig 0.16.0. | Compatibility risk: `.devcontainer/devcontainer.json:27-29` pins Zig 0.13.0, CI uses `ZIG_VERSION: master`; current API may break older supported environments. | `zig fmt --check build.zig test/main.zig`; `zig build test`; ideally matrix against intended Zig versions. | Needs human review |
| UZ-DRY-001 | DRY/build | Deduplicate build C flags and fmt path list. | `build.zig` base/TUI flags and fmt/fmt-check paths. | `build.zig:70-87` duplicates flag assembly; `build.zig:90-97` repeats it for TUI; `build.zig:175-187` repeats `.paths = &.{ "build.zig", "src", "test" }`. | Build-system blast radius; validation currently blocked by UZ-BLD-001; avoid mixing with Zig-version contract changes. | After UZ-BLD-001 decision: `zig build test`, `zig build -Doptimize=ReleaseSafe`, `zig build fmt-check`, optional `zig build -Denable-tui=true`. | Needs human review |
| UZ-DRY-002 | DRY/C internals | Add private owned-string setter helper for repeated config setters. | `src/core/config.c` / `app_config_set_program_name`, `app_config_set_command`, `app_config_set_config_file`. | `src/core/config.c:292-323` repeats `if (config && value) { free(existing); existing = strdup(value); }` shape. | Low: private implementation only; must preserve existing NULL-ignore and `strdup` failure behavior. | `zig cc -std=c2x ... -Isrc -c src/core/config.c -o /tmp/config.o`; full `zig build test` once build compatibility is resolved; CLI smoke after build. | Implement now |
| UZ-DRY-003 | DRY/tests | Add a Zig test helper for repeated CLI child-process runs. | `test/main.zig` / `runBuild`, `testHelp`, `testVersion`, `testCommands`, `testInvalidCommand`. | `test/main.zig:48-145` repeats child run, argv, stdout/stderr defers; all current calls use removed API. | Test-only, but depends on current Zig API and cannot be validated until Zig version target is clarified. | `zig fmt --check build.zig test/main.zig`; `zig build test` across intended Zig versions. | Needs human review |
| UZ-TYPE-001 | Types/contracts | Remove redundant `app_config_t` typedefs where `types.h` is already included. | `src/core/config.h`, `src/cli/args.h`, `src/io/output.h`; canonical `src/core/types.h`. | `src/core/types.h:18` canonical typedef; redundant declarations at `src/core/config.h:15,21`, `src/cli/args.h:13,17`, `src/io/output.h:15,19`. | Low: typedef remains visible through existing includes; no prototypes/layout change. | Include smoke for each header; compile impacted C; full `zig build test` when available. | Implement now |
| UZ-TYPE-002 | Types/contracts | Consider removing `app_config_t` redeclaration from colors header. | `src/utils/colors.h`. | `src/utils/colors.h:29` redeclares `app_config_t` but does not include `types.h`. | Would require adding/changing includes and dependency boundaries; less local than UZ-TYPE-001. | Not selected; revisit with include-boundary review. | No action |
| UZ-TYPE-003 | Types/contracts | Remove duplicate `tui_progress_t` typedef from `tui.h`, relying on `tui_progress.h`. | `src/tui/tui.h`, `src/tui/tui_progress.h`. | `src/tui/tui.h:18-21` duplicates `src/tui/tui_progress.h:14`; `src/tui/tui.h:101-102` includes `tui_progress.h`. | Low but TUI header surface; ensure no earlier declarations in `tui.h` require the typedef. | Header smoke with ncurses/PDCurses available; `zig build -Denable-tui=true`. | Implement now |
| UZ-TYPE-004 | Types/contracts | Make `app_config_get_command_args`/`handle_command` const-correct. | `src/core/config.h`, `src/core/config.c`, `src/main.c`. | Getter docs promise read-only at `src/core/config.h:40-46`; implementation casts away const at `src/core/config.c:219-224`; in-tree caller reads at `src/main.c:249-250`. | Public header signature change may break external code mutating through weak type. | `zig build test`; CLI smoke; external/header compatibility review. | Needs human review |
| UZ-TYPE-005 | Types/C casts | Remove private TUI progress const-cast by making draw helper mutable. | `src/tui/tui_progress.c` / `tui_progress_draw`. | `src/tui/tui_progress.c:20-28,64` casts `(tui_window_t *)window`; callers at `src/tui/tui_progress.c:100,109` pass mutable progress. | Low: private static helper; TUI compile/manual validation may not be local. | `zig build -Denable-tui=true` where ncurses/PDCurses exists; grep no `(tui_window_t *)window`; optional manual menu progress smoke. | Implement now |
| UZ-TYPE-006 | Types/public API | Make TUI menu item labels/descriptions and `tui_show_menu` items read-only. | `src/tui/tui.h`, `src/tui/tui.c`, `src/main.c`, examples. | `src/tui/tui.h:51-52,85-87`; `src/main.c:103-109` uses string literals; `src/tui/tui.c:281-333` only reads items. | Public TUI API/header change; examples/downstream callers may use mutable arrays. | TUI build/manual smoke plus API review. | Needs human review |
| UZ-DRY-004 | DRY/workflow | Capture placeholder file list once instead of repeating `find ... grep`. | `.github/workflows/ci.yaml` validate-template step. | `.github/workflows/ci.yaml:225` and `:229` repeat identical `find`/`grep -l` expression. | External CI workflow; actionlint has existing warnings; shell quoting behavior must be preserved. | `actionlint` before/after; extracted shell `bash -n`; workflow dry run if available. | Needs human review |
| UZ-DRY-005 | DRY/template | Factor shared delimiter transform helper for `to_snake`/`to_kebab`. | `.template/replacer.sh`. | `.template/replacer.sh:119-128` differs only by delimiter after identical `to_words`/empty handling. | Template replacement behavior affects generated downstream repos; dry-run comparison needed. | `.template/replacer.sh --dry-run -v` with representative env before/after. | Needs human review |
| UZ-DRY-006 | DRY/TUI | Extract centered dialog/window lifecycle helper. | `src/tui/tui.c` message/confirm/input dialogs. | `src/tui/tui.c:381-419`, `:427-475`, `:484-519` repeat sizing, create/draw/destroy/refresh. | Interactive behavior, dimensions, refresh order, and failure returns can be user-visible; no automated tests. | `zig build -Denable-tui=true`; manual TUI dialog smoke. | Needs human review |
| UZ-DRY-007 | Contracts/architecture | Centralize CLI command/option metadata across runtime/help/OpenCLI/docs. | `src/main.c`, `src/cli/help.c`, `opencli.json`, README/docs/tests. | Command names repeated at `src/main.c:73-94`, help/docs/OpenCLI surfaces; OpenCLI commands match runtime but TUI conditionality differs. | Public CLI/docs/OpenCLI contract and likely architecture/generator decision. | Owner decision and snapshot tests required. | Out of scope |
| UZ-DRY-008 | Error behavior | Reuse file-input reader in config loader. | `src/core/config.c`, `src/io/input.c`. | Both read whole files (`src/core/config.c:145-178`, `src/io/input.c:109-157`) but behavior differs. | Error/logging/size/missing-config semantics could change. | Tests for config load errors, large/unreadable files, logs. | Out of scope |
| UZ-SLOP-001 | Docs/template | Replace or repair corrupted template docs. | `.template/TEMPLATE_README.md`, `.template/TEMPLATE_USAGE.md`. | Corruption examples at `.template/TEMPLATE_README.md:3,7,12,52,54,66,72,86-93`; `.template/TEMPLATE_USAGE.md:20-28,49-63,68-75`. | Broad public template documentation/content contract; likely needs owner rewrite choices. | Markdown lint/link checks and template dry-run after owner-approved copy. | Needs human review |
| UZ-TPL-001 | Template behavior | Fix recursive cleanup prompt. | `.template/setup.sh`. | `.template/setup.sh:102-104` calls `bash ./.template/setup.sh` again during cleanup. | Template cleanup behavior affects downstream generated repos; likely bug but not safe-removal-only. | Manual/dry-run template setup in throwaway copy. | Needs human review |
| UZ-TPL-002 | Workflow/template integration | Reconcile `PROJECT_AUTHOR` vs `AUTHOR_NAME`. | `.github/workflows/template-cleanup.yml`, `.template/template-vars.json`, `.template/replacer.sh`. | Workflow passes `PROJECT_AUTHOR` at `.github/workflows/template-cleanup.yml:55`; schema/script expect `AUTHOR_NAME` at `.template/template-vars.json:37-41` and `.template/replacer.sh:213`. | External workflow/template integration behavior. | Workflow dispatch/dry-run in throwaway repo. | Needs human review |
| UZ-SCHEMA-001 | Template schema | Enforce documented `validation`, `required`, and `sources` fields. | `.template/template-vars.json`, `.template/replacer.sh`, `.template/setup.sh`, `.template/README.md`. | Schema includes fields; scripts consume fallback/transform but do not enforce validation/required/sources per discovery. | Behavior-changing; could fail workflows that currently pass. | Owner policy and dry-run matrix. | Out of scope |
| UZ-CONTRACT-001 | Public contract drift | Reconcile version metadata and TUI availability docs/OpenCLI/build default. | `build.zig`, `build.zig.zon`, `opencli.json`, README/CONTRIBUTING/docs. | Runtime/public versions differ (`1.0.0` surfaces vs `build.zig.zon:3` `0.15.0`); `build.zig:28` defaults TUI off while docs/OpenCLI advertise menu/TUI. | Public release/build/docs contract decision. | Owner policy plus docs/OpenCLI/build tests. | Out of scope |
| UZ-STUB-001 | Unused/stub | Delete or replace top-level test runner stub. | `test-runner.sh`. | `test-runner.sh:1-4` is a stub that exits 0; local grep found no references. | Top-level script may be an external hook/public surface. | Human approval; then shell syntax and CI reference search. | Needs human review |
| UZ-STUB-002 | Public API/stubs | Remove/implement public stubs/ignored API precision. | `app_config_load_args`, `app_read_input_from_stdin_async`, `app_output_json(..., bool pretty)`, low-reference exported helpers. | `src/core/config.c:202-207` no-op arg loader; `src/io/input.c:161-164` async wrapper; `src/io/output.c:56-69` ignores `pretty`; headers expose these APIs. | Public API and behavior changes; not safe-removal-only. | Owner decision and API tests. | Needs human review |

## Selected implementation plan
Only include `Implement now` items.
1. Batch: Private config setter DRY
   - Ledger IDs: `UZ-DRY-002`
   - Files: `src/core/config.c`
   - Edits: Add a `static` helper that accepts `char **slot` and `const char *value`; have `app_config_set_program_name`, `app_config_set_command`, and `app_config_set_config_file` call it. Preserve existing behavior exactly: ignore NULL `config`/value, free old value before assigning `strdup(value)`, and do not add new error handling or public API changes.
   - Done criteria: The three setters no longer duplicate the free/`strdup` body; `app_config_add_command_arg` is unchanged; no header/prototype changes.
   - Validation: `zig cc -std=c2x -D_GNU_SOURCE -DAPP_VERSION=\"1.0.0\" -DAPP_NAME=\"myapp\" -DAPP_GIT_COMMIT=\"unknown\" -DAPP_BUILD_DATE=\"reproducible\" -Isrc -c src/core/config.c -o /tmp/config.o`; full `zig build test` and CLI smokes once the known Zig build API issue is resolved or on a compatible Zig version.
   - Rollback/safety notes: Revert the helper and restore the three setter bodies; no persisted data or public contract involved.
2. Batch: Single-source forward declarations
   - Ledger IDs: `UZ-TYPE-001`, `UZ-TYPE-003`
   - Files: `src/core/config.h`, `src/cli/args.h`, `src/io/output.h`, `src/tui/tui.h`
   - Edits: Remove redundant `typedef struct app_config app_config_t;` from headers that already include `types.h`; remove `struct tui_progress; typedef struct tui_progress tui_progress_t;` from `tui.h` and rely on `tui_progress.h`. Do not touch `src/utils/colors.h` because it does not include `types.h` today.
   - Done criteria: `app_config_t` remains provided by `src/core/types.h`; `tui_progress_t` remains provided by `src/tui/tui_progress.h`; no function signatures or struct layouts change.
   - Validation: Include smoke for `config.h`, `args.h`, `output.h`; `zig cc` compile impacted C objects; for TUI, `zig build -Denable-tui=true` on a system with ncurses/PDCurses.
   - Rollback/safety notes: Restore removed typedef lines if any include-order issue appears; changes are text-only header cleanup with no runtime state.
3. Batch: Private TUI progress const-cast cleanup
   - Ledger IDs: `UZ-TYPE-005`
   - Files: `src/tui/tui_progress.c`
   - Edits: Change private `tui_progress_draw(const tui_progress_t *progress, ...)` to accept mutable `tui_progress_t *progress`; make local `window` a mutable `tui_window_t *`; call `tui_clear_window(window)` and `tui_refresh_window(window)` without casts.
   - Done criteria: No `(tui_window_t *)window` casts remain in `tui_progress.c`; public `tui_progress_*` signatures stay unchanged; create/update callers continue to call the helper with mutable pointers.
   - Validation: `zig build -Denable-tui=true` where ncurses/PDCurses exists; optional manual `./zig-out/bin/myapp menu` progress smoke. If TUI dependencies are unavailable, record that only non-TUI validation was possible.
   - Rollback/safety notes: Restore the previous const helper/casts if TUI compile or manual smoke reveals unexpected behavior; no non-TUI behavior affected because TUI sources compile only with `-Denable-tui=true`.

## Oracle packet
- Question for pi-oracle: Should implementation proceed with only `UZ-DRY-002`, `UZ-TYPE-001`, `UZ-TYPE-003`, and `UZ-TYPE-005` in the three small batches above, while explicitly deferring Zig 0.16 `std.process.run` migration/build-system cleanup, public API const/signature changes, workflow/template cleanup behavior, and broad docs/OpenCLI/contract cleanup until human approval? If any selected ID is unsafe, demote it and specify the missing validation or compatibility decision.
- Archive inputs the parent should include: The three authoritative artifacts (`.pi/unslopify-entire-codebase-discovery-duplication.md`, `.pi/unslopify-entire-codebase-discovery-types.md`, `.pi/unslopify-entire-codebase-local-notes.md`), this curator artifact, `git status --short --branch`, baseline validation output from local notes, and source excerpts for `src/core/config.c:292-323`, `src/core/types.h:18`, `src/core/config.h:15-21`, `src/cli/args.h:13-17`, `src/io/output.h:15-19`, `src/tui/tui.h:18-21,101-102`, `src/tui/tui_progress.h:14`, `src/tui/tui_progress.c:20-28,64,100,109`, `build.zig:14,70-97,175-187`, and `test/main.zig:32-145`.
- Specific blockers to ask about: Whether Zig 0.16 `std.process.run` migration is allowed despite Zig 0.13 devcontainer and CI `master` drift; whether exported/public header const-correctness changes are acceptable; whether template/workflow cleanup behavior may be edited; whether corrupted template docs should be rewritten in this pass; whether `test-runner.sh` is external/public and must be preserved; what validation is acceptable when `zig build test` is currently blocked by the known build script API failure.

## State-file body
# Unslopify state — entire codebase

phase: pre-implementation plan gate
scope: entire codebase in `/home/sammy/code/c23-cli-template`
source artifacts: `.pi/unslopify-entire-codebase-discovery-duplication.md`, `.pi/unslopify-entire-codebase-discovery-types.md`, `.pi/unslopify-entire-codebase-local-notes.md`
curator artifact: `.pi/unslopify-entire-codebase-curator.md`

## Baseline/exclusions
- Language/build: C23 CLI template built by Zig (`build.zig`, `build.zig.zon`) with optional ncurses/PDCurses TUI, shell template scripts, GitHub Actions, OpenCLI JSON, docs/examples.
- Baseline: `zig fmt --check build.zig test/main.zig`, shell `bash -n`, and JSON parse checks pass. `actionlint` exits 1 with existing `ci.yaml` shellcheck warnings. `zig build test` exits 2 on local Zig 0.16.0 because `build.zig` uses removed `std.process.Child.run` at `build.zig:14`.
- Exclude from implementation: `.git/`, `.pi/` except this state/artifact, `.zig-cache/`, `zig-out/`, `zig-pkg/`, generated/vendor outputs, external workflow/template-cleanup behavior, public contracts (`opencli.json`, docs/version/TUI availability, exported headers/API) unless explicitly approved.

## Proposed Implement now IDs
1. `UZ-DRY-002` — `src/core/config.c`: add a private helper for the repeated owned-string setter pattern in `app_config_set_program_name`, `app_config_set_command`, and `app_config_set_config_file`; preserve current NULL-ignore and `strdup` failure semantics.
2. `UZ-TYPE-001` — `src/core/config.h`, `src/cli/args.h`, `src/io/output.h`: remove redundant `app_config_t` forward typedefs where `../core/types.h`/`types.h` is already included. Do not touch `src/utils/colors.h` in this pass.
3. `UZ-TYPE-003` — `src/tui/tui.h`: remove duplicate `tui_progress_t` typedef and rely on the existing `#include "tui_progress.h"` at the bottom of `tui.h`.
4. `UZ-TYPE-005` — `src/tui/tui_progress.c`: make private `tui_progress_draw`/local `window` mutable and remove `(tui_window_t *)window` casts; callers already pass mutable `tui_progress_t *`.

## Proposed batches
- Batch A: config setter DRY (`UZ-DRY-002`).
- Batch B: single-source forward declarations (`UZ-TYPE-001`, `UZ-TYPE-003`).
- Batch C: private TUI progress const-cast cleanup (`UZ-TYPE-005`).

## Validation contract
- Always run/check: `zig fmt --check build.zig test/main.zig`; shell syntax remains unchanged.
- For C config/header changes: compile impacted C in a throwaway output, e.g. `zig cc -std=c2x -D_GNU_SOURCE -DAPP_VERSION=\"1.0.0\" -DAPP_NAME=\"myapp\" -DAPP_GIT_COMMIT=\"unknown\" -DAPP_BUILD_DATE=\"reproducible\" -Isrc -c src/core/config.c -o /tmp/config.o`, plus a small include smoke test for `config.h`, `args.h`, and `output.h`.
- Full intended validation: `zig build test` and CLI smoke tests (`--help`, `hello Alice`, `echo test message`, invalid command) after the Zig build-run API compatibility issue is approved/resolved or on a supported Zig version.
- TUI-specific validation for `UZ-TYPE-003`/`UZ-TYPE-005`: `zig build -Denable-tui=true` on a system with ncurses/PDCurses; if possible, manual `./zig-out/bin/myapp menu` smoke. If ncurses/PDCurses is unavailable, do not claim TUI validation.
- Known gate: without an approved `build.zig` Zig API compatibility fix, `zig build test` is expected to keep failing at the existing `std.process.Child.run` error; selected source cleanups must not introduce additional failures.

## Downgraded/deferred IDs
- `UZ-BLD-001` and `UZ-DRY-003`: updating `build.zig`/`test/main.zig` from `std.process.Child.run` to current `std.process.run` and deduplicating Zig test runner code depends on current Zig API. Compatibility risk is explicit because `.devcontainer/devcontainer.json` pins Zig 0.13.0 while CI uses `ZIG_VERSION: master` and local validation uses Zig 0.16.0.
- `UZ-DRY-001`: build flag/fmt-path DRY is behavior-preserving in principle but should wait until the Zig version/build-script compatibility target is clarified.
- `UZ-TYPE-004` and `UZ-TYPE-006`: const-correct public header/API improvements need human review.
- `UZ-DRY-004`, `UZ-DRY-005`, `UZ-TPL-001`, `UZ-TPL-002`, `UZ-SCHEMA-001`: external workflow/template cleanup behavior needs human review.
- `UZ-SLOP-001`, `UZ-CONTRACT-001`: broad docs/template/OpenCLI/version/TUI contract cleanup is deferred.
- `UZ-STUB-001`, `UZ-STUB-002`: top-level/public API stubs need owner decision.

## Oracle question
Should implementation proceed with only `UZ-DRY-002`, `UZ-TYPE-001`, `UZ-TYPE-003`, and `UZ-TYPE-005` in the three small batches above, while explicitly deferring Zig 0.16 `std.process.run` migration/build-system cleanup, public API const/signature changes, workflow/template cleanup behavior, and broad docs/OpenCLI/contract cleanup until human approval? If any selected ID is unsafe, demote it and specify the missing validation or compatibility decision.

