# TUI Menu Reference Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Rebuild `tui_show_menu` as a reference-grade ncurses menu component with separators, mnemonics, incremental search, mouse support, wide-char correct rendering, signal-wired interrupt handling, and a clean MVC split. Rename the starter shell from `tui_demo.c` to `tui_app.c` so consumers of this template inherit it as the entry point to their app.

**Architecture:** New `tui_menu` module split into three translation units that mirror the MVC seam (`tui_menu_model.c` is ncurses-free and unit-testable; `tui_menu.c` carries view + controller + orchestration; `tui_menu_internal.h` exposes model functions to unit tests). Public API is one entry point — `tui_show_menu(window /* nullable */, config)` returning `tui_menu_result_t`. The starter shell (`tui_app.c`) demonstrates the canonical resize-safe main loop.

**Tech Stack:** C23, ncurses (`ncursesw`), Zig build (`zig build test` / `zig build unit-test` / `zig build terminal-test`), TAP unit-test runner at `test/unit_runner.c`, Ghostty-VT-backed PTY scenarios at `test/terminal_vt_scenarios.c`.

**Reference spec:** `docs/superpowers/specs/2026-05-27-tui-menu-reference-design.md`

---

## File map

**Create:**
- `src/tui/tui_menu.h` — public API: types + `tui_show_menu` prototype.
- `src/tui/tui_menu_internal.h` — internal: `tui_menu_state_t` + model fn prototypes (for unit tests).
- `src/tui/tui_menu_model.c` — model: filter, navigation, mnemonic parsing. No ncurses.
- `src/tui/tui_menu.c` — view + controller + orchestrator. Includes ncurses.

**Modify:**
- `src/tui/tui.h` — re-export `tui_menu.h`; remove old `tui_show_menu` proto, `tui_menu_item_t` definition, `tui_set_background_window` / `tui_clear_background_window` from public; rename `tui_run_demo` → `tui_run_app`.
- `src/tui/tui.c` — upgrade `signal()` → `sigaction()` (clear SA_RESTART on SIGINT); delete old `tui_show_menu` body; make background-window helpers internal-only.
- `src/tui/tui_demo.c` → `src/tui/tui_app.c` (rename + restructure into three labeled regions).
- `build.zig` — update source list at line 189 (`tui_demo.c` → `tui_app.c`); add `tui_menu.c` and `tui_menu_model.c` to the binary; add `tui_menu_model.c` to the `unit-tests` exe.
- `src/cli/commands_menu.c` — update one call site (`tui_run_demo` → `tui_run_app`).
- `src/cli/commands.c` — drop "showcase" wording from the menu command summary.
- `test/unit_runner.c` — add unit tests for model layer; add `setlocale` to `main`.
- `test/terminal_vt_scenarios.c` — switch existing scenarios to mnemonic-driven selection; add five new scenarios.

---

## Coding conventions for this plan

- **C23.** Use designated initializers, `nullptr`, `(type){...}` compound literals, and `bool`/`true`/`false` from `<stdbool.h>` (in older code) or built-in (in C23 mode).
- **No `goto`** in new code unless cleaning up an existing handler's `goto cleanup` pattern.
- **Allocations**: existing code uses bare `calloc` / `free` (the previous `app_secure_*` wrappers were removed). Match that. For variable-count allocations, prefer `calloc(n, sizeof(T))` so the allocator does the overflow check.
- **Pointer style**: NULL checks at the public-API boundary; trust internal contracts.
- **Wide chars**: `setlocale(LC_ALL, "")` is already called by `tui_init`. New string helpers use `wchar_t *` + `mvwaddnwstr`. Use `wcwidth` for column-budget math.
- **Mnemonic case-fold**: use `towlower` from `<wctype.h>`.
- **Commit cadence**: one commit per task. Co-author line: `Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>`. No `--no-verify`.

---

## Task 1: Public header — types + result struct

**Files:**
- Create: `src/tui/tui_menu.h`
- Test: `test/unit_runner.c` (compile-only sanity check)

- [ ] **Step 1: Write the failing compile-only test**

Add to `test/unit_runner.c` near the top of `main`, before any other tests:

```c
#include "../src/tui/tui_menu.h"
/* ... inside main(), before unit_record calls ... */
{
  const tui_menu_item_t item = {
      .label = "&Foo",
      .description = "demo",
      .id = 42,
  };
  /* Default-init must yield enabled, NORMAL kind. */
  bool ok = !item.disabled && item.kind == TUI_MENU_ITEM_NORMAL && item.id == 42;
  unit_record(&stats, ok, "tui_menu_item_t zero-init defaults are enabled+normal");
}
{
  const tui_menu_result_t r = {.status = TUI_MENU_OK, .selected_id = 7};
  unit_record(&stats, r.status == TUI_MENU_OK && r.selected_id == 7,
              "tui_menu_result_t designated-init works");
}
```

- [ ] **Step 2: Run unit tests to confirm failure**

Run: `zig build unit-test`
Expected: build fails — `tui_menu.h` not found.

- [ ] **Step 3: Create `src/tui/tui_menu.h`**

```c
/*
 * tui_menu.h — reference-grade ncurses menu component.
 *
 * Public API for a blocking, modal menu that supports separators, mnemonics
 * (&Label syntax), incremental search, mouse, scroll indicators, a detail
 * pane for descriptions, and SIGINT-aware interrupt handling.
 *
 * The menu is "reference" rather than a framework: one entry point
 * (tui_show_menu), one config struct in, one result struct out. The
 * implementation is split into a pure model layer (tui_menu_model.c,
 * no ncurses) and a view+controller layer (tui_menu.c).
 *
 * Ownership contract: all pointers reachable from `config` (including
 * config->title, items[i].label, items[i].description) must remain valid
 * until tui_show_menu returns. The menu makes no copies of caller data.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "../core/error.h"
#include "../core/types.h"

typedef struct tui_window tui_window_t; /* defined in tui.h */

typedef enum {
  TUI_MENU_ITEM_NORMAL = 0,
  TUI_MENU_ITEM_SEPARATOR, /* visual rule; navigation skips silently */
} tui_menu_item_kind_t;

typedef struct {
  const char *label;       /* NUL-terminated UTF-8; "&x" marks mnemonic 'x'; "&&" => literal '&' */
  const char *description; /* optional UTF-8; shown in the detail pane */
  int id;                  /* returned via result.selected_id when chosen */
  bool disabled;           /* zero-init => ENABLED. Polarity is intentional. */
  tui_menu_item_kind_t kind;
} tui_menu_item_t;

typedef struct {
  const char *title;
  const tui_menu_item_t *items;
  int item_count;
  int default_index;     /* -1 picks first enabled */
  int frame_height;      /* used when window == NULL */
  int frame_width;       /* used when window == NULL */
  bool enable_search;
  bool enable_mouse;
  bool show_detail_pane;
  bool show_numeric_keys;
} tui_menu_config_t;

typedef enum {
  TUI_MENU_OK = 0,
  TUI_MENU_CANCELLED,
  TUI_MENU_INTERRUPTED,
  TUI_MENU_TOO_SMALL,
  TUI_MENU_INVALID_ARG,
} tui_menu_status_t;

typedef struct {
  tui_menu_status_t status;
  int selected_id;
  int selected_index;
} tui_menu_result_t;

/* If window == NULL, the menu owns its frame and recreates it on KEY_RESIZE.
 * If window != NULL, the menu draws into the caller's window and returns
 * TUI_MENU_TOO_SMALL on a resize the caller's window can't host. */
APP_NODISCARD tui_menu_result_t tui_show_menu(tui_window_t *window,
                                              const tui_menu_config_t *config);
```

- [ ] **Step 4: Add the new header to `build.zig`'s unit-test source list**

In `build.zig`, find the `unit_exe.root_module.addCSourceFiles` block (around line 266). The unit runner already compiles against `src/`, so no source addition is needed yet — only `tui_menu_model.c` (added in a later task). But we need to add `src/tui/tui_menu.c` as a *stub* source so the binary links once the public API gets used. Skip until Task 2 introduces it.

- [ ] **Step 5: Run unit tests to verify pass**

Run: `zig build unit-test`
Expected: PASS. Two new TAP lines appear.

- [ ] **Step 6: Commit**

```bash
git add src/tui/tui_menu.h test/unit_runner.c
git commit -m "$(cat <<'EOF'
feat(tui): introduce tui_menu.h public API surface

Adds tui_menu_item_t, tui_menu_config_t, tui_menu_result_t, and the
tui_show_menu prototype with documented ownership contract.
Polarity-flipped 'disabled' field so zero-init yields enabled items.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 2: Internal header — model state + function signatures

**Files:**
- Create: `src/tui/tui_menu_internal.h`
- Test: `test/unit_runner.c`

- [ ] **Step 1: Write the failing test**

Append to `test/unit_runner.c`:

```c
#include "../src/tui/tui_menu_internal.h"
/* ... inside main(), before printf("1..%d\n", ...) ... */
{
  /* Forward-declared opaque pointer is usable as a value type. */
  tui_menu_state_t *s = NULL;
  unit_record(&stats, s == NULL, "tui_menu_state_t is forward-declared opaque");
}
```

- [ ] **Step 2: Run to verify failure**

Run: `zig build unit-test`
Expected: build fails — `tui_menu_internal.h` not found.

- [ ] **Step 3: Create `src/tui/tui_menu_internal.h`**

```c
/*
 * tui_menu_internal.h — exposed for unit tests and for the view/controller
 * layer in tui_menu.c. Not part of the public API.
 *
 * The state struct is forward-declared opaque so the model layer
 * (tui_menu_model.c) can be tested without dragging in ncurses headers.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <wchar.h>

#include "tui_menu.h"

typedef struct tui_menu_state tui_menu_state_t;

/* Lifecycle. */
APP_NODISCARD tui_menu_status_t
tui_menu_state_create(const tui_menu_config_t *cfg, tui_menu_state_t **out);
void tui_menu_state_destroy(tui_menu_state_t *s);

/* Read-only accessors used by view + tests. */
int tui_menu_state_visible_count(const tui_menu_state_t *s);
int tui_menu_state_selected_index(const tui_menu_state_t *s); /* index into cfg->items[], or -1 */
int tui_menu_state_selected_visible(const tui_menu_state_t *s); /* index into visible_indices[] */
int tui_menu_state_top_visible(const tui_menu_state_t *s);
void tui_menu_state_set_top_visible(tui_menu_state_t *s, int top);
const wchar_t *tui_menu_state_label_wcs(const tui_menu_state_t *s, int items_index);
wchar_t tui_menu_state_mnemonic(const tui_menu_state_t *s, int items_index);
bool tui_menu_state_search_active(const tui_menu_state_t *s);
const wchar_t *tui_menu_state_search_query(const tui_menu_state_t *s);

/* Mutation — pure data ops, no ncurses. */
void tui_menu_state_step(tui_menu_state_t *s, int direction); /* +1/-1, skips separators + disabled */
void tui_menu_state_page(tui_menu_state_t *s, int direction, int page_size);
void tui_menu_state_home(tui_menu_state_t *s);
void tui_menu_state_end(tui_menu_state_t *s);
void tui_menu_state_search_open(tui_menu_state_t *s);
void tui_menu_state_search_close(tui_menu_state_t *s);
void tui_menu_state_search_append(tui_menu_state_t *s, wchar_t ch);
void tui_menu_state_search_backspace(tui_menu_state_t *s);

/* Mnemonic dispatch. Returns the items[] index that should auto-confirm
 * (>= 0), or -1 if no unique match (selection may have advanced to cycle
 * among candidates). out_beep is set to true when the caller should beep
 * (disabled match, or ambiguous match). */
int tui_menu_state_mnemonic_jump(tui_menu_state_t *s, wchar_t key, bool *out_beep);

/* Numeric accelerator: jump to visible row 0..8 (no auto-confirm). */
void tui_menu_state_numeric_jump(tui_menu_state_t *s, int visible_row);
```

- [ ] **Step 4: Run to verify pass**

Run: `zig build unit-test`
Expected: PASS. One new TAP line.

- [ ] **Step 5: Commit**

```bash
git add src/tui/tui_menu_internal.h test/unit_runner.c
git commit -m "$(cat <<'EOF'
feat(tui): add tui_menu_internal.h with model fn signatures

Forward-declares tui_menu_state_t opaque so the model layer can be
unit-tested without ncurses. Lists the pure-data operations the view
and tests will call.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 3: Model — state struct + create/destroy with overflow-checked alloc

**Files:**
- Create: `src/tui/tui_menu_model.c`
- Modify: `test/unit_runner.c`
- Modify: `build.zig` (add `tui_menu_model.c` to both binary and unit-test source lists)

- [ ] **Step 1: Add `tui_menu_model.c` to `build.zig`**

In `build.zig` around line 188 (the binary's `addCSourceFiles` block — find the existing `"src/tui/tui_demo.c",` line and add a new entry just above it):

```
"src/tui/tui_menu_model.c",
```

Also add the same line to the `unit_exe.root_module.addCSourceFiles` block around line 268, alongside `"src/utils/memory.c"`.

- [ ] **Step 2: Write the failing test**

Append to `test/unit_runner.c`:

```c
static bool test_menu_state_rejects_zero_items(void) {
  const tui_menu_config_t cfg = {.items = NULL, .item_count = 0};
  tui_menu_state_t *s = NULL;
  return tui_menu_state_create(&cfg, &s) == TUI_MENU_INVALID_ARG && s == NULL;
}

static bool test_menu_state_picks_first_enabled_when_default_negative(void) {
  const tui_menu_item_t items[] = {
      {.label = "first", .id = 1, .disabled = true},
      {.label = "second", .id = 2},
      {.label = "third", .id = 3},
  };
  const tui_menu_config_t cfg = {
      .items = items, .item_count = 3, .default_index = -1};
  tui_menu_state_t *s = NULL;
  if (tui_menu_state_create(&cfg, &s) != TUI_MENU_OK) return false;
  bool ok = tui_menu_state_selected_index(s) == 1;
  tui_menu_state_destroy(s);
  return ok;
}

static bool test_menu_state_honors_default_index_when_enabled(void) {
  const tui_menu_item_t items[] = {
      {.label = "a", .id = 1},
      {.label = "b", .id = 2},
      {.label = "c", .id = 3},
  };
  const tui_menu_config_t cfg = {
      .items = items, .item_count = 3, .default_index = 2};
  tui_menu_state_t *s = NULL;
  if (tui_menu_state_create(&cfg, &s) != TUI_MENU_OK) return false;
  bool ok = tui_menu_state_selected_index(s) == 2;
  tui_menu_state_destroy(s);
  return ok;
}
```

And inside `main()`, before the `printf("1..%d\n", ...)`:

```c
unit_record(&stats, test_menu_state_rejects_zero_items(),
            "tui_menu_state_create rejects zero items");
unit_record(&stats, test_menu_state_picks_first_enabled_when_default_negative(),
            "tui_menu_state_create skips disabled default");
unit_record(&stats, test_menu_state_honors_default_index_when_enabled(),
            "tui_menu_state_create honors enabled default_index");
```

Also at the top of `main()`, **before** any unit_record call, add:

```c
setlocale(LC_ALL, "");  /* required for wchar_t tests later */
```

And add at the top of the file with the other includes:

```c
#include <locale.h>
```

- [ ] **Step 3: Run to verify failure**

Run: `zig build unit-test`
Expected: build fails — `tui_menu_state_create` undefined.

- [ ] **Step 4: Create `src/tui/tui_menu_model.c`**

```c
/*
 * tui_menu_model.c — pure-data model for tui_menu.
 *
 * No ncurses; everything is wchar_t / int / pointer math. Unit-testable.
 */
#include "tui_menu_internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

struct tui_menu_state {
  const tui_menu_config_t *cfg;
  wchar_t **label_w;   /* size = cfg->item_count; ampersand-stripped */
  wchar_t *mnemonics;  /* size = cfg->item_count; lower-cased; 0 = none */
  int *visible;        /* size = cfg->item_count; visible items[] indices */
  int visible_count;
  int selected_visible; /* index into visible[] */
  int top_visible;
  wchar_t search_buf[64];
  size_t search_len;
  bool search_active;
};

static bool menu_item_selectable(const tui_menu_item_t *it) {
  return it && it->kind == TUI_MENU_ITEM_NORMAL && !it->disabled;
}

static int menu_first_enabled(const tui_menu_config_t *cfg) {
  for (int i = 0; i < cfg->item_count; i++) {
    if (menu_item_selectable(&cfg->items[i])) return i;
  }
  return -1;
}

/* Defined later in this task chain. Stub now. */
static void menu_state_parse_labels(struct tui_menu_state *s) {
  (void)s; /* Task 5 fills this in */
}
static void menu_state_apply_filter(struct tui_menu_state *s) {
  /* Default visible set: every item, no filter. */
  s->visible_count = 0;
  for (int i = 0; i < s->cfg->item_count; i++) {
    s->visible[s->visible_count++] = i;
  }
}

tui_menu_status_t tui_menu_state_create(const tui_menu_config_t *cfg,
                                        tui_menu_state_t **out) {
  if (!cfg || !out) return TUI_MENU_INVALID_ARG;
  *out = NULL;
  if (!cfg->items || cfg->item_count <= 0) return TUI_MENU_INVALID_ARG;
  if ((size_t)cfg->item_count > SIZE_MAX / sizeof(int)) return TUI_MENU_INVALID_ARG;

  struct tui_menu_state *s = calloc(1, sizeof(*s));
  if (!s) return TUI_MENU_INVALID_ARG;
  s->cfg = cfg;

  s->visible = calloc((size_t)cfg->item_count, sizeof(int));
  s->label_w = calloc((size_t)cfg->item_count, sizeof(wchar_t *));
  s->mnemonics = calloc((size_t)cfg->item_count, sizeof(wchar_t));
  if (!s->visible || !s->label_w || !s->mnemonics) {
    tui_menu_state_destroy(s);
    return TUI_MENU_INVALID_ARG;
  }

  menu_state_parse_labels(s);
  menu_state_apply_filter(s);

  int initial = -1;
  if (cfg->default_index >= 0 && cfg->default_index < cfg->item_count &&
      menu_item_selectable(&cfg->items[cfg->default_index])) {
    initial = cfg->default_index;
  } else {
    initial = menu_first_enabled(cfg);
  }
  if (initial < 0) {
    tui_menu_state_destroy(s);
    return TUI_MENU_INVALID_ARG;
  }
  /* Find initial in visible[]. */
  for (int v = 0; v < s->visible_count; v++) {
    if (s->visible[v] == initial) {
      s->selected_visible = v;
      break;
    }
  }
  return TUI_MENU_OK;
}

void tui_menu_state_destroy(tui_menu_state_t *s) {
  if (!s) return;
  if (s->label_w) {
    for (int i = 0; i < s->cfg->item_count; i++) free(s->label_w[i]);
    free(s->label_w);
  }
  free(s->mnemonics);
  free(s->visible);
  free(s);
}

int tui_menu_state_visible_count(const tui_menu_state_t *s) {
  return s ? s->visible_count : 0;
}

int tui_menu_state_selected_index(const tui_menu_state_t *s) {
  if (!s || s->visible_count == 0) return -1;
  return s->visible[s->selected_visible];
}

int tui_menu_state_selected_visible(const tui_menu_state_t *s) {
  return s ? s->selected_visible : -1;
}

int tui_menu_state_top_visible(const tui_menu_state_t *s) {
  return s ? s->top_visible : 0;
}

void tui_menu_state_set_top_visible(tui_menu_state_t *s, int top) {
  if (s && top >= 0) s->top_visible = top;
}

bool tui_menu_state_search_active(const tui_menu_state_t *s) {
  return s ? s->search_active : false;
}

const wchar_t *tui_menu_state_search_query(const tui_menu_state_t *s) {
  return s ? s->search_buf : L"";
}

const wchar_t *tui_menu_state_label_wcs(const tui_menu_state_t *s, int idx) {
  if (!s || idx < 0 || idx >= s->cfg->item_count) return L"";
  return s->label_w[idx] ? s->label_w[idx] : L"";
}

wchar_t tui_menu_state_mnemonic(const tui_menu_state_t *s, int idx) {
  if (!s || idx < 0 || idx >= s->cfg->item_count) return 0;
  return s->mnemonics[idx];
}

/* Step/page/home/end/numeric/mnemonic/search ops are added in subsequent
 * tasks. Stub them so the link succeeds during incremental TDD. */
void tui_menu_state_step(tui_menu_state_t *s, int direction) { (void)s; (void)direction; }
void tui_menu_state_page(tui_menu_state_t *s, int direction, int page_size) { (void)s; (void)direction; (void)page_size; }
void tui_menu_state_home(tui_menu_state_t *s) { (void)s; }
void tui_menu_state_end(tui_menu_state_t *s) { (void)s; }
void tui_menu_state_search_open(tui_menu_state_t *s) { (void)s; }
void tui_menu_state_search_close(tui_menu_state_t *s) { (void)s; }
void tui_menu_state_search_append(tui_menu_state_t *s, wchar_t ch) { (void)s; (void)ch; }
void tui_menu_state_search_backspace(tui_menu_state_t *s) { (void)s; }
int tui_menu_state_mnemonic_jump(tui_menu_state_t *s, wchar_t k, bool *b) { (void)s; (void)k; if (b) *b = false; return -1; }
void tui_menu_state_numeric_jump(tui_menu_state_t *s, int row) { (void)s; (void)row; }
```

- [ ] **Step 5: Run unit tests**

Run: `zig build unit-test`
Expected: PASS. Three new TAP lines pass.

- [ ] **Step 6: Commit**

```bash
git add src/tui/tui_menu_model.c test/unit_runner.c build.zig
git commit -m "$(cat <<'EOF'
feat(tui): add tui_menu_model.c with state create/destroy

Overflow-checked allocation for the visible-indices and label-cache
arrays. Picks first enabled item when default_index is negative or
points at a disabled item.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 4: Model — navigation step (skip separators + disabled)

**Files:**
- Modify: `src/tui/tui_menu_model.c`
- Modify: `test/unit_runner.c`

- [ ] **Step 1: Write the failing tests**

Append to `test/unit_runner.c`:

```c
static bool test_menu_step_skips_separator(void) {
  const tui_menu_item_t items[] = {
      {.label = "a", .id = 1},
      {.kind = TUI_MENU_ITEM_SEPARATOR},
      {.label = "b", .id = 2},
  };
  const tui_menu_config_t cfg = {.items = items, .item_count = 3};
  tui_menu_state_t *s = NULL;
  if (tui_menu_state_create(&cfg, &s) != TUI_MENU_OK) return false;
  if (tui_menu_state_selected_index(s) != 0) { tui_menu_state_destroy(s); return false; }
  tui_menu_state_step(s, 1);
  bool ok = tui_menu_state_selected_index(s) == 2;
  tui_menu_state_destroy(s);
  return ok;
}

static bool test_menu_step_skips_disabled(void) {
  const tui_menu_item_t items[] = {
      {.label = "a", .id = 1},
      {.label = "b", .id = 2, .disabled = true},
      {.label = "c", .id = 3},
  };
  const tui_menu_config_t cfg = {.items = items, .item_count = 3};
  tui_menu_state_t *s = NULL;
  if (tui_menu_state_create(&cfg, &s) != TUI_MENU_OK) return false;
  tui_menu_state_step(s, 1);
  bool ok = tui_menu_state_selected_index(s) == 2;
  tui_menu_state_destroy(s);
  return ok;
}

static bool test_menu_step_wraps(void) {
  const tui_menu_item_t items[] = {
      {.label = "a", .id = 1},
      {.label = "b", .id = 2},
  };
  const tui_menu_config_t cfg = {.items = items, .item_count = 2};
  tui_menu_state_t *s = NULL;
  if (tui_menu_state_create(&cfg, &s) != TUI_MENU_OK) return false;
  tui_menu_state_step(s, -1); /* wrap from 0 to 1 */
  bool ok = tui_menu_state_selected_index(s) == 1;
  tui_menu_state_destroy(s);
  return ok;
}
```

And inside `main`:

```c
unit_record(&stats, test_menu_step_skips_separator(),
            "tui_menu_state_step skips separators");
unit_record(&stats, test_menu_step_skips_disabled(),
            "tui_menu_state_step skips disabled");
unit_record(&stats, test_menu_step_wraps(),
            "tui_menu_state_step wraps at ends");
```

- [ ] **Step 2: Run to verify failure**

Run: `zig build unit-test`
Expected: three new TAP lines FAIL.

- [ ] **Step 3: Replace the `tui_menu_state_step` stub in `src/tui/tui_menu_model.c`**

```c
void tui_menu_state_step(tui_menu_state_t *s, int direction) {
  if (!s || s->visible_count == 0 || direction == 0) return;
  int next = s->selected_visible;
  for (int i = 0; i < s->visible_count; i++) {
    next += direction;
    if (next < 0) next = s->visible_count - 1;
    else if (next >= s->visible_count) next = 0;
    const int item_idx = s->visible[next];
    if (menu_item_selectable(&s->cfg->items[item_idx])) {
      s->selected_visible = next;
      return;
    }
  }
  /* All non-selectable; leave selection in place. */
}
```

- [ ] **Step 4: Run unit tests**

Run: `zig build unit-test`
Expected: PASS — three navigation tests pass.

- [ ] **Step 5: Commit**

```bash
git add src/tui/tui_menu_model.c test/unit_runner.c
git commit -m "feat(tui): step-selection skips separators and disabled items

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 5: Model — page / home / end navigation

**Files:**
- Modify: `src/tui/tui_menu_model.c`
- Modify: `test/unit_runner.c`

- [ ] **Step 1: Write the failing tests**

Append to `test/unit_runner.c`:

```c
static bool test_menu_home_end(void) {
  const tui_menu_item_t items[] = {
      {.label = "a", .id = 1}, {.label = "b", .id = 2},
      {.label = "c", .id = 3}, {.label = "d", .id = 4},
  };
  const tui_menu_config_t cfg = {.items = items, .item_count = 4};
  tui_menu_state_t *s = NULL;
  if (tui_menu_state_create(&cfg, &s) != TUI_MENU_OK) return false;
  tui_menu_state_end(s);
  if (tui_menu_state_selected_index(s) != 3) { tui_menu_state_destroy(s); return false; }
  tui_menu_state_home(s);
  bool ok = tui_menu_state_selected_index(s) == 0;
  tui_menu_state_destroy(s);
  return ok;
}

static bool test_menu_page_clamps(void) {
  const tui_menu_item_t items[] = {
      {.label = "a", .id = 1}, {.label = "b", .id = 2},
      {.label = "c", .id = 3}, {.label = "d", .id = 4},
      {.label = "e", .id = 5},
  };
  const tui_menu_config_t cfg = {.items = items, .item_count = 5};
  tui_menu_state_t *s = NULL;
  if (tui_menu_state_create(&cfg, &s) != TUI_MENU_OK) return false;
  tui_menu_state_page(s, 1, 3); /* expect selection moves to index 3 */
  if (tui_menu_state_selected_index(s) != 3) { tui_menu_state_destroy(s); return false; }
  tui_menu_state_page(s, 1, 3); /* clamps to last selectable */
  bool ok = tui_menu_state_selected_index(s) == 4;
  tui_menu_state_destroy(s);
  return ok;
}
```

And inside `main`:

```c
unit_record(&stats, test_menu_home_end(), "tui_menu home/end land on first/last");
unit_record(&stats, test_menu_page_clamps(), "tui_menu page clamps at ends");
```

- [ ] **Step 2: Run to verify failure**

Run: `zig build unit-test`
Expected: two new TAP lines FAIL.

- [ ] **Step 3: Replace stubs in `src/tui/tui_menu_model.c`**

```c
void tui_menu_state_home(tui_menu_state_t *s) {
  if (!s || s->visible_count == 0) return;
  for (int v = 0; v < s->visible_count; v++) {
    if (menu_item_selectable(&s->cfg->items[s->visible[v]])) {
      s->selected_visible = v;
      return;
    }
  }
}

void tui_menu_state_end(tui_menu_state_t *s) {
  if (!s || s->visible_count == 0) return;
  for (int v = s->visible_count - 1; v >= 0; v--) {
    if (menu_item_selectable(&s->cfg->items[s->visible[v]])) {
      s->selected_visible = v;
      return;
    }
  }
}

void tui_menu_state_page(tui_menu_state_t *s, int direction, int page_size) {
  if (!s || page_size <= 0 || direction == 0) return;
  for (int i = 0; i < page_size; i++) {
    int prev = s->selected_visible;
    tui_menu_state_step(s, direction);
    if (s->selected_visible == prev) break; /* couldn't move further */
  }
}
```

- [ ] **Step 4: Run unit tests**

Run: `zig build unit-test`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add src/tui/tui_menu_model.c test/unit_runner.c
git commit -m "feat(tui): home/end/page-step navigation in menu model

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 6: Model — UTF-8 → wchar label parsing with ampersand stripping

**Files:**
- Modify: `src/tui/tui_menu_model.c`
- Modify: `test/unit_runner.c`

- [ ] **Step 1: Write the failing tests**

Append to `test/unit_runner.c`:

```c
static bool test_menu_label_strips_ampersand(void) {
  const tui_menu_item_t items[] = {{.label = "&Overview", .id = 1}};
  const tui_menu_config_t cfg = {.items = items, .item_count = 1};
  tui_menu_state_t *s = NULL;
  if (tui_menu_state_create(&cfg, &s) != TUI_MENU_OK) return false;
  const wchar_t *w = tui_menu_state_label_wcs(s, 0);
  bool ok = wcscmp(w, L"Overview") == 0 &&
            tui_menu_state_mnemonic(s, 0) == L'o';
  tui_menu_state_destroy(s);
  return ok;
}

static bool test_menu_label_literal_ampersand(void) {
  const tui_menu_item_t items[] = {{.label = "AT&&T", .id = 1}};
  const tui_menu_config_t cfg = {.items = items, .item_count = 1};
  tui_menu_state_t *s = NULL;
  if (tui_menu_state_create(&cfg, &s) != TUI_MENU_OK) return false;
  const wchar_t *w = tui_menu_state_label_wcs(s, 0);
  bool ok = wcscmp(w, L"AT&T") == 0 && tui_menu_state_mnemonic(s, 0) == 0;
  tui_menu_state_destroy(s);
  return ok;
}

static bool test_menu_label_mnemonic_mid_word(void) {
  const tui_menu_item_t items[] = {{.label = "E&xit", .id = 0}};
  const tui_menu_config_t cfg = {.items = items, .item_count = 1};
  tui_menu_state_t *s = NULL;
  if (tui_menu_state_create(&cfg, &s) != TUI_MENU_OK) return false;
  const wchar_t *w = tui_menu_state_label_wcs(s, 0);
  bool ok = wcscmp(w, L"Exit") == 0 && tui_menu_state_mnemonic(s, 0) == L'x';
  tui_menu_state_destroy(s);
  return ok;
}
```

And inside `main`:

```c
unit_record(&stats, test_menu_label_strips_ampersand(),
            "tui_menu label strips '&' and records mnemonic");
unit_record(&stats, test_menu_label_literal_ampersand(),
            "tui_menu label '&&' renders as literal '&'");
unit_record(&stats, test_menu_label_mnemonic_mid_word(),
            "tui_menu mnemonic can be mid-word");
```

- [ ] **Step 2: Run to verify failure**

Run: `zig build unit-test`
Expected: three new tests FAIL.

- [ ] **Step 3: Replace the `menu_state_parse_labels` stub in `src/tui/tui_menu_model.c`**

```c
static void menu_state_parse_labels(struct tui_menu_state *s) {
  for (int i = 0; i < s->cfg->item_count; i++) {
    const char *src = s->cfg->items[i].label;
    if (!src) {
      s->label_w[i] = wcsdup(L"");
      s->mnemonics[i] = 0;
      continue;
    }
    const size_t src_bytes = strlen(src);
    wchar_t *dst = calloc(src_bytes + 1, sizeof(wchar_t));
    if (!dst) {
      s->label_w[i] = NULL;
      s->mnemonics[i] = 0;
      continue;
    }

    size_t out = 0;
    wchar_t mnemonic = 0;
    const char *p = src;
    while (*p) {
      if (p[0] == '&' && p[1] == '&') {
        dst[out++] = L'&';
        p += 2;
        continue;
      }
      if (p[0] == '&' && p[1] != '\0' && mnemonic == 0) {
        wchar_t wc = 0;
        const int n = mbtowc(&wc, p + 1, MB_CUR_MAX);
        if (n > 0 && iswalnum(wc)) {
          mnemonic = (wchar_t)towlower(wc);
          dst[out++] = wc;
          p += 1 + (size_t)n;
          continue;
        }
      }
      wchar_t wc = 0;
      const int n = mbtowc(&wc, p, MB_CUR_MAX);
      if (n <= 0) { p++; continue; }
      dst[out++] = wc;
      p += n;
    }
    dst[out] = 0;
    s->label_w[i] = dst;
    s->mnemonics[i] = mnemonic;
  }
}
```

Also at the top of `src/tui/tui_menu_model.c` add `#include <stdlib.h>` for `mbtowc` (already there) and add the helper for `wcsdup` if not present — many libc implementations provide it; for portability also add this fallback at the top of the file (after includes):

```c
#ifndef HAVE_WCSDUP
static wchar_t *wcsdup_fallback(const wchar_t *s) {
  const size_t n = wcslen(s) + 1;
  wchar_t *r = calloc(n, sizeof(wchar_t));
  if (r) memcpy(r, s, n * sizeof(wchar_t));
  return r;
}
#define wcsdup wcsdup_fallback
#endif
```

- [ ] **Step 4: Run unit tests**

Run: `zig build unit-test`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add src/tui/tui_menu_model.c test/unit_runner.c
git commit -m "feat(tui): parse '&Label' mnemonic syntax with utf-8 -> wchar

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 7: Model — mnemonic jump (case-insensitive, cycle on duplicate)

**Files:**
- Modify: `src/tui/tui_menu_model.c`
- Modify: `test/unit_runner.c`

- [ ] **Step 1: Write the failing tests**

Append to `test/unit_runner.c`:

```c
static bool test_mnemonic_unique_returns_index(void) {
  const tui_menu_item_t items[] = {
      {.label = "&Foo", .id = 1},
      {.label = "&Bar", .id = 2},
  };
  const tui_menu_config_t cfg = {.items = items, .item_count = 2};
  tui_menu_state_t *s = NULL;
  if (tui_menu_state_create(&cfg, &s) != TUI_MENU_OK) return false;
  bool beep = false;
  int r = tui_menu_state_mnemonic_jump(s, L'b', &beep);
  bool ok = (r == 1) && !beep;
  tui_menu_state_destroy(s);
  return ok;
}

static bool test_mnemonic_duplicate_cycles_no_confirm(void) {
  const tui_menu_item_t items[] = {
      {.label = "F&oo", .id = 1},
      {.label = "B&ar", .id = 2},
      {.label = "B&az", .id = 3},
  };
  const tui_menu_config_t cfg = {.items = items, .item_count = 3};
  tui_menu_state_t *s = NULL;
  if (tui_menu_state_create(&cfg, &s) != TUI_MENU_OK) return false;
  bool beep = false;
  /* Two items share mnemonic 'a' — calling it twice should cycle. */
  int r1 = tui_menu_state_mnemonic_jump(s, L'a', &beep);
  int idx1 = tui_menu_state_selected_index(s);
  int r2 = tui_menu_state_mnemonic_jump(s, L'a', &beep);
  int idx2 = tui_menu_state_selected_index(s);
  bool ok = r1 < 0 && r2 < 0 && idx1 != idx2 && beep;
  tui_menu_state_destroy(s);
  return ok;
}

static bool test_mnemonic_disabled_beeps(void) {
  const tui_menu_item_t items[] = {
      {.label = "&Foo", .id = 1, .disabled = true},
      {.label = "&Bar", .id = 2},
  };
  const tui_menu_config_t cfg = {.items = items, .item_count = 2};
  tui_menu_state_t *s = NULL;
  if (tui_menu_state_create(&cfg, &s) != TUI_MENU_OK) return false;
  bool beep = false;
  int r = tui_menu_state_mnemonic_jump(s, L'f', &beep);
  bool ok = r < 0 && beep;
  tui_menu_state_destroy(s);
  return ok;
}
```

And inside `main`:

```c
unit_record(&stats, test_mnemonic_unique_returns_index(),
            "tui_menu unique mnemonic returns the items[] index");
unit_record(&stats, test_mnemonic_duplicate_cycles_no_confirm(),
            "tui_menu duplicate mnemonic cycles selection");
unit_record(&stats, test_mnemonic_disabled_beeps(),
            "tui_menu disabled mnemonic beeps, no confirm");
```

- [ ] **Step 2: Run to verify failure**

Run: `zig build unit-test`
Expected: three tests FAIL.

- [ ] **Step 3: Replace the `tui_menu_state_mnemonic_jump` stub in `src/tui/tui_menu_model.c`**

```c
int tui_menu_state_mnemonic_jump(tui_menu_state_t *s, wchar_t key,
                                 bool *out_beep) {
  if (out_beep) *out_beep = false;
  if (!s || key == 0) return -1;
  const wchar_t target = (wchar_t)towlower(key);

  int matches[2] = {-1, -1};
  int match_count = 0;
  bool has_disabled_match = false;
  for (int i = 0; i < s->cfg->item_count; i++) {
    if (s->mnemonics[i] != target) continue;
    const tui_menu_item_t *it = &s->cfg->items[i];
    if (it->disabled || it->kind == TUI_MENU_ITEM_SEPARATOR) {
      has_disabled_match = true;
      continue;
    }
    if (match_count < 2) matches[match_count] = i;
    match_count++;
  }

  if (match_count == 0) {
    if (has_disabled_match && out_beep) *out_beep = true;
    return -1;
  }
  if (match_count == 1) {
    return matches[0]; /* caller should auto-confirm */
  }

  /* Multiple matches — cycle selection through them, beep, no confirm. */
  const int current = tui_menu_state_selected_index(s);
  int target_visible = -1;
  bool past_current = false;
  for (int v = 0; v < s->visible_count; v++) {
    const int idx = s->visible[v];
    if (idx == current) { past_current = true; continue; }
    if (!past_current) continue;
    if (s->mnemonics[idx] == target &&
        menu_item_selectable(&s->cfg->items[idx])) {
      target_visible = v;
      break;
    }
  }
  if (target_visible < 0) {
    /* wrap to first match before current */
    for (int v = 0; v < s->visible_count; v++) {
      const int idx = s->visible[v];
      if (s->mnemonics[idx] == target &&
          menu_item_selectable(&s->cfg->items[idx])) {
        target_visible = v;
        break;
      }
    }
  }
  if (target_visible >= 0) s->selected_visible = target_visible;
  if (out_beep) *out_beep = true;
  return -1;
}
```

- [ ] **Step 4: Run unit tests**

Run: `zig build unit-test`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add src/tui/tui_menu_model.c test/unit_runner.c
git commit -m "feat(tui): mnemonic jump with case-fold, duplicate cycling, disabled beep

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 8: Model — incremental search filter (jump-narrow)

**Files:**
- Modify: `src/tui/tui_menu_model.c`
- Modify: `test/unit_runner.c`

- [ ] **Step 1: Write the failing tests**

Append to `test/unit_runner.c`:

```c
static bool test_search_filters_matches_to_front_of_selection(void) {
  const tui_menu_item_t items[] = {
      {.label = "&Overview", .id = 1},
      {.label = "&System Information", .id = 2},
      {.label = "&Input Dialog", .id = 3},
      {.label = "&Progress Pattern", .id = 4},
  };
  const tui_menu_config_t cfg = {.items = items, .item_count = 4};
  tui_menu_state_t *s = NULL;
  if (tui_menu_state_create(&cfg, &s) != TUI_MENU_OK) return false;

  tui_menu_state_search_open(s);
  tui_menu_state_search_append(s, L'p');
  tui_menu_state_search_append(s, L'r');
  tui_menu_state_search_append(s, L'o');

  /* "Progress Pattern" is the only "pro" substring match */
  bool ok = tui_menu_state_search_active(s) &&
            tui_menu_state_selected_index(s) == 3;

  tui_menu_state_destroy(s);
  return ok;
}

static bool test_search_case_insensitive(void) {
  const tui_menu_item_t items[] = {
      {.label = "Overview", .id = 1},
      {.label = "System", .id = 2},
  };
  const tui_menu_config_t cfg = {.items = items, .item_count = 2};
  tui_menu_state_t *s = NULL;
  if (tui_menu_state_create(&cfg, &s) != TUI_MENU_OK) return false;
  tui_menu_state_search_open(s);
  tui_menu_state_search_append(s, L'S');
  bool ok = tui_menu_state_selected_index(s) == 1;
  tui_menu_state_destroy(s);
  return ok;
}

static bool test_search_close_clears_query(void) {
  const tui_menu_item_t items[] = {{.label = "Foo", .id = 1}};
  const tui_menu_config_t cfg = {.items = items, .item_count = 1};
  tui_menu_state_t *s = NULL;
  if (tui_menu_state_create(&cfg, &s) != TUI_MENU_OK) return false;
  tui_menu_state_search_open(s);
  tui_menu_state_search_append(s, L'f');
  tui_menu_state_search_close(s);
  bool ok = !tui_menu_state_search_active(s) &&
            wcslen(tui_menu_state_search_query(s)) == 0;
  tui_menu_state_destroy(s);
  return ok;
}

static bool test_search_backspace_removes_one_wchar(void) {
  const tui_menu_item_t items[] = {{.label = "Foo", .id = 1}};
  const tui_menu_config_t cfg = {.items = items, .item_count = 1};
  tui_menu_state_t *s = NULL;
  if (tui_menu_state_create(&cfg, &s) != TUI_MENU_OK) return false;
  tui_menu_state_search_open(s);
  tui_menu_state_search_append(s, L'a');
  tui_menu_state_search_append(s, L'b');
  tui_menu_state_search_backspace(s);
  bool ok = wcscmp(tui_menu_state_search_query(s), L"a") == 0;
  tui_menu_state_destroy(s);
  return ok;
}
```

And inside `main`:

```c
unit_record(&stats, test_search_filters_matches_to_front_of_selection(),
            "tui_menu search snaps selection to first match");
unit_record(&stats, test_search_case_insensitive(),
            "tui_menu search is case-insensitive");
unit_record(&stats, test_search_close_clears_query(),
            "tui_menu search_close clears the query");
unit_record(&stats, test_search_backspace_removes_one_wchar(),
            "tui_menu search backspace pops one wchar");
```

- [ ] **Step 2: Run to verify failure**

Run: `zig build unit-test`
Expected: four tests FAIL.

- [ ] **Step 3: Replace `menu_state_apply_filter` and the search stubs in `src/tui/tui_menu_model.c`**

Replace `menu_state_apply_filter`:

```c
static bool wcs_contains_icase(const wchar_t *hay, const wchar_t *needle) {
  if (!needle || needle[0] == 0) return true;
  const size_t nlen = wcslen(needle);
  for (const wchar_t *p = hay; *p; p++) {
    size_t k = 0;
    while (k < nlen && p[k] != 0 &&
           towlower(p[k]) == towlower(needle[k])) k++;
    if (k == nlen) return true;
  }
  return false;
}

static void menu_state_apply_filter(struct tui_menu_state *s) {
  s->visible_count = 0;
  if (!s->search_active || s->search_len == 0) {
    for (int i = 0; i < s->cfg->item_count; i++) s->visible[s->visible_count++] = i;
    return;
  }
  s->search_buf[s->search_len] = 0;
  for (int i = 0; i < s->cfg->item_count; i++) {
    if (s->cfg->items[i].kind == TUI_MENU_ITEM_SEPARATOR) continue;
    const wchar_t *lab = s->label_w[i] ? s->label_w[i] : L"";
    if (wcs_contains_icase(lab, s->search_buf)) {
      s->visible[s->visible_count++] = i;
    }
  }
  /* Snap selection to first selectable visible. */
  s->selected_visible = 0;
  for (int v = 0; v < s->visible_count; v++) {
    if (menu_item_selectable(&s->cfg->items[s->visible[v]])) {
      s->selected_visible = v;
      return;
    }
  }
}
```

Replace the search stubs:

```c
void tui_menu_state_search_open(tui_menu_state_t *s) {
  if (!s) return;
  s->search_active = true;
  s->search_len = 0;
  s->search_buf[0] = 0;
  menu_state_apply_filter(s);
}

void tui_menu_state_search_close(tui_menu_state_t *s) {
  if (!s) return;
  s->search_active = false;
  s->search_len = 0;
  s->search_buf[0] = 0;
  menu_state_apply_filter(s);
}

void tui_menu_state_search_append(tui_menu_state_t *s, wchar_t ch) {
  if (!s || !s->search_active) return;
  const size_t cap = (sizeof(s->search_buf) / sizeof(s->search_buf[0])) - 1;
  if (s->search_len >= cap) return;
  s->search_buf[s->search_len++] = ch;
  s->search_buf[s->search_len] = 0;
  menu_state_apply_filter(s);
}

void tui_menu_state_search_backspace(tui_menu_state_t *s) {
  if (!s || !s->search_active || s->search_len == 0) return;
  s->search_len--;
  s->search_buf[s->search_len] = 0;
  menu_state_apply_filter(s);
}
```

- [ ] **Step 4: Run unit tests**

Run: `zig build unit-test`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add src/tui/tui_menu_model.c test/unit_runner.c
git commit -m "feat(tui): incremental search with case-insensitive substring match

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 9: Model — numeric accelerator

**Files:**
- Modify: `src/tui/tui_menu_model.c`
- Modify: `test/unit_runner.c`

- [ ] **Step 1: Write the failing test**

Append to `test/unit_runner.c`:

```c
static bool test_numeric_jump(void) {
  const tui_menu_item_t items[] = {
      {.label = "a", .id = 1}, {.label = "b", .id = 2},
      {.label = "c", .id = 3, .disabled = true},
      {.label = "d", .id = 4},
  };
  const tui_menu_config_t cfg = {.items = items, .item_count = 4};
  tui_menu_state_t *s = NULL;
  if (tui_menu_state_create(&cfg, &s) != TUI_MENU_OK) return false;
  tui_menu_state_numeric_jump(s, 1); /* visible row 1 = items[1] */
  if (tui_menu_state_selected_index(s) != 1) { tui_menu_state_destroy(s); return false; }
  tui_menu_state_numeric_jump(s, 2); /* disabled — must not change selection */
  bool ok = tui_menu_state_selected_index(s) == 1;
  tui_menu_state_destroy(s);
  return ok;
}
```

And inside `main`:

```c
unit_record(&stats, test_numeric_jump(),
            "tui_menu numeric jump targets visible row, skips disabled");
```

- [ ] **Step 2: Run to verify failure**

Run: `zig build unit-test`
Expected: test FAILS.

- [ ] **Step 3: Replace the stub in `src/tui/tui_menu_model.c`**

```c
void tui_menu_state_numeric_jump(tui_menu_state_t *s, int visible_row) {
  if (!s || visible_row < 0 || visible_row >= s->visible_count) return;
  if (menu_item_selectable(&s->cfg->items[s->visible[visible_row]])) {
    s->selected_visible = visible_row;
  }
}
```

- [ ] **Step 4: Run unit tests**

Run: `zig build unit-test`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add src/tui/tui_menu_model.c test/unit_runner.c
git commit -m "feat(tui): numeric accelerator jumps to visible row, skips disabled

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 10: View — `tui_menu.c` skeleton with wide-string helper

**Files:**
- Create: `src/tui/tui_menu.c`
- Modify: `build.zig` (add `tui_menu.c` to binary source list)

- [ ] **Step 1: Add `tui_menu.c` to `build.zig`**

In `build.zig`'s binary `addCSourceFiles` block (around line 188), add **after** the `tui_menu_model.c` entry added in Task 3:

```
"src/tui/tui_menu.c",
```

Do **not** add to the unit-test source list; this file pulls in ncurses.

- [ ] **Step 2: Create `src/tui/tui_menu.c` skeleton**

```c
/*
 * tui_menu.c — view + controller + orchestrator for the tui_menu module.
 *
 * The model lives in tui_menu_model.c (no ncurses). This file calls into
 * the model through tui_menu_internal.h.
 */
#ifdef _WIN32
#include <curses.h>
#else
#include <ncurses.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

#include "../utils/logging.h"
#include "tui.h"
#include "tui_menu.h"
#include "tui_menu_internal.h"

/* Column-correct wide-string write: budgets `cols` display columns,
 * truncates at glyph boundaries using wcwidth, never emits partial glyphs.
 */
static void tui_menu_write_wcs(WINDOW *w, int y, int x, int cols,
                               const wchar_t *s) {
  if (!w || !s || cols <= 0) return;
  const int max_y = getmaxy(w);
  const int max_x = getmaxx(w);
  if (y < 0 || y >= max_y || x < 0 || x >= max_x) return;
  if (cols > max_x - x) cols = max_x - x;
  if (cols <= 0) return;

  /* Walk s, accumulating wchars whose total column width fits. */
  int used = 0;
  size_t count = 0;
  for (; s[count]; count++) {
    int w_cols = wcwidth(s[count]);
    if (w_cols < 0) w_cols = 1; /* non-printable: best effort */
    if (used + w_cols > cols) break;
    used += w_cols;
  }
  mvwaddnwstr(w, y, x, s, (int)count);
}

/* Stub — implemented in Task 14. */
tui_menu_result_t tui_show_menu(tui_window_t *window,
                                const tui_menu_config_t *config) {
  (void)window;
  (void)config;
  return (tui_menu_result_t){.status = TUI_MENU_INVALID_ARG};
}
```

- [ ] **Step 3: Verify build**

Run: `zig build -Denable-tui=true`
Expected: build succeeds (stub `tui_show_menu` compiles; the old `tui_show_menu` in `tui.c` will conflict at link time — that's expected and is fixed in Task 19. Skip the binary link in this task by running unit-test only.)

Run: `zig build unit-test`
Expected: PASS — unit tests aren't affected.

- [ ] **Step 4: Commit**

```bash
git add src/tui/tui_menu.c build.zig
git commit -m "feat(tui): scaffold tui_menu.c with wide-char write helper

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 11: View — layout compute + frame ownership

**Files:**
- Modify: `src/tui/tui_menu.c`

- [ ] **Step 1: Append to `src/tui/tui_menu.c`**

```c
typedef struct {
  tui_window_t *frame;
  bool owns_frame;
  int item_area_y;
  int item_area_h;
  int detail_y;
  int detail_h;
  int footer_y;
  int inner_w;
  bool detail_pane_visible;
} tui_menu_layout_t;

#define MENU_MIN_ITEM_ROWS 3
#define MENU_FOOTER_ROWS 1
#define MENU_TITLE_ROWS 2   /* title + separator hline */
#define MENU_DETAIL_MIN 3   /* min rows for detail pane */

static bool tui_menu_layout_compute(tui_menu_layout_t *L,
                                    const tui_window_t *w,
                                    const tui_menu_config_t *cfg) {
  if (!L || !w || !cfg) return false;
  const int H = w->height;
  const int W = w->width;
  if (H < 6 + MENU_FOOTER_ROWS + MENU_MIN_ITEM_ROWS) return false;
  if (W < 24) return false;

  L->inner_w = W - 2;
  L->footer_y = H - 2;
  const int top_after_title = (cfg->title ? MENU_TITLE_ROWS : 0) + 1;
  const int avail = L->footer_y - top_after_title;

  if (cfg->show_detail_pane && avail >= MENU_MIN_ITEM_ROWS + MENU_DETAIL_MIN) {
    L->detail_pane_visible = true;
    L->detail_h = MENU_DETAIL_MIN;
    L->item_area_h = avail - L->detail_h;
    L->item_area_y = top_after_title;
    L->detail_y = L->item_area_y + L->item_area_h;
  } else {
    L->detail_pane_visible = false;
    L->item_area_h = avail;
    L->item_area_y = top_after_title;
    L->detail_h = 0;
    L->detail_y = 0;
  }
  return L->item_area_h >= MENU_MIN_ITEM_ROWS;
}
```

- [ ] **Step 2: Verify build (no test wiring yet)**

Run: `zig build unit-test`
Expected: PASS.

- [ ] **Step 3: Commit**

```bash
git add src/tui/tui_menu.c
git commit -m "feat(tui): menu layout compute with detail-pane collapse

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 12: View — render title + items + scrollbar

**Files:**
- Modify: `src/tui/tui_menu.c`

- [ ] **Step 1: Append to `src/tui/tui_menu.c`**

```c
static void tui_menu_render_title(const tui_menu_layout_t *L,
                                  const char *title) {
  if (!title || !L->frame) return;
  tui_set_color(L->frame->win, TUI_COLOR_TITLE);
  tui_print_centered(L->frame->win, 1, title);
  tui_unset_color(L->frame->win, TUI_COLOR_TITLE);
  tui_set_color(L->frame->win, TUI_COLOR_BORDER);
  mvwhline(L->frame->win, 2, 2, ACS_HLINE, L->frame->width - 4);
  tui_unset_color(L->frame->win, TUI_COLOR_BORDER);
}

static void tui_menu_render_items(const tui_menu_layout_t *L,
                                  const tui_menu_state_t *s) {
  WINDOW *win = L->frame->win;
  const int visible = tui_menu_state_visible_count(s);
  const int top = tui_menu_state_top_visible(s);
  const int sel_v = tui_menu_state_selected_visible(s);
  const int rows = L->item_area_h;
  const tui_menu_config_t *cfg = NULL;
  /* Reach into the public config via the items array of the first visible
   * entry — we don't expose cfg, so the controller passes layout + state. */
  /* Get cfg from state via a small accessor we add inline below. */
  /* For now we access items via state through a helper added in the next task. */
  (void)cfg;

  for (int row = 0; row < rows; row++) {
    const int v = top + row;
    if (v >= visible) break;
    const int idx = -1; /* filled by accessor in Task 13 */
    (void)idx;
    const int y = L->item_area_y + row;
    const bool is_selected = (v == sel_v);

    if (is_selected) {
      tui_set_color(win, TUI_COLOR_MENU_SELECTED);
      mvwhline(win, y, 1, ' ', L->frame->width - 2);
      tui_unset_color(win, TUI_COLOR_MENU_SELECTED);
      tui_set_color(win, TUI_COLOR_ACCENT);
      tui_menu_write_wcs(win, y, 2, 2, L"▸ ");
      tui_unset_color(win, TUI_COLOR_ACCENT);
    }
  }
}

static void tui_menu_render_scrollbar(const tui_menu_layout_t *L,
                                      const tui_menu_state_t *s) {
  WINDOW *win = L->frame->win;
  const int visible = tui_menu_state_visible_count(s);
  const int top = tui_menu_state_top_visible(s);
  const int rows = L->item_area_h;
  const int x = L->frame->width - 3;
  if (top > 0) {
    tui_set_color(win, TUI_COLOR_INFO);
    tui_menu_write_wcs(win, L->item_area_y, x, 1, L"▲");
    tui_unset_color(win, TUI_COLOR_INFO);
  }
  if (top + rows < visible) {
    tui_set_color(win, TUI_COLOR_INFO);
    tui_menu_write_wcs(win, L->item_area_y + rows - 1, x, 1, L"▼");
    tui_unset_color(win, TUI_COLOR_INFO);
  }
}
```

- [ ] **Step 2: Verify build**

Run: `zig build unit-test`
Expected: PASS.

- [ ] **Step 3: Commit**

```bash
git add src/tui/tui_menu.c
git commit -m "feat(tui): title/items/scrollbar render skeletons

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 13: View — full item row rendering (label + mnemonic underline + disabled hint)

**Files:**
- Modify: `src/tui/tui_menu_internal.h` (one accessor)
- Modify: `src/tui/tui_menu_model.c` (accessor implementation)
- Modify: `src/tui/tui_menu.c`

- [ ] **Step 1: Add an accessor to `src/tui/tui_menu_internal.h`**

Add this prototype near the other accessors:

```c
/* Return the items[] index at the given visible row, or -1. */
int tui_menu_state_visible_at(const tui_menu_state_t *s, int visible_row);
const tui_menu_config_t *tui_menu_state_config(const tui_menu_state_t *s);
```

- [ ] **Step 2: Implement in `src/tui/tui_menu_model.c`**

```c
int tui_menu_state_visible_at(const tui_menu_state_t *s, int v) {
  if (!s || v < 0 || v >= s->visible_count) return -1;
  return s->visible[v];
}

const tui_menu_config_t *tui_menu_state_config(const tui_menu_state_t *s) {
  return s ? s->cfg : NULL;
}
```

- [ ] **Step 3: Replace `tui_menu_render_items` body in `src/tui/tui_menu.c`**

```c
static void tui_menu_render_items(const tui_menu_layout_t *L,
                                  const tui_menu_state_t *s) {
  WINDOW *win = L->frame->win;
  const tui_menu_config_t *cfg = tui_menu_state_config(s);
  const int visible = tui_menu_state_visible_count(s);
  const int top = tui_menu_state_top_visible(s);
  const int sel_v = tui_menu_state_selected_visible(s);
  const int rows = L->item_area_h;
  const int label_x = 4;
  const int label_text_x = label_x + 3;
  const int label_max_w = L->frame->width - label_text_x - 4;

  for (int row = 0; row < rows; row++) {
    const int v = top + row;
    if (v >= visible) break;
    const int idx = tui_menu_state_visible_at(s, v);
    const tui_menu_item_t *it = &cfg->items[idx];
    const int y = L->item_area_y + row;
    const bool is_selected = (v == sel_v);

    if (it->kind == TUI_MENU_ITEM_SEPARATOR) {
      tui_set_color(win, TUI_COLOR_BORDER);
      mvwhline(win, y, 3, ACS_HLINE, L->frame->width - 6);
      tui_unset_color(win, TUI_COLOR_BORDER);
      continue;
    }

    if (is_selected) {
      tui_set_color(win, TUI_COLOR_MENU_SELECTED);
      mvwhline(win, y, 1, ' ', L->frame->width - 2);
      tui_unset_color(win, TUI_COLOR_MENU_SELECTED);
      tui_set_color(win, TUI_COLOR_ACCENT);
      tui_menu_write_wcs(win, y, 2, 2, L"▸ ");
      tui_unset_color(win, TUI_COLOR_ACCENT);
    }

    /* Numeric prefix shown when numeric keys are enabled and row < 9 in
     * the *visible* slice. */
    if (cfg->show_numeric_keys && v < 9) {
      char num[4];
      snprintf(num, sizeof(num), "%d.", v + 1);
      if (is_selected) tui_set_color(win, TUI_COLOR_MENU_SELECTED);
      else             tui_set_color(win, TUI_COLOR_INFO);
      mvwaddnstr(win, y, label_x, num, 3);
      if (is_selected) tui_unset_color(win, TUI_COLOR_MENU_SELECTED);
      else             tui_unset_color(win, TUI_COLOR_INFO);
    }

    /* Label, with mnemonic underline. */
    if (is_selected) tui_set_color(win, TUI_COLOR_MENU_SELECTED);
    else if (it->disabled) tui_set_color(win, TUI_COLOR_DIM);
    const wchar_t *lab = tui_menu_state_label_wcs(s, idx);
    const wchar_t mn = tui_menu_state_mnemonic(s, idx);
    /* Render the label, applying A_UNDERLINE to the first wchar matching
     * mn (case-insensitive) — single pass. */
    int cur_x = label_text_x;
    int budget = label_max_w;
    bool underlined = (mn == 0);
    for (size_t k = 0; lab[k] && budget > 0; k++) {
      int cw = wcwidth(lab[k]); if (cw < 0) cw = 1;
      if (cw > budget) break;
      const bool here = !underlined && (wchar_t)towlower(lab[k]) == mn;
      if (here) wattron(win, A_UNDERLINE);
      mvwaddnwstr(win, y, cur_x, &lab[k], 1);
      if (here) { wattroff(win, A_UNDERLINE); underlined = true; }
      cur_x += cw;
      budget -= cw;
    }
    if (is_selected) tui_unset_color(win, TUI_COLOR_MENU_SELECTED);
    else if (it->disabled) tui_unset_color(win, TUI_COLOR_DIM);

    if (it->disabled) {
      tui_set_color(win, TUI_COLOR_DIM);
      tui_menu_write_wcs(win, y, L->frame->width - 13, 11, L"(disabled) ");
      tui_unset_color(win, TUI_COLOR_DIM);
    }
  }
}
```

- [ ] **Step 4: Verify build**

Run: `zig build unit-test && zig build -Denable-tui=true` (the binary build will still fail at link in `tui.c`'s old `tui_show_menu`, but unit tests compile.)
Expected: unit-test PASS.

- [ ] **Step 5: Commit**

```bash
git add src/tui/tui_menu_internal.h src/tui/tui_menu_model.c src/tui/tui_menu.c
git commit -m "feat(tui): full item row render with mnemonic underline + dim disabled

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 14: View — detail pane + footer

**Files:**
- Modify: `src/tui/tui_menu.c`

- [ ] **Step 1: Append to `src/tui/tui_menu.c`**

```c
static void tui_menu_render_detail(const tui_menu_layout_t *L,
                                   const tui_menu_state_t *s) {
  if (!L->detail_pane_visible) return;
  WINDOW *win = L->frame->win;
  const tui_menu_config_t *cfg = tui_menu_state_config(s);
  const int sel_v = tui_menu_state_selected_visible(s);
  const int idx = tui_menu_state_visible_at(s, sel_v);
  if (idx < 0) return;

  tui_set_color(win, TUI_COLOR_BORDER);
  mvwhline(win, L->detail_y, 2, ACS_HLINE, L->frame->width - 4);
  tui_unset_color(win, TUI_COLOR_BORDER);
  const char *desc = cfg->items[idx].description;
  if (desc) {
    tui_print_wrapped(win, L->detail_y + 1, 3, L->frame->width - 6, desc);
  }
}

static void tui_menu_render_footer(const tui_menu_layout_t *L,
                                   const tui_menu_state_t *s) {
  WINDOW *win = L->frame->win;
  const tui_menu_config_t *cfg = tui_menu_state_config(s);
  tui_set_color(win, TUI_COLOR_INFO);
  mvwhline(win, L->footer_y, 1, ' ', L->frame->width - 2);

  if (tui_menu_state_search_active(s)) {
    char buf[128];
    /* Convert the wchar query to a multibyte string for display. */
    char mb[64];
    const wchar_t *q = tui_menu_state_search_query(s);
    size_t n = wcstombs(mb, q, sizeof(mb) - 1);
    if (n == (size_t)-1) n = 0;
    mb[n] = 0;
    snprintf(buf, sizeof(buf), "Search: %s_     Esc cancel", mb);
    mvwaddnstr(win, L->footer_y, 3, buf, L->frame->width - 6);
  } else {
    const char *parts =
        cfg->enable_search && cfg->show_numeric_keys
            ? "  navigate  / search  1-9 jump  Enter ok  q quit"
        : cfg->enable_search ? "  navigate  / search  Enter ok  q quit"
        : cfg->show_numeric_keys
            ? "  navigate  1-9 jump  Enter ok  q quit"
            : "  navigate  Enter ok  q quit";
    mvwaddnstr(win, L->footer_y, 3, parts, L->frame->width - 6);
  }
  tui_unset_color(win, TUI_COLOR_INFO);
}
```

- [ ] **Step 2: Verify build**

Run: `zig build unit-test`
Expected: PASS.

- [ ] **Step 3: Commit**

```bash
git add src/tui/tui_menu.c
git commit -m "feat(tui): detail pane and adaptive footer renderers

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 15: Controller — key + mouse dispatch

**Files:**
- Modify: `src/tui/tui_menu.c`

- [ ] **Step 1: Append to `src/tui/tui_menu.c`**

```c
typedef enum {
  TUI_MENU_EV_NONE,
  TUI_MENU_EV_CONFIRM,
  TUI_MENU_EV_CANCEL,
  TUI_MENU_EV_INTERRUPT,
  TUI_MENU_EV_REDRAW,
} tui_menu_event_t;

static tui_menu_event_t menu_handle_key_in_search(tui_menu_state_t *s,
                                                  int ch) {
  switch (ch) {
  case 27: /* Esc */
    tui_menu_state_search_close(s);
    return TUI_MENU_EV_NONE;
  case '\n':
  case KEY_ENTER:
    return TUI_MENU_EV_CONFIRM;
  case KEY_BACKSPACE:
  case 127:
    tui_menu_state_search_backspace(s);
    return TUI_MENU_EV_NONE;
  default:
    if (ch >= 32 && ch < 0x7f) {
      tui_menu_state_search_append(s, (wchar_t)ch);
    }
    return TUI_MENU_EV_NONE;
  }
}

static tui_menu_event_t menu_handle_key(tui_menu_state_t *s, int ch,
                                        int *out_confirm_id) {
  const tui_menu_config_t *cfg = tui_menu_state_config(s);
  if (tui_menu_state_search_active(s)) {
    return menu_handle_key_in_search(s, ch);
  }
  switch (ch) {
  case KEY_UP:   case 'k': tui_menu_state_step(s, -1); return TUI_MENU_EV_NONE;
  case KEY_DOWN: case 'j': tui_menu_state_step(s,  1); return TUI_MENU_EV_NONE;
  case KEY_HOME:           tui_menu_state_home(s);    return TUI_MENU_EV_NONE;
  case KEY_END:            tui_menu_state_end(s);     return TUI_MENU_EV_NONE;
  case KEY_PPAGE:          tui_menu_state_page(s, -1, 8); return TUI_MENU_EV_NONE;
  case KEY_NPAGE:          tui_menu_state_page(s,  1, 8); return TUI_MENU_EV_NONE;
  case '\n': case KEY_ENTER: return TUI_MENU_EV_CONFIRM;
  case 'q': case 27:         return TUI_MENU_EV_CANCEL;
  case '/':
    if (cfg->enable_search) tui_menu_state_search_open(s);
    return TUI_MENU_EV_NONE;
  default:
    if (cfg->show_numeric_keys && ch >= '1' && ch <= '9') {
      tui_menu_state_numeric_jump(s, ch - '1');
      return TUI_MENU_EV_NONE;
    }
    if (iswalnum((wint_t)ch)) {
      bool beep = false;
      int auto_idx = tui_menu_state_mnemonic_jump(s, (wchar_t)ch, &beep);
      if (auto_idx >= 0) {
        *out_confirm_id = cfg->items[auto_idx].id;
        return TUI_MENU_EV_CONFIRM;
      }
      if (beep) tui_beep();
    }
    return TUI_MENU_EV_NONE;
  }
}

#ifdef NCURSES_MOUSE_VERSION
static tui_menu_event_t menu_handle_mouse(tui_menu_state_t *s,
                                          const tui_menu_layout_t *L,
                                          int *out_confirm_id) {
  MEVENT ev;
  if (getmouse(&ev) != OK) return TUI_MENU_EV_NONE;
  int wy = ev.y, wx = ev.x;
  if (!wmouse_trafo(L->frame->win, &wy, &wx, FALSE)) return TUI_MENU_EV_NONE;

  if (wy < L->item_area_y || wy >= L->item_area_y + L->item_area_h) {
    return TUI_MENU_EV_NONE;
  }
  const int row = wy - L->item_area_y;
  const int v = tui_menu_state_top_visible(s) + row;
  if (v < 0 || v >= tui_menu_state_visible_count(s)) return TUI_MENU_EV_NONE;
  const tui_menu_config_t *cfg = tui_menu_state_config(s);
  const int idx = tui_menu_state_visible_at(s, v);
  const tui_menu_item_t *it = &cfg->items[idx];

  if (ev.bstate & BUTTON4_PRESSED) { tui_menu_state_step(s, -1); return TUI_MENU_EV_NONE; }
  if (ev.bstate & BUTTON5_PRESSED) { tui_menu_state_step(s,  1); return TUI_MENU_EV_NONE; }

  if (it->kind == TUI_MENU_ITEM_SEPARATOR || it->disabled) {
    tui_beep();
    return TUI_MENU_EV_NONE;
  }
  if (ev.bstate & BUTTON1_CLICKED) {
    /* Set selection but don't confirm. */
    const int sel_v = tui_menu_state_selected_visible(s);
    if (v != sel_v) {
      /* step to v using the model API by walking */
      const int delta = v - sel_v;
      tui_menu_state_step(s, delta > 0 ? 1 : -1);
    }
    return TUI_MENU_EV_NONE;
  }
  if (ev.bstate & BUTTON1_DOUBLE_CLICKED) {
    *out_confirm_id = it->id;
    return TUI_MENU_EV_CONFIRM;
  }
  return TUI_MENU_EV_NONE;
}
#endif
```

- [ ] **Step 2: Verify build**

Run: `zig build unit-test`
Expected: PASS.

- [ ] **Step 3: Commit**

```bash
git add src/tui/tui_menu.c
git commit -m "feat(tui): key and mouse dispatch for the menu controller

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 16: Orchestrator — `tui_show_menu` real implementation

**Files:**
- Modify: `src/tui/tui_menu.c`

- [ ] **Step 1: Replace the stub `tui_show_menu` in `src/tui/tui_menu.c`**

Locate the existing stub (added in Task 10) and replace its entire body:

```c
/* Forward declarations for the helpers we registered with tui_set_background_window
 * — declared in tui.h (currently public; made internal-only in Task 19). */
void tui_set_background_window(tui_window_t *window);
void tui_clear_background_window(void);

tui_menu_result_t tui_show_menu(tui_window_t *window,
                                const tui_menu_config_t *config) {
  tui_menu_result_t result = {.status = TUI_MENU_INVALID_ARG, .selected_id = 0,
                              .selected_index = -1};
  if (!config) return result;

  tui_menu_state_t *state = NULL;
  tui_menu_status_t st = tui_menu_state_create(config, &state);
  if (st != TUI_MENU_OK) { result.status = st; return result; }

  tui_menu_layout_t L = {0};
  L.frame = window;
  L.owns_frame = (window == NULL);
  if (L.owns_frame) {
    const int h = config->frame_height > 0 ? config->frame_height : 22;
    const int w = config->frame_width  > 0 ? config->frame_width  : 72;
    L.frame = tui_create_centered_window(h, w);
    if (!L.frame) {
      tui_menu_state_destroy(state);
      result.status = TUI_MENU_TOO_SMALL;
      return result;
    }
    tui_draw_border(L.frame);
    if (config->title) tui_set_window_title(L.frame, config->title);
  }

  tui_set_background_window(L.frame);

#ifdef NCURSES_MOUSE_VERSION
  mmask_t prev_mask = 0;
  if (config->enable_mouse) {
    mousemask(BUTTON1_CLICKED | BUTTON1_DOUBLE_CLICKED |
                  BUTTON4_PRESSED | BUTTON5_PRESSED,
              &prev_mask);
  }
#endif

  bool exit_loop = false;
  while (!exit_loop) {
    if (!tui_menu_layout_compute(&L, L.frame, config)) {
      result.status = TUI_MENU_TOO_SMALL;
      break;
    }
    werase(L.frame->win);
    tui_draw_border(L.frame);
    if (config->title) tui_menu_render_title(&L, config->title);

    /* Adjust top_visible so selection stays in view. */
    const int sel_v = tui_menu_state_selected_visible(state);
    int top = tui_menu_state_top_visible(state);
    if (sel_v < top) top = sel_v;
    else if (sel_v >= top + L.item_area_h) top = sel_v - L.item_area_h + 1;
    if (top < 0) top = 0;
    tui_menu_state_set_top_visible(state, top);

    tui_menu_render_items(&L, state);
    tui_menu_render_scrollbar(&L, state);
    if (L.detail_pane_visible) tui_menu_render_detail(&L, state);
    tui_menu_render_footer(&L, state);

    wnoutrefresh(L.frame->win);
    doupdate();

    const int ch = wgetch(L.frame->win);
    int confirm_id = 0;
    tui_menu_event_t ev = TUI_MENU_EV_NONE;

    if (ch == ERR) {
      if (tui_interrupted()) {
        result.status = TUI_MENU_INTERRUPTED;
        exit_loop = true;
      }
      continue;
    }
    if (ch == KEY_RESIZE) {
      if (L.owns_frame) {
        tui_destroy_window(L.frame);
        const int h = config->frame_height > 0 ? config->frame_height : 22;
        const int w = config->frame_width  > 0 ? config->frame_width  : 72;
        L.frame = tui_create_centered_window(h, w);
        if (!L.frame) { result.status = TUI_MENU_TOO_SMALL; break; }
        tui_draw_border(L.frame);
        if (config->title) tui_set_window_title(L.frame, config->title);
        tui_set_background_window(L.frame);
      }
      continue;
    }
#ifdef NCURSES_MOUSE_VERSION
    if (ch == KEY_MOUSE) {
      ev = menu_handle_mouse(state, &L, &confirm_id);
    } else
#endif
    {
      ev = menu_handle_key(state, ch, &confirm_id);
    }

    switch (ev) {
    case TUI_MENU_EV_CONFIRM: {
      const int idx = tui_menu_state_selected_index(state);
      if (confirm_id != 0) {
        result.status = TUI_MENU_OK;
        result.selected_id = confirm_id;
        result.selected_index = idx;
      } else if (idx >= 0) {
        const tui_menu_item_t *it = &config->items[idx];
        if (it->disabled || it->kind == TUI_MENU_ITEM_SEPARATOR) {
          tui_beep();
          continue;
        }
        result.status = TUI_MENU_OK;
        result.selected_id = it->id;
        result.selected_index = idx;
      }
      exit_loop = true;
      break;
    }
    case TUI_MENU_EV_CANCEL:
      result.status = TUI_MENU_CANCELLED;
      exit_loop = true;
      break;
    case TUI_MENU_EV_INTERRUPT:
      result.status = TUI_MENU_INTERRUPTED;
      exit_loop = true;
      break;
    case TUI_MENU_EV_NONE:
    case TUI_MENU_EV_REDRAW:
      break;
    }
  }

#ifdef NCURSES_MOUSE_VERSION
  if (config->enable_mouse) mousemask(prev_mask, NULL);
#endif

  tui_clear_background_window();
  if (L.owns_frame && L.frame) tui_destroy_window(L.frame);
  tui_menu_state_destroy(state);
  return result;
}
```

- [ ] **Step 2: Verify unit tests still pass**

Run: `zig build unit-test`
Expected: PASS (no model-layer regressions).

- [ ] **Step 3: Commit**

```bash
git add src/tui/tui_menu.c
git commit -m "feat(tui): tui_show_menu orchestrator with frame ownership + event loop

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 17: Remove old `tui_show_menu` and obsolete types from `tui.c` / `tui.h`

**Files:**
- Modify: `src/tui/tui.h`
- Modify: `src/tui/tui.c`

- [ ] **Step 1: Edit `src/tui/tui.h`**

Delete the existing definitions:

- Remove the `tui_menu_item_t` struct definition (lines ~50-55 in the current file).
- Remove the `tui_show_menu` prototype (lines ~93-95).
- Remove the `tui_set_background_window` and `tui_clear_background_window` prototypes (lines ~64-65). They become internal — declared in `tui_menu.c` directly.
- Rename `tui_run_demo` to `tui_run_app` (line ~102).
- At the end of the file (before `#include "tui_progress.h"`), add `#include "tui_menu.h"`.

- [ ] **Step 2: Edit `src/tui/tui.c`**

- Delete the entire `tui_show_menu` function (and its `tui_menu_next_enabled` helper).
- Delete the `tui_set_background_window` and `tui_clear_background_window` function bodies.
- Move the `tui_background_win` static, `tui_set_background_window` and `tui_clear_background_window` into `tui_menu.c` as internal symbols (still callable from `tui_modal_open`/`tui_modal_close` in `tui.c` via an extern declaration). Concrete steps:
  - Cut the three symbols out of `tui.c`.
  - Paste them at the **top** of `tui_menu.c` (above the `tui_show_menu` body but below the wide-string helper). They become file-static in `tui_menu.c`.
  - Replace the file-static definitions with **non-static** versions so `tui.c` can link against them (the modal helpers in `tui.c` already call them).

So at the top of `tui_menu.c`, add:

```c
static tui_window_t *tui_menu_background_win = NULL;

void tui_set_background_window(tui_window_t *window) {
  tui_menu_background_win = window;
}
void tui_clear_background_window(void) { tui_menu_background_win = NULL; }
```

And in `tui.c`, replace any reference to `tui_background_win` with a call to a getter — or expose `tui_menu_background_win` via a `tui_get_background_window` accessor. Simplest path: change `tui.c`'s modal-redraw block from reading the static directly to calling a new getter `tui_window_t *tui_get_background_window(void);` declared in a new private header `src/tui/tui_internal.h` (or just forward-declared at top of `tui.c`).

Add to `tui_menu.c`:

```c
tui_window_t *tui_get_background_window(void) { return tui_menu_background_win; }
```

In `tui.c`, replace the `tui_background_win` references with:

```c
extern tui_window_t *tui_get_background_window(void);
/* ... */
tui_window_t *bg = tui_get_background_window();
if (bg && bg->win) { /* same body as before, using bg instead */ }
```

- [ ] **Step 3: Build the binary**

Run: `zig build -Denable-tui=true`
Expected: build succeeds (or fails only on `tui_run_demo` → `tui_run_app` rename — fixed in Task 18).

- [ ] **Step 4: Commit**

```bash
git add src/tui/tui.h src/tui/tui.c src/tui/tui_menu.c
git commit -m "refactor(tui): remove old tui_show_menu; move background-window into tui_menu

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 18: Rename `tui_demo.c` → `tui_app.c` and restructure

**Files:**
- Rename: `src/tui/tui_demo.c` → `src/tui/tui_app.c`
- Modify: `src/tui/tui_app.c` (rewrite to new API, three-region layout)
- Modify: `build.zig` (line 189)
- Modify: `src/cli/commands_menu.c` (one line)
- Modify: `src/cli/commands.c` (summary text)

- [ ] **Step 1: Rename the file**

```bash
git mv src/tui/tui_demo.c src/tui/tui_app.c
```

- [ ] **Step 2: Rewrite `src/tui/tui_app.c` to use the new API**

Replace the file's contents entirely:

```c
/*
 * tui_app.c — main menu entry point for this template's TUI.
 *
 * This file is the starter shell. The functions below are organized so you can:
 *
 *   1. Edit `main_menu[]` to replace the seed items with your commands.
 *   2. Replace or delete the `app_show_*` handlers that you don't need.
 *   3. Keep `tui_run_app()` as-is — the resize loop, exit confirmation, and
 *      cancel/interrupt handling are part of the reference implementation
 *      and Just Work for any item list you supply.
 *
 * The handlers below also serve as worked examples of every TUI primitive
 * (message dialog, confirm, input dialog, progress bar, custom layout,
 * sub-menu). Treat them as documentation you can delete or copy from.
 */
#include <stdio.h>

#include "tui.h"

/* ============================================================
 * Section 1: Handlers — replace these with your app's actions.
 * ============================================================ */

static void app_show_overview(void) {
  tui_show_message(
      "Starter Overview",
      "This template gives you a production-shaped starting point:\n\n"
      "  C23 modules with explicit error codes\n"
      "  Zig build graph with optional ncurses support\n"
      "  CLI output modes for humans and automation\n"
      "  Reusable TUI windows, menus, dialogs, and progress bars");
}

static void app_show_system_info(void) {
  char info[512];
  snprintf(info, sizeof(info),
           "Application: %s\n"
           "Version: %s\n"
           "Git Commit: %s\n"
           "Build Date: %s\n"
           "Terminal Size: %dx%d\n"
           "Colors Supported: %s",
           APP_NAME, APP_VERSION, APP_GIT_COMMIT, APP_BUILD_DATE,
           tui_get_max_x(), tui_get_max_y(), has_colors() ? "yes" : "no");
  tui_show_message("System Information", info);
}

static void app_show_input(void) {
  char buf[128] = {0};
  if (tui_input_dialog("Input Dialog", "Enter a display name:", buf,
                       sizeof(buf)) != APP_SUCCESS) {
    return;
  }
  char msg[256];
  snprintf(msg, sizeof(msg),
           "Hello, %s.\n\n"
           "Use this pattern for short prompts. For secrets, keep echo "
           "and cursor state scoped inside a dedicated helper.",
           buf[0] != '\0' ? buf : "World");
  tui_show_message("Input Captured", msg);
}

static void app_show_progress(void) {
  tui_progress_t *progress = tui_progress_create("Progress Pattern", 100);
  if (!progress) {
    tui_show_message("Progress",
                     "The terminal is too small for the progress example.");
    return;
  }
  for (int i = 0; i <= 100; i += 5) {
    char status[80];
    snprintf(status, sizeof(status), "Processing step %d of 20", i / 5);
    tui_progress_update(progress, i, status);
    napms(35);
  }
  tui_progress_destroy(progress);
  tui_show_message("Progress Complete",
                   "The progress helper clamps values, renders a "
                   "percentage, and owns its window lifecycle.");
}

static void app_show_layout(void) {
  tui_window_t *win = tui_create_centered_window(14, 72);
  if (!win) return;
  tui_draw_border(win);
  tui_set_window_title(win, "Layout Pattern");
  tui_set_color(win->win, TUI_COLOR_TITLE);
  tui_print_centered(win->win, 2, "Composable terminal UI");
  tui_unset_color(win->win, TUI_COLOR_TITLE);
  tui_print_wrapped(win->win, 4, 4, win->width - 8,
                    "Prefer small owned windows, bounded writes, and "
                    "status lines. Keep raw ncurses calls inside src/tui so "
                    "command code can stay easy to test.");
  tui_draw_status_line(win->win, "Enter/Esc closes this panel", APP_NAME);
  tui_refresh_window(win);
  while (true) {
    int ch = tui_get_char();
    if (ch == '\n' || ch == KEY_ENTER || ch == 27 || ch == 'q' || ch == 'Q') break;
  }
  tui_destroy_window(win);
  touchwin(stdscr);
  refresh();
}

static void app_show_config_menu(void) {
  static const tui_menu_item_t cfg_items[] = {
      {.label = "Output &mode", .description = "Plain, JSON, or colorized output", .id = 1},
      {.label = "&Log level", .description = "Quiet, normal, debug verbosity", .id = 2},
      {.label = "&Terminal settings", .description = "Color, minimum dimensions, fallbacks", .id = 3},
      {.label = "&Reset to defaults", .description = "Restore all configuration", .id = 4},
      {.label = "&Export config", .description = "Write current settings to disk", .id = 5, .disabled = true},
      {.kind = TUI_MENU_ITEM_SEPARATOR},
      {.label = "&Back", .description = "Return to the main menu", .id = 0},
  };
  bool sub_running = true;
  while (sub_running) {
    tui_menu_result_t r = tui_show_menu(NULL, &(tui_menu_config_t){
        .title = "Configuration",
        .items = cfg_items,
        .item_count = (int)(sizeof(cfg_items) / sizeof(cfg_items[0])),
        .default_index = 0,
        .frame_height = 16, .frame_width = 60,
        .enable_search = true,
        .show_detail_pane = true,
        .show_numeric_keys = true,
    });
    if (r.status != TUI_MENU_OK) { sub_running = false; continue; }
    switch (r.selected_id) {
    case 0:  sub_running = false; break;
    case 1:  tui_show_message("Output Mode", "Set via --json, --plain, or --quiet flags."); break;
    case 2:  tui_show_message("Log Level", "Use --debug for verbose logging."); break;
    case 3:  tui_show_message("Terminal Settings", "Minimum terminal: 48 x 12."); break;
    case 4:  tui_show_message("Reset to Defaults", "All settings reverted."); break;
    default: tui_beep(); break;
    }
  }
}

/* ============================================================
 * Section 2: Menu definition — your items + dispatch table.
 * ============================================================ */

static const tui_menu_item_t main_menu[] = {
    {.label = "&Overview",
     .description = "See the starter's CLI, TUI foundation, and core architecture",
     .id = 1},
    {.label = "&System Information",
     .description = "Inspect build metadata, terminal dimensions, and color support",
     .id = 2},
    {.label = "&Input Dialog",
     .description = "Capture bounded text with scoped echo and cursor state",
     .id = 3},
    {.kind = TUI_MENU_ITEM_SEPARATOR},
    {.label = "&Progress Pattern",
     .description = "Show a modal progress indicator with percentage and status",
     .id = 4},
    {.label = "&Layout Pattern",
     .description = "Open a reusable bordered panel with a status line",
     .id = 5},
    {.label = "&Configuration",
     .description = "Adjust output mode, log level, and terminal settings",
     .id = 6},
    {.kind = TUI_MENU_ITEM_SEPARATOR},
    {.label = "E&xit", .description = "Return to the shell", .id = 0},
};

static void app_dispatch(int id) {
  switch (id) {
  case 1: app_show_overview();    break;
  case 2: app_show_system_info(); break;
  case 3: app_show_input();       break;
  case 4: app_show_progress();    break;
  case 5: app_show_layout();      break;
  case 6: app_show_config_menu(); break;
  default: tui_beep();            break;
  }
}

/* ============================================================
 * Section 3: Entry point — usually no edits needed here.
 * ============================================================ */

app_error tui_run_app(void) {
  app_error err = tui_init();
  if (err != APP_SUCCESS) return err;

  bool running = true;
  while (running) {
    tui_menu_result_t r = tui_show_menu(NULL, &(tui_menu_config_t){
        .title             = "Starter Showcase",
        .items             = main_menu,
        .item_count        = (int)(sizeof(main_menu) / sizeof(main_menu[0])),
        .default_index     = 0,
        .frame_height      = 22,
        .frame_width       = 72,
        .enable_search     = true,
        .enable_mouse      = true,
        .show_detail_pane  = true,
        .show_numeric_keys = true,
    });
    switch (r.status) {
    case TUI_MENU_OK:
      if (r.selected_id == 0) {
        running = !tui_confirm("Exit", "Return to the shell?");
      } else {
        app_dispatch(r.selected_id);
      }
      break;
    case TUI_MENU_CANCELLED:
      running = !tui_confirm("Exit", "Return to the shell?");
      break;
    case TUI_MENU_INTERRUPTED:
      running = false;
      break;
    case TUI_MENU_TOO_SMALL:
    case TUI_MENU_INVALID_ARG:
      running = false;
      err = APP_ERROR_OUT_OF_RANGE;
      break;
    }
  }

  tui_cleanup();
  return err;
}
```

- [ ] **Step 3: Update `build.zig:189`**

Open `build.zig`. Find the line `"src/tui/tui_demo.c",` (was line 189 before the Task-3 insertion; now shifted by one). Change it to:

```
"src/tui/tui_app.c",
```

- [ ] **Step 4: Update `src/cli/commands_menu.c:28`**

Change:

```c
const app_error tui_err = tui_run_demo();
```

to:

```c
const app_error tui_err = tui_run_app();
```

- [ ] **Step 5: Update `src/cli/commands.c` summary**

Change the menu command summary string from:

```c
.summary = "Launch the interactive TUI showcase (requires --enable-tui).",
```

to:

```c
.summary = "Launch the interactive TUI main menu (requires --enable-tui).",
```

- [ ] **Step 6: Build**

Run: `zig build -Denable-tui=true`
Expected: build succeeds.

- [ ] **Step 7: Commit**

```bash
git add src/tui/tui_app.c build.zig src/cli/commands_menu.c src/cli/commands.c src/tui/tui.h
git commit -m "$(cat <<'EOF'
refactor(tui): rename tui_demo.c -> tui_app.c with new menu API

The TUI menu file is the starter shell template consumers inherit;
the demo naming was misleading. Restructure into three labeled
regions: handlers, menu definition, entry point. All handlers and
the menu literal use the new tui_show_menu(NULL, &config) form.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 19: Upgrade `signal()` → `sigaction()` with `SA_RESTART` cleared on SIGINT

**Files:**
- Modify: `src/tui/tui.c`

- [ ] **Step 1: Replace the `tui_install_signal_handlers` / `tui_uninstall_signal_handlers` block**

Locate the existing block near the top of `src/tui/tui.c` and replace:

```c
static void tui_install_signal_handlers(void) {
#ifndef _WIN32
  struct sigaction sa_int = {.sa_handler = tui_signal_handler};
  sigemptyset(&sa_int.sa_mask);
  /* Clear SA_RESTART so wgetch returns ERR and the menu loop can react. */
  sa_int.sa_flags = 0;
  sigaction(SIGINT, &sa_int, NULL);

  struct sigaction sa_term = {.sa_handler = tui_signal_handler};
  sigemptyset(&sa_term.sa_mask);
  sa_term.sa_flags = SA_RESTART;
  sigaction(SIGTERM, &sa_term, NULL);
#endif
}

static void tui_uninstall_signal_handlers(void) {
#ifndef _WIN32
  struct sigaction sa = {.sa_handler = SIG_DFL};
  sigemptyset(&sa.sa_mask);
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);
#endif
}
```

Also update the handler body to save/restore errno:

```c
#ifndef _WIN32
static void tui_signal_handler(int signum) {
  (void)signum;
  const int saved_errno = errno;
  tui_interrupted_flag = 1;
  errno = saved_errno;
}
#endif
```

Add `#include <errno.h>` near the top of `src/tui/tui.c` if not already present.

- [ ] **Step 2: Build**

Run: `zig build -Denable-tui=true`
Expected: build succeeds.

- [ ] **Step 3: Commit**

```bash
git add src/tui/tui.c
git commit -m "fix(tui): use sigaction with SA_RESTART cleared on SIGINT

signal() leaves the SA_RESTART behavior platform-dependent. sigaction
gives us explicit control: SA_RESTART cleared on SIGINT so wgetch
returns ERR (letting the menu loop check tui_interrupted), and the
handler now saves/restores errno per the one-breath rule.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 20: VT test migration — switch existing scenarios to mnemonic-driven selection

**Files:**
- Modify: `test/terminal_vt_scenarios.c`

- [ ] **Step 1: Build first to confirm a green baseline**

Run: `zig build -Denable-tui=true terminal-test`
Expected: existing VT tests should still pass after Tasks 1-19 — the new menu renders the same labels, so `vt_expect_text("Overview", ...)` etc. still matches. If failures, fix before proceeding.

- [ ] **Step 2: Replace `vt_select_menu_index` with a mnemonic helper**

In `test/terminal_vt_scenarios.c`, find the function `vt_select_menu_index` near the top and replace with:

```c
/* Select a menu item by its mnemonic letter (lower-case). */
static bool vt_select_menu_mnemonic(vt_session_t *session, char mnemonic) {
  char buf[2] = {mnemonic, 0};
  return vt_send(session, buf);
}
```

Update the `tui_step_kind_t` enum and `tui_step_t` struct: replace `TUI_STEP_SELECT` (and its `menu_index` field) with `TUI_STEP_MNEMONIC` and a `char mnemonic` field. Or simpler — keep the enum/struct shape and reinterpret `menu_index` as `mnemonic` (an int holding a char).

For minimal diff, replace all six `{TUI_STEP_SELECT, NULL, N, ...}` invocations with `{TUI_STEP_SEND, "o", ...}` etc. using mnemonics:

- `{TUI_STEP_SEND, "o", 0, 0, 0, 0, "failed to select Overview"}`
- `{TUI_STEP_SEND, "s", 0, 0, 0, 0, "failed to select System Information"}`
- `{TUI_STEP_SEND, "i", 0, 0, 0, 0, "failed to select Input Dialog"}`
- `{TUI_STEP_SEND, "p", 0, 0, 0, 0, "failed to select Progress Pattern"}`
- `{TUI_STEP_SEND, "l", 0, 0, 0, 0, "failed to select Layout Pattern"}`
- `{TUI_STEP_SEND, "x", 0, 0, 0, 0, "failed to select Exit"}`

The `vt_select_menu_index` helper and `TUI_STEP_SELECT` enum value can then be removed.

- [ ] **Step 3: Run terminal tests**

Run: `zig build -Denable-tui=true terminal-test`
Expected: PASS.

- [ ] **Step 4: Commit**

```bash
git add test/terminal_vt_scenarios.c
git commit -m "test(tui): drive menu selection by mnemonic instead of j-count

Position-independent and exercises the new mnemonic feature in the
happy path.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 21: VT scenario — incremental search narrows and confirms

**Files:**
- Modify: `test/terminal_vt_scenarios.c`

- [ ] **Step 1: Append a new scenario function**

After `run_tui_fuzz_smoke` in `test/terminal_vt_scenarios.c`, add:

```c
int run_tui_menu_search(test_stats_t *stats, const char *binary,
                        bool tui_enabled) {
  const char *name = "tui menu search filter narrows and confirms";
  if (!tui_enabled) { test_skip(stats, name, "rebuild with -Denable-tui=true"); return 0; }
  const char *args[] = {"menu"};
  vt_session_t session;
  if (!vt_session_start(&session, binary, args, 1, 80, 24)) {
    return test_fail(stats, name, "failed to start PTY session");
  }
  char *snapshot = NULL;
  int failed = 0;
  if (!vt_expect_text(&session, "Starter Showcase", PTY_TIMEOUT_MS, &snapshot))
    failed = test_fail(stats, name, "initial menu did not render");
  if (!failed && !vt_send(&session, "/"))
    failed = test_fail(stats, name, "failed to enter search mode");
  if (!failed && !vt_expect_text(&session, "Search:", PTY_TIMEOUT_MS, &snapshot))
    failed = test_fail(stats, name, "search prompt did not appear");
  if (!failed && !vt_send(&session, "prog"))
    failed = test_fail(stats, name, "failed to type 'prog'");
  if (!failed && !vt_expect_text(&session, "Progress Pattern", PTY_TIMEOUT_MS, &snapshot))
    failed = test_fail(stats, name, "Progress Pattern not filtered in");
  if (!failed && !vt_send(&session, "\r"))
    failed = test_fail(stats, name, "failed to confirm");
  if (!failed && !vt_expect_text(&session, "Progress Complete", PTY_TIMEOUT_MS, &snapshot))
    failed = test_fail(stats, name, "Progress dialog did not appear");
  if (!failed && !vt_send(&session, "x"))
    failed = test_fail(stats, name, "failed to dismiss progress");
  if (!failed && !vt_send(&session, "q"))
    failed = test_fail(stats, name, "failed to start exit");
  if (!failed && !vt_expect_text(&session, "Return to the shell?", PTY_TIMEOUT_MS, &snapshot))
    failed = test_fail(stats, name, "exit confirm did not appear");
  if (!failed && !vt_send(&session, "y"))
    failed = test_fail(stats, name, "failed to confirm exit");
  if (!failed && vt_wait_for_exit(&session, PTY_TIMEOUT_MS) != 0)
    failed = test_fail(stats, name, "process did not exit cleanly");
  if (!failed) test_pass(stats, name);
  free(snapshot);
  vt_session_close(&session);
  return failed;
}
```

- [ ] **Step 2: Wire `run_tui_menu_search` into the runner**

Find `test/terminal_vt_runner.c` (the scenario dispatcher). Add a call site for `run_tui_menu_search(...)` next to the existing scenario calls, and add its prototype to `test/terminal_vt.h`.

In `test/terminal_vt.h`, add:

```c
int run_tui_menu_search(test_stats_t *stats, const char *binary,
                        bool tui_enabled);
```

In `test/terminal_vt_runner.c`, find the existing block that calls `run_tui_menu_test(...)` and `run_tui_fuzz_smoke(...)` and add:

```c
run_tui_menu_search(&stats, binary_path, tui_enabled);
```

- [ ] **Step 3: Run**

Run: `zig build -Denable-tui=true terminal-test`
Expected: PASS.

- [ ] **Step 4: Commit**

```bash
git add test/terminal_vt_scenarios.c test/terminal_vt_runner.c test/terminal_vt.h
git commit -m "test(tui): VT scenario for menu search filter

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 22: VT scenario — mnemonic auto-confirm

**Files:**
- Modify: `test/terminal_vt_scenarios.c`
- Modify: `test/terminal_vt_runner.c`
- Modify: `test/terminal_vt.h`

- [ ] **Step 1: Add the scenario in `terminal_vt_scenarios.c`**

```c
int run_tui_menu_mnemonic(test_stats_t *stats, const char *binary,
                          bool tui_enabled) {
  const char *name = "tui menu mnemonic auto-confirms";
  if (!tui_enabled) { test_skip(stats, name, "rebuild with -Denable-tui=true"); return 0; }
  const char *args[] = {"menu"};
  vt_session_t session;
  if (!vt_session_start(&session, binary, args, 1, 80, 24)) {
    return test_fail(stats, name, "failed to start PTY session");
  }
  char *snapshot = NULL;
  int failed = 0;
  if (!vt_expect_text(&session, "Starter Showcase", PTY_TIMEOUT_MS, &snapshot))
    failed = test_fail(stats, name, "initial menu did not render");
  /* "&System Information" has unique mnemonic 's' — auto-confirms. */
  if (!failed && !vt_send(&session, "s"))
    failed = test_fail(stats, name, "failed to send 's'");
  if (!failed && !vt_expect_text(&session, "System Information", PTY_TIMEOUT_MS, &snapshot))
    failed = test_fail(stats, name, "System Information dialog did not appear");
  if (!failed && !vt_expect_text(&session, "Application:", 1000, &snapshot))
    failed = test_fail(stats, name, "Application metadata missing");
  if (!failed && !vt_send(&session, "x"))
    failed = test_fail(stats, name, "failed to dismiss dialog");
  if (!failed && !vt_send(&session, "q"))
    failed = test_fail(stats, name, "failed to start exit");
  if (!failed && !vt_send(&session, "y"))
    failed = test_fail(stats, name, "failed to confirm exit");
  if (!failed && vt_wait_for_exit(&session, PTY_TIMEOUT_MS) != 0)
    failed = test_fail(stats, name, "process did not exit cleanly");
  if (!failed) test_pass(stats, name);
  free(snapshot);
  vt_session_close(&session);
  return failed;
}
```

- [ ] **Step 2: Wire into runner + header** (same pattern as Task 21).

- [ ] **Step 3: Run**

Run: `zig build -Denable-tui=true terminal-test`
Expected: PASS.

- [ ] **Step 4: Commit**

```bash
git add test/terminal_vt_scenarios.c test/terminal_vt_runner.c test/terminal_vt.h
git commit -m "test(tui): VT scenario for mnemonic auto-confirm

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 23: VT scenario — separator skipped by navigation

**Files:**
- Modify: `test/terminal_vt_scenarios.c`
- Modify: `test/terminal_vt_runner.c`
- Modify: `test/terminal_vt.h`

- [ ] **Step 1: Add the scenario**

```c
int run_tui_menu_separator(test_stats_t *stats, const char *binary,
                           bool tui_enabled) {
  const char *name = "tui menu navigation skips separator";
  if (!tui_enabled) { test_skip(stats, name, "rebuild with -Denable-tui=true"); return 0; }
  const char *args[] = {"menu"};
  vt_session_t session;
  if (!vt_session_start(&session, binary, args, 1, 80, 24)) {
    return test_fail(stats, name, "failed to start PTY session");
  }
  char *snapshot = NULL;
  int failed = 0;
  if (!vt_expect_text(&session, "Starter Showcase", PTY_TIMEOUT_MS, &snapshot))
    failed = test_fail(stats, name, "initial menu did not render");
  /* Selection starts on Overview. j j j should advance past the separator
   * to Progress Pattern (item 4). */
  if (!failed && !vt_send(&session, "jjj"))
    failed = test_fail(stats, name, "failed to send navigation");
  if (!failed && !vt_send(&session, "\r"))
    failed = test_fail(stats, name, "failed to confirm");
  if (!failed && !vt_expect_text(&session, "Progress Complete", PTY_TIMEOUT_MS, &snapshot))
    failed = test_fail(stats, name, "expected to land on Progress Pattern");
  if (!failed && !vt_send(&session, "x"))
    failed = test_fail(stats, name, "failed to dismiss dialog");
  if (!failed && !vt_send(&session, "q") || !vt_send(&session, "y"))
    failed = test_fail(stats, name, "failed to exit cleanly");
  if (!failed && vt_wait_for_exit(&session, PTY_TIMEOUT_MS) != 0)
    failed = test_fail(stats, name, "process did not exit cleanly");
  if (!failed) test_pass(stats, name);
  free(snapshot);
  vt_session_close(&session);
  return failed;
}
```

- [ ] **Step 2: Wire + run + commit**

Run: `zig build -Denable-tui=true terminal-test`
Expected: PASS.

```bash
git add test/terminal_vt_scenarios.c test/terminal_vt_runner.c test/terminal_vt.h
git commit -m "test(tui): VT scenario for separator skip in navigation

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 24: VT scenario — shrink-then-grow resize keeps menu centered

**Files:**
- Modify: `test/terminal_vt_scenarios.c`
- Modify: `test/terminal_vt_runner.c`
- Modify: `test/terminal_vt.h`

- [ ] **Step 1: Add the scenario**

```c
int run_tui_menu_resize(test_stats_t *stats, const char *binary,
                        bool tui_enabled) {
  const char *name = "tui menu survives shrink-then-grow resize";
  if (!tui_enabled) { test_skip(stats, name, "rebuild with -Denable-tui=true"); return 0; }
  const char *args[] = {"menu"};
  vt_session_t session;
  if (!vt_session_start(&session, binary, args, 1, 100, 30)) {
    return test_fail(stats, name, "failed to start PTY session");
  }
  char *snapshot = NULL;
  int failed = 0;
  if (!vt_expect_text(&session, "Starter Showcase", PTY_TIMEOUT_MS, &snapshot))
    failed = test_fail(stats, name, "initial menu did not render");
  /* Shrink: above minimum but smaller than initial frame_width=72. */
  if (!failed && !vt_resize(&session, 60, 16))
    failed = test_fail(stats, name, "failed to shrink");
  if (!failed && !vt_expect_text(&session, "Starter Showcase", PTY_TIMEOUT_MS, &snapshot))
    failed = test_fail(stats, name, "title vanished after shrink");
  /* Grow back. */
  if (!failed && !vt_resize(&session, 100, 30))
    failed = test_fail(stats, name, "failed to grow");
  if (!failed && !vt_expect_text(&session, "Starter Showcase", PTY_TIMEOUT_MS, &snapshot))
    failed = test_fail(stats, name, "title vanished after grow");
  if (!failed && !vt_send(&session, "q"))
    failed = test_fail(stats, name, "failed to start exit");
  if (!failed && !vt_send(&session, "y"))
    failed = test_fail(stats, name, "failed to confirm exit");
  if (!failed && vt_wait_for_exit(&session, PTY_TIMEOUT_MS) != 0)
    failed = test_fail(stats, name, "process did not exit cleanly");
  if (!failed) test_pass(stats, name);
  free(snapshot);
  vt_session_close(&session);
  return failed;
}
```

- [ ] **Step 2: Wire + run + commit**

Run: `zig build -Denable-tui=true terminal-test`
Expected: PASS.

```bash
git add test/terminal_vt_scenarios.c test/terminal_vt_runner.c test/terminal_vt.h
git commit -m "test(tui): VT scenario for shrink-then-grow resize

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 25: VT scenario — SIGINT cleanly exits the menu

**Files:**
- Modify: `test/terminal_vt_scenarios.c`
- Modify: `test/terminal_vt_runner.c`
- Modify: `test/terminal_vt.h`

- [ ] **Step 1: Add the scenario**

```c
int run_tui_menu_sigint(test_stats_t *stats, const char *binary,
                        bool tui_enabled) {
  const char *name = "tui menu SIGINT cleanly exits";
  if (!tui_enabled) { test_skip(stats, name, "rebuild with -Denable-tui=true"); return 0; }
  const char *args[] = {"menu"};
  vt_session_t session;
  if (!vt_session_start(&session, binary, args, 1, 80, 24)) {
    return test_fail(stats, name, "failed to start PTY session");
  }
  char *snapshot = NULL;
  int failed = 0;
  if (!vt_expect_text(&session, "Starter Showcase", PTY_TIMEOUT_MS, &snapshot))
    failed = test_fail(stats, name, "initial menu did not render");
  /* Send Ctrl-C through the PTY. The shell/terminal converts \x03 to SIGINT. */
  if (!failed && !vt_send(&session, "\x03"))
    failed = test_fail(stats, name, "failed to send Ctrl-C");
  /* Process must exit within PTY_TIMEOUT_MS. We don't pin the exit code
   * because the channel goes through APP_ERROR_OUT_OF_RANGE only when
   * the menu also reports TUI_MENU_INTERRUPTED — the starter currently
   * stops the loop and returns APP_SUCCESS in that case. */
  if (!failed) {
    const int code = vt_wait_for_exit(&session, PTY_TIMEOUT_MS);
    if (code < 0) failed = test_fail(stats, name, "process did not exit within timeout");
  }
  if (!failed) test_pass(stats, name);
  free(snapshot);
  vt_session_close(&session);
  return failed;
}
```

- [ ] **Step 2: Wire + run + commit**

Run: `zig build -Denable-tui=true terminal-test`
Expected: PASS.

```bash
git add test/terminal_vt_scenarios.c test/terminal_vt_runner.c test/terminal_vt.h
git commit -m "test(tui): VT scenario for SIGINT clean exit

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 26: Manual real-terminal verification + final cleanup

**Files:** none (verification pass)

- [ ] **Step 1: Build with TUI**

Run: `zig build -Denable-tui=true`
Expected: clean build.

- [ ] **Step 2: Run the menu in a real terminal**

Run: `./zig-out/bin/<app-name> menu` (replace with the actual binary name from `build.zig`).

Verify in a real terminal:
- Startup and clean exit (Ctrl-C, then `q`+`y`) restore the shell's echo and cursor.
- Arrow + j/k navigation, Home/End, PgUp/PgDn all behave per spec.
- `&Mnemonic`s underline; `o` opens Overview; `s` opens System Information; `x` opens exit confirm.
- `/` enters search; typing `prog` filters to "Progress Pattern"; Enter confirms; Esc cancels.
- Mouse click on an item selects it; double-click confirms; wheel scrolls; click on a separator beeps.
- Resize the terminal smaller (down to 50×14) and larger (100×30); the menu stays centered and the title remains visible.
- Inside the Configuration sub-menu, "Export config" appears dimmed with `(disabled)`; pressing `e` (its mnemonic) beeps but does not enter.
- The Detail pane below the items updates as selection changes.

- [ ] **Step 3: Run the full test suite one more time**

```bash
zig build test
zig build -Denable-tui=true terminal-test
```
Expected: all pass.

- [ ] **Step 4: Final commit (if any cleanup was needed)**

If the manual pass revealed cosmetic issues (off-by-one in a margin, color choice, etc.), commit those fixes in a single commit:

```bash
git add -p
git commit -m "fix(tui): minor real-terminal cleanups

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

- [ ] **Step 5: Push branch / open PR**

Plan-execution gate: when all tasks are complete and verification passes, hand back to the user to open the PR. Do not auto-merge.

---

## Self-review log

**Spec coverage**: each spec section maps to tasks above:
- Layout (Section 1) → Tasks 11–14
- API surface (Section 2) → Tasks 1–2, 16
- Internal architecture (Section 3) → Tasks 3–9 (model), 10–15 (view + controller)
- Feature semantics (Section 4) → search (8), mnemonics (6–7), mouse (15), numeric (9, 13), disabled/separator (4, 13)
- Lifecycle hooks (Section 5) → 19 (sigaction), 16 (loop integration + KEY_RESIZE)
- Starter shell (Section 6) → 18 (rename + restructure + adjacent files + exit-status channel)
- VT test migration → 20–25
- Manual real-terminal pass → 26

**No placeholders**: each step shows the code or command. No "TBD" / "add error handling" / "similar to Task N".

**Type consistency check**: `tui_menu_state_t` is opaque in `tui_menu_internal.h` and concrete in `tui_menu_model.c`; accessors are consistent; `tui_menu_state_step / page / home / end / search_open / search_close / search_append / search_backspace / mnemonic_jump / numeric_jump / visible_at / config` are referenced consistently across tasks.
