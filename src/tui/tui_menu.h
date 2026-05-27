/*
 * tui_menu.h - reference-grade ncurses menu component.
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
  const char *label; /* NUL-terminated UTF-8; "&x" marks mnemonic 'x'; "&&" =>
                        literal '&' */
  const char *description; /* optional UTF-8; shown in the detail pane */
  int id;                  /* returned via result.selected_id when chosen */
  bool disabled;           /* zero-init => ENABLED. Polarity is intentional. */
  tui_menu_item_kind_t kind;
} tui_menu_item_t;

typedef struct {
  const char *title;
  const tui_menu_item_t *items;
  int item_count;
  int default_index; /* -1 picks first enabled */
  int frame_height;  /* used when window == NULL */
  int frame_width;   /* used when window == NULL */
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
 * If window != NULL, the menu draws into and recenters the caller's window on
 * KEY_RESIZE. It returns TUI_MENU_TOO_SMALL when the resized terminal cannot
 * host the requested frame dimensions. */
APP_NODISCARD tui_menu_result_t tui_show_menu(tui_window_t *window,
                                              const tui_menu_config_t *config);
