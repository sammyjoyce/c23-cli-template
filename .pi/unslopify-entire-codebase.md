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

## Oracle gate attempt
- Pre-implementation oracle job `37947397-5111-48a8-a7e8-bcef0ef7300f` failed before model review.
- `oracle_read` terminal error: `Could not open model configuration UI`; response unavailable.
- Implementation remains blocked; no source edits may proceed until a pre-implementation oracle gate returns `SATISFIED` or blockers are resolved.

## Manual pre-implementation oracle satisfaction
- Tool-submitted oracle job `37947397-5111-48a8-a7e8-bcef0ef7300f` failed before review with `Could not open model configuration UI`.
- User manually uploaded the artifacts to oracle and reported the response:

```text
SATISFIED

Based on the archived code snapshot. No separate Unslopify state file was present in the uploaded archive, but the selected items are evidence-backed from the source and are safe as scoped.
```

- Parent decision: treat this user-provided oracle completion as the pre-implementation satisfaction gate for the selected ledger IDs only: `UZ-DRY-002`, `UZ-TYPE-001`, `UZ-TYPE-003`, `UZ-TYPE-005`.
- No downgraded/deferred items are approved by this manual satisfaction.


## Completed implementation
- Implemented approved IDs only: `UZ-DRY-002`, `UZ-TYPE-001`, `UZ-TYPE-003`, `UZ-TYPE-005`.
- Changed files:
  - `src/core/config.c`
  - `src/core/config.h`
  - `src/cli/args.h`
  - `src/io/output.h`
  - `src/tui/tui.h`
  - `src/tui/tui_progress.c`
- `UZ-DRY-002`: added private `app_config_set_string(char **slot, const char *value)` and routed only `app_config_set_program_name`, `app_config_set_command`, and `app_config_set_config_file` through it. `app_config_add_command_arg` unchanged. Behavior preserved, including null-value ignore and free-before-`strdup` semantics.
- `UZ-TYPE-001`: removed redundant `app_config_t` typedef declarations from `src/core/config.h`, `src/cli/args.h`, and `src/io/output.h`, all of which already include `types.h`. `src/utils/colors.h` intentionally unchanged.
- `UZ-TYPE-003`: removed duplicate `tui_progress_t` forward typedef from `src/tui/tui.h`; `tui.h` still includes `tui_progress.h`, where the canonical typedef remains.
- `UZ-TYPE-005`: changed private `tui_progress_draw` and local `window` to mutable pointers and removed `(tui_window_t *)window` casts. No public TUI signatures changed.

## Final validation results
- `git diff --check -- src/core/config.c src/core/config.h src/cli/args.h src/io/output.h src/tui/tui.h src/tui/tui_progress.c` — exit 0.
- `zig fmt --check build.zig test/main.zig` — exit 0.
- Non-TUI C compile of all base sources with `zig cc -std=c2x -Wall -Wextra -D_GNU_SOURCE ... -Isrc -c` — exit 0 for every translation unit. Existing warning: `src/io/output.c` unused `pretty` parameter; this is a known deferred public API/stub issue, not introduced by this cleanup.
- Header include smokes for `src/core/config.h`, `src/cli/args.h`, and `src/io/output.h` — exit 0.
- Static grep checks:
  - `typedef struct app_config app_config_t` remains only at canonical `src/core/types.h:18` and intentionally untouched `src/utils/colors.h:29`.
  - `typedef struct tui_progress tui_progress_t` remains only at canonical `src/tui/tui_progress.h:14`.
  - no `(tui_window_t *)window` casts remain.
- TUI header/source compile attempts failed with `fatal error: 'ncurses.h' file not found`; this is an environment dependency limitation and no TUI compile validation is claimed.
- `zig build test` still exits 2 at pre-existing `build.zig:14` (`std.process.Child.run` unavailable in local Zig 0.16.0). `UZ-BLD-001` was not approved, so this remains a known baseline blocker rather than a cleanup regression.
- Generated validation artifacts `.zig-cache/`, `zig-out/`, and `zig-pkg/` were removed after validation attempts.

## Reviewer synthesis
- Correctness reviewer: `OK`; no evidence-backed blockers or regression risks. Confirmed diff is limited to approved source/header files and behavior-preserving.
- Validation reviewer: `OK`; validation is sufficient for non-TUI touched surfaces, with accepted caveats that TUI compile requires ncurses/PDCurses and full `zig build test` is blocked by the pre-existing build-script Zig API issue.
- Simplicity/scope reviewer: `OK`; no AI slop, overengineering, noisy comments, or scope creep found. Confirmed only approved IDs were implemented.
- No fixer follow-up was needed.

## Skipped/downgraded after implementation
- No approved implementation IDs were skipped or downgraded.
- Deferred items remain deferred: `UZ-BLD-001`, `UZ-DRY-001`, `UZ-DRY-003`, `UZ-TYPE-004`, `UZ-TYPE-006`, workflow/template cleanup, OpenCLI/docs/version/TUI contract cleanup, corrupted template docs, `test-runner.sh`, public stubs, dependency metadata, generated/vendor files, and lockfiles.

## Final risks and follow-ups
- Full end-to-end `zig build test` and CLI smokes still require resolving or approving `UZ-BLD-001`, or running on a Zig version compatible with the current `build.zig`.
- TUI compile/manual validation should be run on a host with ncurses/PDCurses headers/libraries: `zig build -Denable-tui=true`, optionally `./zig-out/bin/myapp menu`.

## Final oracle question
Are there any blocking correctness, scope, or validation concerns before reporting this cleanup complete? Reply with `SATISFIED` if acceptable; otherwise list only blockers and exact fixes. Treat the pre-existing `build.zig:14` Zig API failure and missing local ncurses/PDCurses as documented validation caveats unless they make the selected cleanup unsafe.
