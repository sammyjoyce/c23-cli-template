### Lane
- Name: Lane 2+5 duplicate/overlapping types/contracts/schemas and weak/evasive types/casts/fake precision
- Scope inspected: Entire repository under `/home/sammy/code/c23-cli-template`: `src/` C/C headers, Zig build/test files, OpenCLI metadata, docs/examples, GitHub workflows/config, devcontainer, pre-commit/renovate/dependabot config, template scripts under `.template/`, scripts, and root docs. I treated `.git/`, `.pi/` report artifacts, `.zig-cache/`, and `zig-pkg/` as non-cleanup/generated-or-vendor concerns rather than editable candidates.

### Evidence collected
- `git status --short --ignored` summary — existing non-source/generated state during inspection: `?? .pi/`, `?? zig-pkg/`, `!! .zig-cache/`, and ignored `zig-pkg/.../build/`. This matters because cleanup should not target generated/vendor dependency trees or report artifacts.
- `find . \( -path ./.git -o -path ./.pi -o -path ./.zig-cache -o -path ./zig-pkg \) -prune -o -type f -print | wc -l` — 75 project files inspected outside generated/report/vendor directories.
- `src/core/types.h:18` — canonical `typedef struct app_config app_config_t;` exists in the advertised core types header.
- `src/core/config.h:15` + `src/core/config.h:21`, `src/cli/args.h:13` + `src/cli/args.h:17`, `src/io/output.h:15` + `src/io/output.h:19` — these headers include `types.h` yet redeclare the same `app_config_t` typedef.
- `src/utils/colors.h:29` — also redeclares `app_config_t`, but does not include `types.h`; this is related evidence, but less local to remove without changing includes.
- `src/tui/tui.h:21` and `src/tui/tui_progress.h:14` — duplicate `typedef struct tui_progress tui_progress_t;`; `src/tui/tui.h:102` includes `tui_progress.h`, so both public headers define the same alias on the same include path.
- `src/core/config.h:40-46` — getter comments promise read-only access and returning const pointers, but `app_config_get_command_args` is declared as `char **app_config_get_command_args(const app_config_t *config, int *count);`.
- `src/core/config.c:219-224` — implementation accepts `const app_config_t *` then returns `(char **)config->command_args`, explicitly casting away constness from data owned by a const config.
- `src/main.c:249-250` — only in-tree caller stores the result in `char **cmd_argv` and passes it to `handle_command`; `src/main.c:70-94` reads command arguments only for `hello`, `echo`, and `info` dispatch.
- `src/tui/tui_progress.c:20-28` and `src/tui/tui_progress.c:64` — `tui_progress_draw(const tui_progress_t *progress, ...)` makes `const tui_window_t *window` then casts it to `(tui_window_t *)` for `tui_clear_window` and `tui_refresh_window`; the function is `static` and called only from create/update paths at `src/tui/tui_progress.c:100` and `src/tui/tui_progress.c:109` with mutable `progress`.
- `src/tui/tui.h:51-52` — `tui_menu_item_t` stores `char *label` and `char *description` even though menu definitions use string literals (`src/main.c:103-109`) and rendering only reads the fields (`src/tui/tui.c:318`, `src/tui/tui.c:322`, `src/tui/tui.c:326-327`, `src/tui/tui.c:331`).
- `src/tui/tui.h:85-87` and `src/tui/tui.c:281-283` — `tui_show_menu` takes `tui_menu_item_t *items` but inspected implementation only reads item fields and returns an item id; no writes to `items[i]` were found.
- Command-contract comparison command summary: `jq -r '.commands[].name' opencli.json` returned `hello`, `echo`, `info`, `menu`; `grep -n 'strcmp(command, ' src/main.c` found runtime dispatch for the same names at `src/main.c:73`, `src/main.c:79`, `src/main.c:86`, and `src/main.c:94`, with `menu` inside `#ifdef ENABLE_TUI`.
- Option-contract comparison command summary: `opencli.json` declares `help/-h`, `version`, `quiet/-q`, `debug/-d`, `verbose/-v`, `json`, `plain`, `no-color`, and `config/-c`; `src/cli/args.c:24`, `src/cli/args.c:32`, and `src/cli/args.c:46-61` parse those same spellings manually.
- Exit-code contract comparison command summary: `opencli.json:174-207` lists only codes `0`, `1`, `2`, `3`, `10`, `11`, and `12`, while `src/core/error.h:20-57` defines a broader `app_error` enum including codes `4-7`, `13-17`, `20-25`, and feature base `30`.
- `build.zig:7`, `opencli.json:6`, `src/core/types.h:58-59`, `README.md:121`, and `CHANGELOG.md:45` — several public/version surfaces say or inject `1.0.0`; `build.zig.zon:3` says package version `0.15.0`, so version metadata is overlapping and already drifted.
- `build.zig:28` — `enable_tui` default is explicitly false; `CONTRIBUTING.md:169-170` and `docs/ZIG_PRIMER.md:123-124` document a true default, while `README.md:70-74` implies a normal build has TUI support and `opencli.json:160-170` advertises `menu` unconditionally.
- `.template/template-vars.json:6-67` — template variable schema carries `sources`, `validation`, `required`, `fallback`, and `transform` fields.
- `.template/replacer.sh:86-167` and `.template/setup.sh:51-61` — scripts consume `transform`/`fallback` but no grep hit in scripts enforces `validation`, `required`, or `sources`; `.template/README.md:26-32` nevertheless documents automatic detection and regex validation.
- `.github/workflows/template-cleanup.yml:54-55` — workflow passes `PROJECT_URL` and `PROJECT_AUTHOR`; `.template/template-vars.json:37-41` and `.template/replacer.sh:213` expect `AUTHOR_NAME`, with no `PROJECT_AUTHOR` variable in the template schema.
- `.devcontainer/devcontainer.json:27-29` pins Zig `0.13.0`; `.github/workflows/ci.yaml:18-19` uses `ZIG_VERSION: master`, and README prerequisites also describe Zig master. This is environment-contract drift, not a local type-only cleanup.
- No build/test validation was run because `zig build`/`zig build test` would create or update build artifacts; static inspection only.

### High-confidence candidates
For each candidate:
- ID suggestion: TYPE-CONFIG-FWDDECL-SINGLE-SOURCE
- Files/symbols: `src/core/types.h` / `app_config_t`; redundant declarations in `src/core/config.h`, `src/cli/args.h`, `src/io/output.h`.
- Evidence: `src/core/types.h:18` already defines the forward typedef; `src/core/config.h:15` and `src/core/config.h:21`, `src/cli/args.h:13` and `src/cli/args.h:17`, plus `src/io/output.h:15` and `src/io/output.h:19` show headers that include `types.h` and then redeclare the same alias.
- Why behavior should be preserved: Removing only the redeclarations from headers that already include `types.h` leaves the same typedef visible to every includer through the existing include, with no struct layout, symbol, or runtime behavior change.
- Validation needed: `zig build test`; additionally compile a tiny include smoke test if desired: a C file that includes `src/core/config.h`, `src/cli/args.h`, and `src/io/output.h` independently.

- ID suggestion: TYPE-TUI-PROGRESS-FWDDECL-SINGLE-SOURCE
- Files/symbols: `src/tui/tui.h`, `src/tui/tui_progress.h` / `tui_progress_t`.
- Evidence: `src/tui/tui.h:21` and `src/tui/tui_progress.h:14` both typedef `struct tui_progress` to `tui_progress_t`; `src/tui/tui.h` already includes `tui_progress.h` at the bottom for the progress API.
- Why behavior should be preserved: Keeping the typedef in `tui_progress.h` and removing the earlier duplicate from `tui.h` should leave `tui_progress_t` available after including `tui.h`, because `tui.h` still includes the dedicated progress header. No binary behavior changes.
- Validation needed: `zig build -Denable-tui=true` on a machine with ncurses/PDCurses; header-only check that `#include "src/tui/tui.h"` can declare a `tui_progress_t *`.

- ID suggestion: TYPE-CONFIG-COMMAND-ARGS-CONST
- Files/symbols: `src/core/config.h`, `src/core/config.c`, `src/main.c` / `app_config_get_command_args`, `handle_command`.
- Evidence: `src/core/config.h:40-46` promises read-only getters but declares `char **app_config_get_command_args(const app_config_t *config, int *count)`; `src/core/config.c:224` returns `(char **)config->command_args`; `src/main.c:249-250` only passes the returned arguments into read-only command handling.
- Why behavior should be preserved: Tightening the getter and command-handler types to a const-qualified view (for example, `const char *const *` or `char *const *`, depending on chosen ownership contract) documents existing behavior and removes the cast-away-const without changing parsed values or dispatch logic. In-tree code only reads the strings.
- Validation needed: `zig build test`; smoke tests for `./zig-out/bin/myapp hello Alice`, `./zig-out/bin/myapp echo test message`, and invalid-command handling. Because this touches a public header, also check any examples/docs snippets that declare command arg arrays.

- ID suggestion: TYPE-TUI-PROGRESS-DRAW-NONCONST
- Files/symbols: `src/tui/tui_progress.c` / `tui_progress_draw`.
- Evidence: `src/tui/tui_progress.c:20-28` makes `progress` and `window` const, then casts `window` back to mutable for `tui_clear_window`; `src/tui/tui_progress.c:64` repeats the cast for `tui_refresh_window`; callers at `src/tui/tui_progress.c:100` and `src/tui/tui_progress.c:109` pass mutable progress objects.
- Why behavior should be preserved: `tui_progress_draw` is a private static helper whose purpose is to mutate/redraw the ncurses window. Changing its parameter/local variable to mutable removes the evasive casts while preserving the same drawing calls and state updates.
- Validation needed: `zig build -Denable-tui=true`; manual TUI smoke test for the `menu` command's “Run Tests” progress path if interactive validation is available.

- ID suggestion: TYPE-TUI-MENU-READONLY-ITEMS
- Files/symbols: `src/tui/tui.h`, `src/tui/tui.c`, `src/main.c`, examples using `tui_menu_item_t` / `tui_menu_item_t`, `tui_show_menu`.
- Evidence: `src/tui/tui.h:51-52` uses mutable `char *` fields for labels/descriptions; `src/main.c:103-109` initializes them with string literals; `src/tui/tui.c:318`, `src/tui/tui.c:322`, `src/tui/tui.c:326-327`, and `src/tui/tui.c:331` only read labels/descriptions; `tui_show_menu` takes `tui_menu_item_t *items` at `src/tui/tui.h:85-87` and `src/tui/tui.c:281-283` but does not write to the array.
- Why behavior should be preserved: Making menu labels/descriptions `const char *` and accepting `const tui_menu_item_t *items` aligns the API with current read-only behavior and allows string literals/const arrays without changing menu selection behavior or item ids.
- Validation needed: `zig build -Denable-tui=true`; compile examples or snippets from `examples/custom-tui.md` if examples are kept buildable; manual `myapp menu` navigation if possible.

### Risky or uncertain candidates
- Candidate: Centralize or generate the CLI command/option/OpenCLI/help/test contract.
- Uncertainty: `src/main.c`, `src/cli/args.c`, `src/cli/help.c`, `opencli.json`, README examples, and tests repeat the same command/option spellings. The duplication is real, but choosing a canonical registry or generator would cross runtime behavior, public CLI docs, and OpenCLI metadata.
- Evidence still needed: Maintainer decision on whether `opencli.json` is source of truth, generated output, or manually maintained public contract; snapshot tests for help output before any generated/help refactor.

- Candidate: Reconcile exit-code schema with `app_error` enum.
- Uncertainty: `opencli.json:174-207` intentionally may list only public/user-facing exit codes, while `src/core/error.h:20-57` includes internal/future codes. Expanding or renumbering either side can change documented API expectations.
- Evidence still needed: Public contract policy for whether all `app_error` values must appear in OpenCLI; tests or docs for exact exit codes returned by each failure path.

- Candidate: Reconcile version/app-name/build metadata (`build.zig`, `build.zig.zon`, `opencli.json`, `types.h`, README/CHANGELOG).
- Uncertainty: Runtime app version is `1.0.0`, package manifest is `0.15.0`, and docs/changelog also mention `1.0.0`. Package version and application template version might intentionally be separate.
- Evidence still needed: Release/versioning policy deciding whether `build.zig.zon` package version should drive `APP_VERSION`, OpenCLI metadata, or remain separate.

- Candidate: Reconcile TUI availability contract.
- Uncertainty: `build.zig:28` defaults TUI off, but docs/OpenCLI mention `menu` more broadly and some docs say default true. Fixing it may mean docs-only cleanup, OpenCLI conditional metadata, or build default changes; those are public behavior/contract decisions.
- Evidence still needed: Maintainer decision on desired default (`enable-tui=false` for portability vs `true` for feature completeness) and whether `menu` should be advertised when not compiled.

- Candidate: Enforce `.template/template-vars.json` validation/required/sources schema in `.template/replacer.sh` and cleanup workflow inputs.
- Uncertainty: The schema advertises validation and sources, but scripts currently use defaults/env/transforms. Adding enforcement could fail workflows that currently succeed with loose input.
- Evidence still needed: Dry-run baseline for `.template/replacer.sh --dry-run -v`, intended behavior for invalid/missing variables, and mapping decision for workflow `PROJECT_AUTHOR` vs schema `AUTHOR_NAME`.

- Candidate: Clarify or remove `app_config_load_args` overlap with `app_parse_args`.
- Uncertainty: `src/core/config.h:36-38` declares config-layer arg loading, but `src/core/config.c:202-207` ignores args and returns success while `src/cli/args.c:15-82` does real parsing. Removing or wiring it changes public API/module boundaries.
- Evidence still needed: Call-site search outside the repo/public consumers; maintainer decision whether config owns parsing or CLI owns parsing.

- Candidate: Address fake/unused precision in output API `pretty` flag.
- Uncertainty: `src/io/output.h:31-34` exposes `app_output_json(..., bool pretty)`, while `src/io/output.c:56-69` ignores `pretty`. Implementing pretty-printing or removing the flag changes behavior/API.
- Evidence still needed: Expected JSON output contract and any downstream callers that rely on the current pass-through behavior.

- Candidate: Remove or replace unused C23 compatibility macros in `src/core/types.h`.
- Uncertainty: `src/core/types.h:35-39` defines `nullptr`, `typeof_unqual`, `_BitInt`, and `auto` compatibility macros; grep found only `nullptr` used in project source. The others may be template/public conveniences, and `_BitInt`/`auto` names are language-sensitive.
- Evidence still needed: Supported compiler matrix and decision on whether `types.h` is public extension surface for downstream template users.

### Generated/vendor/public API concerns
- Public headers under `src/**/*.h` are API surfaces for template users. Even const-only signature improvements can break external code that was mutating through weak types; keep header changes small and call out in changelog if this repo treats headers as public.
- `opencli.json` is a documented CLI metadata/contract surface. Do not silently reshape command, option, version, or exit-code schemas without owner approval.
- `.template/*` and `.github/workflows/template-cleanup.yml` affect newly generated downstream repositories; validation changes can break template cleanup flows.
- `build.zig.zon` is package/dependency metadata; its `.version` may have independent semantics from runtime `APP_VERSION`.
- `README.md`, `docs/`, `examples/`, and `CHANGELOG.md` are public documentation surfaces; contract drift fixes should preserve documented promises or intentionally update them.
- `zig-pkg/` appears to contain the Aro dependency and `.zig-cache/` is generated build cache; both should be excluded from cleanup edits unless the parent explicitly scopes generated/vendor artifacts.
- `.pi/` contains discovery/report artifacts and was untracked during inspection; do not treat it as project source.

### Out-of-scope findings
- Finding: `.template/setup.sh:102-104` appears to call `bash ./.template/setup.sh` again during cleanup instead of deleting template files.
- Why out of scope: Likely a template cleanup behavior bug, not Lane 2/5 type/contract cleanup.

- Finding: `.template/TEMPLATE_README.md` and `.template/TEMPLATE_USAGE.md` contain corrupted repeated prose such as `A modern A modern CLI...`.
- Why out of scope: Lane 8 AI-slop/docs cleanup, not duplicate type/schema or weak typing cleanup.

- Finding: `src/core/config.c:174-176`, `src/io/input.c:162-163`, and `src/io/output.c:67` contain placeholder/stub comments for JSON parsing, async input, and pretty JSON.
- Why out of scope: Implementation completeness/stub cleanup belongs primarily to Lane 8 or behavior work; only the exposed `pretty` parameter was noted as a risky Lane 5 API concern.

- Finding: `test-runner.sh:1-4` is a stub that exits 0.
- Why out of scope: Unused/stub validation behavior is Lane 3/8, not this type/schema/weak-cast lane.

- Finding: `.github/dependabot.yml` includes npm, pip, and cargo ecosystems even though no package manifests for those ecosystems were found in the inspected file list.
- Why out of scope: Dependency/config pruning is Lane 3 unused config, not Lane 2/5.

### Recommended next checks
- Smallest static recheck for high-confidence type findings: `grep -R -n 'typedef struct app_config app_config_t\|typedef struct tui_progress tui_progress_t\|(char \*\*)config->command_args\|(tui_window_t \*)window\|char \*label\|char \*description' src --include='*.h' --include='*.c'`.
- After selected edits, run `zig build test` for the non-TUI config/type changes.
- If touching TUI headers or progress/menu types, run `zig build -Denable-tui=true` and, if possible, manually smoke-test `./zig-out/bin/myapp menu`.
- Before contract/schema refactors, run a baseline capture of `./zig-out/bin/myapp --help`, `./zig-out/bin/myapp --version`, and `jq -r '.commands[].name, .options[].name, .exitCodes[].code' opencli.json` so public CLI/OpenCLI drift can be reviewed deliberately.
