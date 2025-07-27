/*
 * Terminal UI (TUI) public API using ncurses.
 */

#pragma once

#include <ncurses.h>
#include <stdbool.h>
#include <stddef.h>

#include "../core/error.h"
#include "../core/types.h"

// Forward declaration for progress API
struct tui_progress;

typedef struct tui_progress tui_progress_t;

// TUI color pairs
typedef enum {
  TUI_COLOR_DEFAULT = 0,
  TUI_COLOR_HIGHLIGHT,
  TUI_COLOR_ERROR,
  TUI_COLOR_SUCCESS,
  TUI_COLOR_WARNING,
  TUI_COLOR_INFO,
  TUI_COLOR_MENU_SELECTED,
  TUI_COLOR_MENU_NORMAL,
  TUI_COLOR_BORDER,
  TUI_COLOR_TITLE,
  TUI_COLOR_MAX
} tui_color_pair_t;

// Window wrapper for safer window management
typedef struct {
  WINDOW *win;
  int height;
  int width;
  int y;
  int x;
  bool has_border;
  char *title;
} tui_window_t;

// Menu item structure
typedef struct {
  char *label;
  char *description;
  int id;
  bool enabled;
} tui_menu_item_t;

// Initialization / cleanup
APP_NODISCARD app_error tui_init(void);
void tui_cleanup(void);
bool tui_is_initialized(void);

// Color management
APP_NODISCARD app_error tui_init_colors(void);
void tui_set_color(WINDOW *win, tui_color_pair_t color);

// Window helpers
APP_NODISCARD tui_window_t *tui_create_window(int height, int width, int y,
                                              int x);
void tui_destroy_window(tui_window_t *window);
void tui_draw_border(tui_window_t *window);
void tui_set_window_title(tui_window_t *window, const char *title);
void tui_refresh_window(tui_window_t *window);
void tui_clear_window(tui_window_t *window);

// Text helpers
void tui_print_centered(WINDOW *win, int y, const char *text);
void tui_print_wrapped(WINDOW *win, int y, int x, int width, const char *text);

// Input helpers
APP_NODISCARD int tui_get_char(void);
APP_NODISCARD app_error tui_get_string(WINDOW *win, char *buffer, size_t size,
                                       const char *prompt);

// Menu
APP_NODISCARD int tui_show_menu(tui_window_t *window, const char *title,
                                tui_menu_item_t *items, int item_count,
                                int default_selection);

// Dialogs
void tui_show_message(const char *title, const char *message);
bool tui_confirm(const char *title, const char *question);
APP_NODISCARD app_error tui_input_dialog(const char *title, const char *prompt,
                                         char *buffer, size_t size);

// Utilities
void tui_beep(void);
void tui_flash(void);
int tui_get_max_x(void);
int tui_get_max_y(void);

// Progress API is declared in a dedicated header to keep tui.h lean.
#include "tui_progress.h"
