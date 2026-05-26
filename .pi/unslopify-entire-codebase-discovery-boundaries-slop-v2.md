### Lane
- Name: Lane 4+8 — circular dependencies/small boundary breaks plus AI slop, stubs, placeholder implementations, stale/noisy comments.
- Scope inspected: tracked files under `src/`, `test/`, `build.zig`, `build.zig.zon`, `justfile`, `opencli.json`, `.github/`, `.devcontainer/`, `.template/`, `scripts/`, `docs/`, `examples/`, `README.md`, `test-runner.sh`; excluded `.git`, `.pi`, build/cache/temp dirs. Read-only inspection; no validation commands that build or mutate caches were run. Baseline dirty state: `?? .pi/` only.

### Evidence collected
- `python include-graph over src` — module edges include `core -> utils` (`src/core/config.c:20-21`) and `utils -> core` (`src/utils/colors.c:11`, plus types includes), so the intended utility/core layering is cyclic at module level.
- `src/tui/tui.h:101-102` — comment says progress API is in a dedicated header “to keep tui.h lean” but `tui.h` immediately includes `tui_progress.h`; `src/tui/tui_progress.c:5,11` includes both `tui_progress.h` and umbrella `tui.h`.
- `.template/TEMPLATE_README.md:3,7-16,82-93` and `.template/TEMPLATE_USAGE.md:3,20-28,55-63,158-160` — template docs are visibly corrupted with repeated `A modern CLI application` inserted into words and code samples.
- `.template/setup.sh:45-48,75-98,100-105` — `setup.sh` is interactive, requires `gum`, runs `replacer.sh`, then on “cleanup” recursively calls `bash ./.template/setup.sh`.
- `.github/workflows/template-cleanup.yml:49-65` — workflow installs only `jq`/`sd`, passes `PROJECT_AUTHOR` but not `AUTHOR_NAME`, then calls interactive `setup.sh` as “cleanup”.
- `.template/replacer.sh:88-107` with `.template/template-vars.json:6,12,19,33,40,46,59,65` — config declares `sources` such as `repository_name`, `git_user_name`, `current_year`, but replacer only uses fallback/placeholder or existing env var; `.template/README.md:19-21,46-58` claims automatic detection/cleanup.
- `src/main.c:73-94` and `opencli.json:108-167` — implemented/public commands are `hello`, `echo`, `info`, and optional `menu`; no `progress`, `config`, `process`, or `validate` command was found.
- `docs/demos/README.md:31-32,59-69,77-81` and `scripts/create-demo.sh:139,199-211,220-225` — demos document/run unimplemented `progress`, `config`, `process`, and `validate` commands.
- `scripts/create-demo.sh:152-170` — interactive demo is explicitly simulated while docs present it as a feature demo; `src/main.c:102-108,135-141,164,172-183` advertises menu actions (`File Operations`, `Settings`, `Run Tests`) that do not implement file/settings/test behavior and display fake success.
- `src/utils/logging.c:4-7,44-61` — comments reference “Claude Code”, “hook developers”, and “hooks”, which do not match this C23 CLI template.
- `src/core/config.c:173-175,202-207`, `src/io/input.c:160-164`, `src/io/output.c:67` — explicit placeholder/stub comments: config JSON is not parsed, `app_config_load_args` is a no-op, async input just calls sync input, JSON pretty printing is not implemented.
- `README.md:70-74` vs `build.zig:26-28,89-112` — README says normal release build is “with TUI support” and `-Denable-tui=false` disables it; build default is `enable-tui ... orelse false` and TUI sources compile only when enabled.
- `docs/ARCHITECTURE.md:43-45,87-92` vs include graph — architecture diagrams omit/contradict actual edges such as `core/config.c -> utils/*` and `utils/colors.c -> core/config.h`; `docs/ARCHITECTURE.md:257-290` claims stack canaries/PIE/Fortify while `build.zig:62-67` flags only `-Wall`, `-Wextra`, `-std=c23`, `-D_GNU_SOURCE`.
- `.github/CODEOWNERS:1-2`, `.github/FUNDING.yml:3-13`, `.github/dependabot.yml:10-11,19-27,39-47,55-63`, `.github/ISSUE_TEMPLATE/config.yml:3-10` — placeholder owners/usernames and empty/default funding; dependency manifests for npm/pip/cargo were not tracked (`git ls-files | grep package/requirements/Cargo` returned no matches).
- `.devcontainer/devcontainer.json:27-28,41,81-82` with `git ls-files .devcontainer` — Zig feature pinned to `0.13.0`, VS Code C standard is `c11`, and a mount targets `.devcontainer/scripts` although tracked files are only `devcontainer.json` and `post-create.sh`.

### High-confidence candidates
For each candidate:
- ID suggestion: `template-docs-corruption`
- Files/symbols: `.template/TEMPLATE_README.md`, `.template/TEMPLATE_USAGE.md`
- Evidence: `.template/TEMPLATE_README.md:3,7-16,82-93`; `.template/TEMPLATE_USAGE.md:3,20-28,55-63,158-160` contain corrupted prose/code.
- Why behavior should be preserved: documentation/template copy restoration should not change compiled CLI behavior; preserve intended placeholder semantics for generated projects.
- Validation needed: `git diff -- .template/TEMPLATE_README.md .template/TEMPLATE_USAGE.md`; `markdownlint` if available; dry-run template replacement in a temporary copy before committing.

- ID suggestion: `template-cleanup-workflow-wiring`
- Files/symbols: `.github/workflows/template-cleanup.yml`, `.template/setup.sh`, `.template/replacer.sh`, `.template/template-vars.json`
- Evidence: workflow calls interactive `setup.sh` as cleanup (`template-cleanup.yml:62-65`); `setup.sh` requires `gum` (`setup.sh:45`) and recursively calls itself for cleanup (`setup.sh:100-105`); workflow passes `PROJECT_AUTHOR` while replacer expects `AUTHOR_NAME` (`template-cleanup.yml:54-56`, `setup.sh:83-84`).
- Why behavior should be preserved: intended behavior is variable replacement plus template-file cleanup for generated repos; app runtime is unaffected.
- Validation needed: `bash -n .template/replacer.sh .template/setup.sh`; run workflow logic against a disposable copy with env vars and verify `.template` removal/replacements.

- ID suggestion: `demo-scripts-unimplemented-commands`
- Files/symbols: `docs/demos/README.md`, `scripts/create-demo.sh`, `src/main.c`, `opencli.json`
- Evidence: docs/scripts use `progress`, `config`, `process`, `validate` (`docs/demos/README.md:31-32,59-69,77-81`; `scripts/create-demo.sh:139,199-211,220-225`); source/spec list only `hello`, `echo`, `info`, optional `menu` (`src/main.c:73-94`; `opencli.json:108-167`).
- Why behavior should be preserved: cleanups can either remove/label speculative demos or change demos to implemented commands without changing command behavior.
- Validation needed: `git grep -n -E 'myapp (progress|config|process|validate)' docs scripts examples README.md`; after edits, run `bash -n scripts/create-demo.sh` and optionally a manual demo dry run with deps installed.

- ID suggestion: `tui-menu-fake-actions`
- Files/symbols: `src/main.c` TUI `menu` switch
- Evidence: menu labels claim file/settings/tests behavior (`src/main.c:102-108`) but case 1 says “would be implemented here” (`src/main.c:135-141`), case 3 says settings saved without storage (`src/main.c:158-166`), and case 4 simulates tests then says all passed (`src/main.c:172-183`).
- Why behavior should be preserved: preserve `menu` as optional demo UI, but avoid claiming side effects/tests that do not happen.
- Validation needed: `zig build -Denable-tui=true` and manual smoke test of `zig build run -Denable-tui=true -- menu` in a terminal.

- ID suggestion: `foreign-stale-comments-logging`
- Files/symbols: `src/utils/logging.c` comments
- Evidence: references to “Claude Code”, “hook developers”, and “hooks” at `src/utils/logging.c:4-7,44-61`.
- Why behavior should be preserved: comment-only cleanup; no compiled code changes required.
- Validation needed: `git grep -n -E 'Claude|hook developers|hooks' src docs README.md`.

- ID suggestion: `github-metadata-placeholders`
- Files/symbols: `.github/CODEOWNERS`, `.github/FUNDING.yml`, `.github/dependabot.yml`, `.github/ISSUE_TEMPLATE/config.yml`
- Evidence: placeholder team/user URLs (`CODEOWNERS:1-2`, `dependabot.yml:10-11,26-27,46-47,62-63`, `ISSUE_TEMPLATE/config.yml:3-10`) and empty “Replace with...” funding keys (`FUNDING.yml:3-13`); no tracked npm/pip/cargo manifests for the configured ecosystems.
- Why behavior should be preserved: either remove inert placeholder metadata or replace with actual template-aware values; runtime behavior is unaffected.
- Validation needed: GitHub YAML parse; confirm desired owners/reviewers/funding policy with maintainer before changing public repo metadata.

- ID suggestion: `stale-docs-build-and-architecture`
- Files/symbols: `README.md`, `docs/ARCHITECTURE.md`, `build.zig`, `.devcontainer/devcontainer.json`
- Evidence: README TUI default contradicts `build.zig` (`README.md:70-74`, `build.zig:26-28,89-112`); architecture claims hardening flags absent from build flags (`docs/ARCHITECTURE.md:257-290`, `build.zig:62-67`); devcontainer says C standard `c11` while project is C23 (`devcontainer.json:41`).
- Why behavior should be preserved: documentation/config comments should describe current build behavior; any devcontainer version/standard adjustment should not change app runtime.
- Validation needed: `zig build -Doptimize=ReleaseSafe`; devcontainer JSON validation if available.

### Risky or uncertain candidates
- Candidate: break `core <-> utils` module cycle.
  - Uncertainty: `core/config.c` currently uses utils logging/memory, while `utils/colors.c` calls config getters. Breaking this may require API ownership decisions (e.g., whether color decisions accept a primitive flag or config owns color policy).
  - Evidence still needed: maintainer decision on intended layer order; compile/test both with and without TUI after any API change.

- Candidate: split TUI progress API out of umbrella `tui.h`.
  - Uncertainty: `tui.h` may intentionally be an umbrella public header despite stale “keep lean” comment; removing the include would require explicit `#include "tui/tui_progress.h"` at call sites and may break downstream users.
  - Evidence still needed: public header contract for TUI consumers; compile examples/docs snippets after change.

- Candidate: remove or implement config/async/pretty-print stubs.
  - Uncertainty: `app_config_load_file`, `app_config_load_args`, `app_read_input_from_stdin_async`, and `app_output_json(..., pretty)` are public-ish API surfaces; implementing JSON parsing/async behavior is broader than local cleanup, but leaving comments is slop.
  - Evidence still needed: desired feature set for config file parsing, async input, and JSON output; tests that define current vs desired behavior.

- Candidate: broad cleanup of generic examples using non-existent commands in `examples/advanced-usage.md`.
  - Uncertainty: examples may be intentionally aspirational integration patterns for generated apps, not commands promised by this template.
  - Evidence still needed: docs policy on whether examples must be executable against `myapp` or clearly marked conceptual.

### Generated/vendor/public API concerns
- `.template/*` and `.github/workflows/template-cleanup.yml` are template-generation public API; fixes can affect every generated repository and should be validated in a disposable clone/copy.
- `opencli.json`, `README.md`, `src/cli/help.c`, `.github/*`, and `src/tui/*.h` are public-facing contracts; avoid silent command/header/metadata removals without maintainer approval.
- No generated/vendor/build-output files were inspected beyond scoped tracked files. No lockfiles/snapshots/migrations were in the requested scope.

### Out-of-scope findings
- Finding: `app_config_load_args` appears unused except declaration/definition.
  - Why out of scope: unused-code/export removal is lane 3; it also touches public API.
- Finding: dependency cleanup for Aro/Renovate/pre-commit/Codecov references may be warranted.
  - Why out of scope: dependency/script ownership and files outside the requested scope (for example `renovate.json`, pre-commit config, security docs) were not inspected fully.
- Finding: potential runtime bugs in memory/security/error handling.
  - Why out of scope: lane requested boundaries/slop, not correctness/security audit.

### Recommended next checks
- `git grep -n -E 'A modern CLI application|Claude Code|hook developers|would be implemented|In a real implementation|yourusername|template-maintainers|progress --steps|config show|process /tmp|validate invalid' -- src docs examples scripts .github .template README.md`
- `bash -n .template/replacer.sh .template/setup.sh scripts/create-demo.sh test-runner.sh`
- In a disposable copy only: run `.template/replacer.sh --dry-run` with representative env vars, then test actual cleanup behavior.
- After any source/header change: `zig build test` and `zig build -Denable-tui=true`.
