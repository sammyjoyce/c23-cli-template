# TUI menu — reference-level redesign

- **Status:** approved (brainstorm)
- **Date:** 2026-05-27
- **Owner:** Sam Joyce
- **Scope:** `src/tui/` and the starter shell consumers of this template inherit
- **Skill grounding:** `.claude/skills/ncurses` (authoritative for ncurses behavior); `ritchie-c-mastery` review applied

## Why

`tui_show_menu` in `src/tui/tui.c` is the most prominent reference surface in this
template — every consumer of the template clones the menu file as their app's
main menu. Today it is a 220-line monolith that mixes layout, rendering, input
dispatch, scrolling, and accelerator handling. It also has real defects that a
"reference-level" implementation cannot ship with:

- Signal handlers are installed in the source but never wired into `tui_init`,
  so `tui_interrupted_flag` is dead code.
- Descriptions force every item to occupy 2 rows even when absent; the selected
  highlight only covers the label row, leaving description rows half-painted.
- Scroll arrows steal the first and last visible item rows.
- `KEY_RESIZE` only touches `stdscr`; the menu frame is not recentered.
- The return value overloads `-1` for both "user quit" and "no enabled items".
- Number/letter accelerators change selection but cannot confirm.
- No mouse, no separators, no incremental search, no mnemonics.
- `wclear` per keystroke triggers full-screen repaint flicker.
- Selected-row highlight glyph `"▶ "` is written with `mvwaddnstr(... 2)` —
  truncates the 3-byte UTF-8 glyph to 2 bytes and emits broken UTF-8.

This document specifies the redesign. The implementation plan that follows lives
in `docs/superpowers/plans/`.

## Goals

1. Idiomatic ncurses: locale + ncursesw wide-char rendering, `wnoutrefresh` /
   `doupdate` flicker-free frames, ACS line drawing, careful color-pair pairing,
   `ERR` handling, signal-flag plumbing.
2. Clear model / view / controller split. Model unit-testable without a TTY.
3. Richer menu features without leaving "reference" territory: separators,
   mnemonics (`&Label` syntax), incremental search, mouse, scroll indicators in
   the right column, and a dedicated detail pane for descriptions.
4. The starter shell (today `tui_demo.c`, renamed to `tui_app.c`) reads as a
   *starter* — clearly labeled sections for "edit these" vs "keep these",
   with the canonical resize-safe main loop.

## Non-goals

- Reusable, embeddable, stateful menu controller (`tui_menu_t *` with builder
  API). Considered and rejected for this pass — the config-struct API gives
  90% of the benefit with a much smaller surface.
- Mouse coverage in the VT test suite. Deferred until `terminal_vt.h` grows
  mouse-event injection (separate uplift). Mouse code is still gated behind
  `NCURSES_MOUSE_VERSION` so it compiles everywhere.
- Async/non-blocking menu (step-by-event). The blocking `tui_show_menu(...)`
  call is the right shape for this template's use cases.

## Layout

```
╭─ Starter Showcase ────────────────────────────────────╮
│                                                       │
│  1  ▸ Overview                                      ▲ │
│  2    System Information                              │
│  3    Input Dialog                                    │
│  ───────────────────────────────────────────────────  │
│  4    Progress Pattern                                │
│  5    Layout Pattern                                  │
│  6    Configuration                                   │
│  7    Quick Actions                          (disabled)│
│  8    Exit                                          ▼ │
│                                                       │
│ ╭─ Detail ─────────────────────────────────────────╮  │
│ │ See the starter's CLI, TUI foundation, and core  │  │
│ │ architecture.                                    │  │
│ ╰──────────────────────────────────────────────────╯  │
│                                                       │
│  ↑↓ navigate   / search   1-9 jump   Enter ok   q quit│
╰───────────────────────────────────────────────────────╯
```

Rules:

- One row per item. The detail pane renders the selected item's description
  with word-wrap (2–3 lines).
- Scroll indicators in the right-most interior column (`▲`/`▼`) never steal
  item rows.
- Separators are non-selectable horizontal rules drawn with `ACS_HLINE`;
  navigation skips them with no beep.
- Disabled items dim (`A_DIM`) with a trailing `(disabled)` hint in
  `TUI_COLOR_DIM`; navigation skips them, direct selection beeps.
- Selected line uses `▸` + full-row inverse background. The detail row is in
  the dedicated pane below the items, so no half-painted highlight.
- Title rendered in the border at top. Footer in the last interior row uses
  `TUI_COLOR_INFO` (not the harsh `TUI_COLOR_HIGHLIGHT`).
- Search mode swaps the footer to `Search: <query>_     Esc cancel` and
  filters live (jump-narrow: matches highlighted, non-matches dimmed in place
  so spatial position stays stable; matched chars get `A_UNDERLINE`).
- Small-terminal fallback: if the window cannot fit at least 3 item rows +
  1-line detail + footer, the detail pane collapses; if still too small,
  returns `TUI_MENU_TOO_SMALL`.

## API surface

```c
// src/tui/tui_menu.h (new; re-exported from tui.h)

typedef enum {
  TUI_MENU_ITEM_NORMAL = 0,
  TUI_MENU_ITEM_SEPARATOR,        // visual rule; navigation skips
} tui_menu_item_kind_t;

typedef struct {
  const char           *label;        // UTF-8; "&x" marks mnemonic 'x'; "&&" => literal '&'
  const char           *description;  // optional UTF-8; shown in detail pane
  int                   id;
  bool                  disabled;     // zero-init = ENABLED (polarity intentional)
  tui_menu_item_kind_t  kind;         // defaults to NORMAL
} tui_menu_item_t;

typedef struct {
  const char            *title;
  const tui_menu_item_t *items;
  int                    item_count;
  int                    default_index;     // -1 picks first enabled
  int                    frame_height;      // used when window == NULL
  int                    frame_width;       // used when window == NULL
  bool                   enable_search;     // '/' incremental filter
  bool                   enable_mouse;      // BUTTON1 click selects, double-click confirms
  bool                   show_detail_pane;  // detail pane at bottom
  bool                   show_numeric_keys; // 1-9 jump (no auto-confirm)
} tui_menu_config_t;

typedef enum {
  TUI_MENU_OK = 0,            // selected_id / selected_index valid
  TUI_MENU_CANCELLED,         // q / Esc
  TUI_MENU_INTERRUPTED,       // SIGINT received
  TUI_MENU_TOO_SMALL,         // terminal cannot host the menu
  TUI_MENU_INVALID_ARG,       // null items, zero count, overflow
} tui_menu_status_t;

typedef struct {
  tui_menu_status_t status;
  int               selected_id;
  int               selected_index;
} tui_menu_result_t;

/// All pointers reachable from `config` (including `config->title`,
/// `items[i].label`, and `items[i].description`) must remain valid until
/// `tui_show_menu` returns. The menu makes no copies.
///
/// If `window == NULL`, the menu creates and owns a centered frame sized
/// `config->frame_height` x `config->frame_width` (clamped to terminal
/// bounds) and recreates it in place on `KEY_RESIZE`.
///
/// If `window != NULL`, the menu draws into the caller's window. On a
/// resize that makes the window unworkable, the menu returns
/// `TUI_MENU_TOO_SMALL` and the caller owns recovery.
APP_NODISCARD tui_menu_result_t tui_show_menu(tui_window_t *window /* nullable */,
                                              const tui_menu_config_t *config);
```

The old `int`-returning `tui_show_menu(window, title, items, count, default)`
signature is **removed**, not aliased. There is no backwards-compatibility
shim in a starter template.

`tui_set_background_window` / `tui_clear_background_window` are also
removed from the public API. The background hook becomes an internal helper
that `tui_show_menu` installs on entry and clears on exit, so modal dialogs
(`tui_show_message`, `tui_confirm`, `tui_input_dialog`) still redraw the
menu frame on close.

## Internal architecture

### Files

```
src/tui/
  tui.h               public umbrella header; re-exports tui_menu.h
  tui.c               (existing scope: lifecycle, windows, dialogs, colors, helpers)
  tui_menu.h          (NEW) public menu types + tui_show_menu prototype
  tui_menu.c          (NEW) menu implementation
  tui_progress.h/.c   (unchanged)
  tui_app.c           (renamed from tui_demo.c)
```

### Model / view / controller split inside `tui_menu.c`

#### Model
Pure data + filter math. No ncurses calls. Unit-testable without a TTY.

```c
typedef struct {
  const tui_menu_config_t *cfg;
  wchar_t **label_w_cache;   // per-item wchar buffers, mnemonic '&' stripped
  wchar_t  *mnemonics;       // case-folded mnemonic char per item, 0 for none
  int      *visible_indices; // items[] indices currently visible (filtered)
  int       visible_count;
  int       selected_visible;
  int       top_visible;
  wchar_t   search_buf_w[64];
  size_t    search_len;
  bool      search_active;
} tui_menu_state_t;

static tui_menu_status_t tui_menu_state_init   (tui_menu_state_t *s,
                                                const tui_menu_config_t *cfg);
static void              tui_menu_state_free   (tui_menu_state_t *s);
static void              tui_menu_apply_filter (tui_menu_state_t *s);
static void              tui_menu_step_select  (tui_menu_state_t *s, int direction);
static bool              tui_menu_jump_mnemonic(tui_menu_state_t *s, wchar_t key,
                                                bool *out_unique);
```

#### View
Reads `const` model + layout. Each render call uses `wnoutrefresh`; controller
calls `doupdate()` exactly once per frame.

```c
typedef struct {
  tui_window_t *frame;
  bool          owns_frame;  // true when window == NULL was passed
  int           item_area_y, item_area_h;
  int           detail_y,    detail_h;
  int           footer_y;
  int           inner_w;
  bool          detail_pane_visible;
} tui_menu_layout_t;

static bool tui_menu_layout_compute  (tui_menu_layout_t *L, const tui_window_t *w,
                                      const tui_menu_config_t *cfg);
static void tui_menu_render_title    (const tui_menu_layout_t *L, const char *title);
static void tui_menu_render_items    (const tui_menu_layout_t *L,
                                      const tui_menu_state_t *s);
static void tui_menu_render_scrollbar(const tui_menu_layout_t *L,
                                      const tui_menu_state_t *s);
static void tui_menu_render_detail   (const tui_menu_layout_t *L,
                                      const tui_menu_state_t *s);
static void tui_menu_render_footer   (const tui_menu_layout_t *L,
                                      const tui_menu_state_t *s);

// Wide-string write helper used by all renderers — column-correct via wcwidth.
static void tui_menu_write_wcs(WINDOW *w, int y, int x, int cols,
                               const wchar_t *s);
```

#### Controller
Dispatch only.

```c
typedef enum {
  TUI_MENU_EV_NONE,
  TUI_MENU_EV_CONFIRM,
  TUI_MENU_EV_CANCEL,
  TUI_MENU_EV_INTERRUPT,
  TUI_MENU_EV_REDRAW,
} tui_menu_event_t;

static tui_menu_event_t tui_menu_handle_key  (tui_menu_state_t *s, int ch);
static tui_menu_event_t tui_menu_handle_mouse(tui_menu_state_t *s,
                                              const tui_menu_layout_t *L);
```

### Frame flow (single loop iteration)

1. `tui_menu_layout_compute(...)` — cheap, runs every iteration; survives resize.
2. `werase(frame->win)` — not `wclear`; no full-screen repaint.
3. `tui_draw_border` + `render_title`.
4. `render_items` + `render_scrollbar`.
5. `render_detail` (if `detail_pane_visible`).
6. `render_footer` (search prompt or hint).
7. `wnoutrefresh(frame->win)`; `doupdate()`.
8. `ch = wgetch(frame->win)`.
9. Dispatch:
   - `KEY_MOUSE` → `handle_mouse` → event.
   - `KEY_RESIZE` → event = `REDRAW`; if `owns_frame`, destroy + recreate frame.
   - `ERR` + `tui_interrupted()` → event = `INTERRUPT`.
   - `ERR` + no flag → `continue` (spurious; redraw next iter).
   - Otherwise → `handle_key` → event.
10. If event in `{CONFIRM, CANCEL, INTERRUPT}` → exit loop with result.

### Memory and ownership

- `state.visible_indices`: one `int[item_count]` allocation, reused on filter
  changes. Overflow-checked: `if ((size_t)item_count > SIZE_MAX / sizeof(int))
  return TUI_MENU_INVALID_ARG;` then allocate.
- `state.label_w_cache`: one `wchar_t *[item_count]` array + per-item
  `wchar_t[label_bytes + 1]` (UTF-8 → wchar via `mbstowcs` after `setlocale`).
  Allocated once at init, freed in `tui_menu_state_free`. No allocations in
  the render path.
- `state.search_buf_w`: fixed `wchar_t[64]`, bounds-checked append.
- `state.mnemonics`: one `wchar_t[item_count]`.
- Caller-owned frame is never destroyed by the menu (only menu-owned frames
  are).

## Feature semantics

### Incremental search (`enable_search`)
- `/` enters search mode; footer becomes `Search: <query>_     Esc cancel`.
- Case-insensitive substring match on `label` (description is not searched).
- Non-matches stay in position, dimmed; matched chars get `A_UNDERLINE`.
- Backspace deletes one `wchar_t` (not one byte).
- `Esc` clears filter and exits search mode. `Enter` confirms selection.
- Selection auto-snaps to first match on each keystroke; "no matches" footer
  shows `(no matches)`.
- Buffer is `wchar_t search_buf_w[64]`.

### Mnemonics (`&Label` syntax)
- Parser scans `label` for `&X` where X is alphanumeric; first occurrence is
  the mnemonic. `&&` renders as a literal `&`.
- Init precomputes `state.mnemonics[i]` (case-folded), and `label_w_cache[i]`
  has the `&` stripped.
- Render strips `&` and applies `A_UNDERLINE` to the next character.
- Pressing a mnemonic key (case-insensitive) auto-confirms if exactly one
  selectable item has that mnemonic. If multiple match (caller bug, tolerated),
  the key cycles selection through them and beeps once. Disabled-but-matching:
  beep, no movement.

### Mouse (`enable_mouse`)
- Gated by `#ifdef NCURSES_MOUSE_VERSION`. Absent: `enable_mouse = true` is
  silently ignored.
- On entry: save prior `mousemask()`, set
  `BUTTON1_CLICKED | BUTTON1_DOUBLE_CLICKED | BUTTON4_PRESSED | BUTTON5_PRESSED`.
  Restored on every exit path.
- `KEY_MOUSE` → `getmouse(&ev)`; translate via `wmouse_trafo` to window-local
  coords. Hit-test against item area:
  - Single click on enabled item → select (no confirm).
  - Double click on selected enabled item → confirm.
  - Click on separator or disabled row → beep.
  - Wheel up/down → step selection by one (skipping separators).
- Clicks outside item area are ignored.

### Numeric accelerators (`show_numeric_keys`)
- `1`–`9` jump to visible index `0`–`8` (within the current filter view).
- Selects only; does not auto-confirm — preserves existing demo behavior and
  lets the user read the detail pane before pressing Enter.
- Letters `a`–`z` are reserved for mnemonics; the old "positional letters
  after 9" behavior is dropped.

### Disabled / separator behavior
- Disabled: `A_DIM` + trailing `  (disabled)`; skipped by `j`/`k`/`↑`/`↓`/
  `PgUp`/`PgDn`/`Home`/`End`; beep on direct selection attempt.
- Separators: `ACS_HLINE` across inner width in `TUI_COLOR_BORDER`; skipped
  silently by navigation (layout element, not a failed action).

### Result-status table

| Exit cause                                | Status                    |
|-------------------------------------------|---------------------------|
| Enter on enabled item                     | `TUI_MENU_OK`             |
| Mnemonic auto-confirm                     | `TUI_MENU_OK`             |
| Double-click on selected enabled item     | `TUI_MENU_OK`             |
| `q` or `Esc` outside search mode          | `TUI_MENU_CANCELLED`      |
| `Esc` inside search mode                  | (no exit; clears filter)  |
| SIGINT received                           | `TUI_MENU_INTERRUPTED`    |
| Window too small at entry or after resize | `TUI_MENU_TOO_SMALL`      |
| NULL config/items, zero count, overflow   | `TUI_MENU_INVALID_ARG`    |

## Lifecycle hooks

### Signal wiring
The dead `tui_install_signal_handlers` and `tui_uninstall_signal_handlers`
are wired into `tui_init` (after `initscr()` succeeds) and `tui_cleanup`
(before `endwin()`), respectively. `signal()` is replaced with `sigaction()`
to disable `SA_RESTART` for `SIGINT` so that `wgetch` returns `ERR` and the
menu loop can observe the flag. Handler body:

```c
static void tui_signal_handler(int signum) {
  (void)signum;
  const int saved_errno = errno;
  tui_interrupted_flag = 1;
  errno = saved_errno;
}
```

`tui_interrupted_flag` remains `volatile sig_atomic_t`.

### Menu loop integration
After every `wgetch`:

```c
int ch = wgetch(frame->win);
if (ch == ERR) {
  if (tui_interrupted()) {
    return (tui_menu_result_t){ .status = TUI_MENU_INTERRUPTED, ... };
  }
  continue;                    // spurious ERR; redraw next frame
}
```

### KEY_RESIZE handling
- `owns_frame == true` (`window == NULL` was passed): destroy current frame,
  reallocate centered frame using `cfg->frame_height` / `cfg->frame_width`
  clamped to current terminal bounds, recompute layout, redraw next frame.
  If even the clamped frame can't host the minimum layout, return
  `TUI_MENU_TOO_SMALL`.
- `owns_frame == false`: recompute layout against caller's existing window.
  If unworkable, return `TUI_MENU_TOO_SMALL` and let the caller handle.

### Clean-exit invariants
On every exit path (OK / CANCELLED / INTERRUPTED / TOO_SMALL / INVALID_ARG):
1. Restore prior `mousemask()` if mouse was enabled.
2. Free `state.visible_indices`, `state.label_w_cache`, `state.mnemonics`.
3. If `owns_frame`, destroy the frame.
4. Never call `endwin()` from inside the menu — that's `tui_cleanup`'s job.

## Starter shell (`tui_app.c`)

### Renames
| Before                       | After                  |
|------------------------------|------------------------|
| `src/tui/tui_demo.c`         | `src/tui/tui_app.c`    |
| `tui_run_demo()`             | `tui_run_app()`        |
| `static tui_demo_show_*()`   | `static app_show_*()`  |
| `build.zig:183` source path  | updated path           |
| `src/main.c:382` call site   | updated call           |
| `src/tui/tui.h` prototype    | updated prototype      |

The `app_*` naming signals **edit these** to the consumer; `tui_*` stays for
primitives the consumer keeps.

### File shape

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
```

Three clearly labeled regions:

```c
/* ============================================================
 * Section 1: Handlers — replace these with your app's actions.
 * (Bodies are unchanged from the existing tui_demo.c — they
 * remain useful worked examples of every TUI primitive.)
 * ============================================================ */
static void app_show_overview(void);
static void app_show_system_info(void);
static void app_show_input(void);
static void app_show_progress(void);
static void app_show_layout(void);
static void app_show_config_menu(void);

/* ============================================================
 * Section 2: Menu definition — your items + dispatch table.
 * Descriptions below are preserved verbatim from tui_demo.c.
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
    {.label = "&Quick Actions",
     .description = "Jump to common operations from a dedicated sub-menu",
     .id = 7, .disabled = true},
    {.kind = TUI_MENU_ITEM_SEPARATOR},
    {.label = "E&xit",
     .description = "Return to the shell",
     .id = 0},
};

static void app_dispatch(int id) {
  switch (id) {
    case 1: app_show_overview();    break;
    case 2: app_show_system_info(); break;
    case 3: app_show_input();       break;
    case 4: app_show_progress();    break;
    case 5: app_show_layout();      break;
    case 6: app_show_config_menu(); break;
    default: tui_beep(); break;
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

## VT test migration

`vt_select_menu_index` (which counts `j` keystrokes) is fragile with
separators and with menu reordering. Migration switches to **mnemonic-driven
selection** in `terminal_vt_scenarios.c` — `vt_send(session, "o")` opens
Overview, `vt_send(session, "x")` exits, etc. Position-independent and
exercises the new mnemonic feature in the happy path.

Existing assertions on visible text (`Starter Showcase`, `Overview`,
`System Information`, `Input Dialog`, `Progress Pattern`, `Layout Pattern`,
`Exit`, `Return to the shell?`) still match — the `&` mnemonic marker is
stripped at render, not displayed.

**New VT scenarios to add:**

1. `tui menu search filter narrows and confirms` — `/`, type `prog`, expect
   detail pane to update, `Enter` confirms Progress Pattern.
2. `tui menu mnemonic auto-confirms` — `s` auto-opens System Information.
3. `tui menu separator is skipped by navigation` — `j` from item 3 lands on
   item 4, not the separator row.
4. `tui menu survives shrink-then-grow resize` — 60×16 then 100×30; assert
   frame stays centered and title visible at each stop.
5. `tui menu SIGINT cleanly exits` — `\x03`, expect process exit within
   `PTY_TIMEOUT_MS`.

Mouse coverage is deferred — `terminal_vt.h` does not currently inject mouse
events. Mouse hit-test math is pure-int and unit-testable from the model.

## Open questions

None remaining at brainstorm exit.

## Verification

The implementation plan will verify in order:

1. Build clean with the project's standard warning + sanitizer ladder.
2. Existing VT tests (`run_tui_menu_test`, `run_tui_fuzz_smoke`) pass with
   updated `vt_select_menu_index` semantics.
3. New VT scenarios pass.
4. Manual real-terminal pass: startup/cleanup restore the shell, keys produce
   intended `KEY_*` paths, redraw works after cover/uncover and resize, color
   degrades when `has_colors()` is false, no writes outside the target window.
