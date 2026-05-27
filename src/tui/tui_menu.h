/*
 * Public menu API for the ncurses TUI layer.
 */

#pragma once

#include <stdbool.h>

#include "../core/types.h"

typedef struct tui_window tui_window_t;

typedef enum {
  TUI_MENU_ITEM_NORMAL = 0,
  TUI_MENU_ITEM_SEPARATOR,
} tui_menu_item_kind_t;

typedef struct {
  const char *label;
  const char *description;
  int id;
  bool disabled;
  tui_menu_item_kind_t kind;
} tui_menu_item_t;

typedef struct {
  const char *title;
  const tui_menu_item_t *items;
  int item_count;
  int default_index;
  int frame_height;
  int frame_width;
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

/// All pointers reachable from `config` must remain valid until this returns.
/// The menu makes no copies of caller-owned labels, descriptions, or titles.
APP_NODISCARD tui_menu_result_t tui_show_menu(tui_window_t *window,
                                              const tui_menu_config_t *config);
