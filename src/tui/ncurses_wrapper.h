/*
 * NCurses wrapper for the application.
 *
 * Provides a clean interface to ncurses functionality with proper
 * initialization, cleanup, and error handling. This wrapper makes it
 * easier to create interactive terminal UIs while maintaining the
 * application's error handling patterns.
 */

#pragma once

#include <ncurses.h>
#include <stdbool.h>

#include "../core/error.h"
#include "../core/types.h"

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

// Initialize ncurses with our standard settings
APP_NODISCARD app_error tui_init(void);

// Cleanup ncurses properly
void tui_cleanup(void);

// Check if TUI is initialized
bool tui_is_initialized(void);

// Color management
APP_NODISCARD app_error tui_init_colors(void);
void tui_set_color(WINDOW *win, tui_color_pair_t color);

// Window management
APP_NODISCARD tui_window_t *tui_create_window(int height, int width, int y,
                                              int x);
void tui_destroy_window(tui_window_t *window);
void tui_draw_border(tui_window_t *window);
void tui_set_window_title(tui_window_t *window, const char *title);
void tui_refresh_window(tui_window_t *window);
void tui_clear_window(tui_window_t *window);

// Text output helpers
void tui_print_centered(WINDOW *win, int y, const char *text);
void tui_print_wrapped(WINDOW *win, int y, int x, int width, const char *text);

// Input helpers
APP_NODISCARD int tui_get_char(void);
APP_NODISCARD app_error tui_get_string(WINDOW *win, char *buffer, size_t size,
                                       const char *prompt);

// Menu system
APP_NODISCARD int tui_show_menu(tui_window_t *window, const char *title,
                                tui_menu_item_t *items, int item_count,
                                int default_selection);

// Common dialogs
void tui_show_message(const char *title, const char *message);
bool tui_confirm(const char *title, const char *question);
APP_NODISCARD app_error tui_input_dialog(const char *title, const char *prompt,
                                         char *buffer, size_t size);

// Progress indicators
typedef struct tui_progress tui_progress_t;
APP_NODISCARD tui_progress_t *tui_progress_create(const char *title, int max);
void tui_progress_update(tui_progress_t *progress, int current,
                         const char *status);
void tui_progress_destroy(tui_progress_t *progress);

// Utility functions
void tui_beep(void);
void tui_flash(void);
int tui_get_max_x(void);
int tui_get_max_y(void);