## Implemented batches
| ID | Change | Files | Validation |
| --- | --- | --- | --- |
| `UZ-DRY-002` | Added a private `app_config_set_string` helper and used it from the three repeated owned-string setters. | `src/core/config.c` | `zig cc` compile of `src/core/config.c` exit 0; `git diff --check` exit 0. |
| `UZ-TYPE-001` | Removed redundant `app_config_t` typedef declarations only from headers that already include `types.h`. | `src/core/config.h`, `src/cli/args.h`, `src/io/output.h` | Independent header smokes for each header exit 0; `src/cli/args.c` and `src/io/output.c` compile exit 0. |
| `UZ-TYPE-003` | Removed duplicate `tui_progress_t` typedef from `tui.h`; the dedicated `tui_progress.h` include remains the source. | `src/tui/tui.h` | TUI header compile attempted, but local environment lacks `ncurses.h`; no TUI validation claimed. Static grep confirms only canonical `src/tui/tui_progress.h` typedef remains. |
| `UZ-TYPE-005` | Made private `tui_progress_draw` use mutable `tui_progress_t *` / `tui_window_t *` and removed `(tui_window_t *)window` casts. | `src/tui/tui_progress.c` | TUI source compile attempted, but local environment lacks `ncurses.h`; static grep confirms removed casts. |

## Diff summary
- File: `src/core/config.c`
  - What changed: Added `static void app_config_set_string(char **slot, const char *value)` and routed `app_config_set_program_name`, `app_config_set_command`, and `app_config_set_config_file` through it. `app_config_add_command_arg` was not changed.
  - Why behavior is preserved: The helper ignores null slot/value, frees the old value before assigning `strdup(value)`, and does not add error handling; this matches the old setter semantics, including losing the old value if `strdup` fails.
- File: `src/core/config.h`
  - What changed: Removed the redundant local `typedef struct app_config app_config_t;`; left `#include "types.h"` in place.
  - Why behavior is preserved: `app_config_t` is still declared by `src/core/types.h`, and no function signatures or struct definitions changed.
- File: `src/cli/args.h`
  - What changed: Removed the redundant local `typedef struct app_config app_config_t;`; left `#include "../core/types.h"` in place.
  - Why behavior is preserved: The same typedef remains visible through the existing include; `app_parse_args` signature is unchanged.
- File: `src/io/output.h`
  - What changed: Removed the redundant local `typedef struct app_config app_config_t;`; left `#include "../core/types.h"` in place.
  - Why behavior is preserved: The same typedef remains visible through the existing include; output function signatures are unchanged.
- File: `src/tui/tui.h`
  - What changed: Removed the duplicate forward declaration/typedef for `tui_progress_t`.
  - Why behavior is preserved: `tui.h` still includes `tui_progress.h`, where `tui_progress_t` remains declared; no public TUI signatures changed.
- File: `src/tui/tui_progress.c`
  - What changed: Changed private `tui_progress_draw` from `const tui_progress_t *` to `tui_progress_t *`, made `window` mutable, and passed it directly to `tui_clear_window`/`tui_refresh_window`.
  - Why behavior is preserved: The helper is `static`, callers already pass mutable progress objects, and the same ncurses/window functions are invoked in the same order.

## Validation
- Command/check: `zig fmt --check build.zig test/main.zig`
  - Exit code/result: 0
  - Notes: Required baseline formatting check passed.
- Command/check: `zig cc -std=c2x -D_GNU_SOURCE -DAPP_VERSION='"1.0.0"' -DAPP_NAME='"myapp"' -DAPP_GIT_COMMIT='"unknown"' -DAPP_BUILD_DATE='"reproducible"' -Isrc -c src/core/config.c -o /tmp/unslopify-config.o`
  - Exit code/result: 0
  - Notes: Focused compile for the touched non-TUI implementation passed; temp object removed.
- Command/check: independent header smokes for `src/core/config.h`, `src/cli/args.h`, and `src/io/output.h`
  - Exit code/result: 0 for all three
  - Notes: Each header was compiled independently with a tiny `app_config_t *` use; temp files removed.
- Command/check: `zig cc ... -Isrc -c src/cli/args.c -o /tmp/unslopify-args.o`
  - Exit code/result: 0
  - Notes: Confirms `args.h` remains usable from its implementation after typedef cleanup.
- Command/check: `zig cc ... -Isrc -c src/io/output.c -o /tmp/unslopify-output.o`
  - Exit code/result: 0
  - Notes: Confirms `output.h` remains usable from its implementation after typedef cleanup.
- Command/check: TUI header smoke for `#include "src/tui/tui.h"`
  - Exit code/result: 1
  - Notes: Failed because local toolchain cannot find `ncurses.h`; this is an environment/dependency limitation, not evidence of a cleanup regression. No TUI compile validation claimed.
- Command/check: `zig cc ... -Isrc -c src/tui/tui_progress.c -o /tmp/unslopify-tui-progress.o`
  - Exit code/result: 1
  - Notes: Same `ncurses.h` missing failure as TUI header smoke; temp files removed.
- Command/check: `git diff --check -- src/core/config.c src/core/config.h src/cli/args.h src/io/output.h src/tui/tui.h src/tui/tui_progress.c`
  - Exit code/result: 0
  - Notes: No whitespace errors in touched files.
- Command/check: `grep -R -n 'typedef struct app_config app_config_t\|typedef struct tui_progress tui_progress_t\|(tui_window_t \*)window' src --include='*.h' --include='*.c'`
  - Exit code/result: 0
  - Notes: Remaining matches are expected: canonical `src/core/types.h`, intentionally untouched `src/utils/colors.h`, and canonical `src/tui/tui_progress.h`; no `(tui_window_t *)window` casts remain.
- Command/check: `zig build test`
  - Exit code/result: 2
  - Notes: Known pre-existing baseline failure at `build.zig:14` because local Zig 0.16.0 has no `std.process.Child.run`; not fixed because `UZ-BLD-001` was not approved.
- Command/check: generated artifact cleanup
  - Exit code/result: complete
  - Notes: Removed `.zig-cache`, `zig-out`, and `zig-pkg` after validation attempts; final `git status --short --branch` shows only the six touched source/header files plus existing untracked `.pi/` artifacts.

## Downgraded or skipped
- Ledger ID: none of the approved implementation IDs were downgraded or skipped.
- Validation item: TUI compile/header validation could not complete because `ncurses.h` is not installed/available to `zig cc` in this environment.
- Deferred by scope: `UZ-BLD-001`, `UZ-DRY-001`, `UZ-DRY-003`, `UZ-TYPE-004`, `UZ-TYPE-006`, workflow/template cleanup, OpenCLI/docs/version/TUI contract cleanup, corrupted template docs, `test-runner.sh`, public stubs, dependency metadata, generated/vendor files, and lockfiles were not touched.

## Risks / decisions needed
- Full `zig build test` remains blocked by the pre-existing `build.zig` / Zig `std.process.Child.run` compatibility issue; resolving it requires approval for `UZ-BLD-001`.
- TUI-specific compile/manual validation still needs an environment with ncurses/PDCurses headers/libraries.
