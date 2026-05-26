/*
 * Terminal UI (TUI) implementation using ncurses.
 */

#include "tui.h"

#include <limits.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>

#include "../utils/logging.h"
#include "../utils/memory.h"

static bool tui_initialized = false;
static bool tui_default_colors = false;

static int tui_clamped_strlen(const char *text, int max_len) {
  if (text == nullptr || max_len <= 0) {
    return 0;
  }

  int len = 0;
  while (len < max_len && text[len] != '\0') {
    len++;
  }
  return len;
}

static void tui_write_clamped(WINDOW *win, int y, int x, int width,
                              const char *text) {
  if (!win || !text || width <= 0) {
    return;
  }

  const int max_y = getmaxy(win);
  const int max_x = getmaxx(win);
  if (y < 0 || y >= max_y || x >= max_x) {
    return;
  }
  if (x < 0) {
    x = 0;
  }
  if (width > max_x - x) {
    width = max_x - x;
  }
  if (width <= 0) {
    return;
  }

  mvwaddnstr(win, y, x, text, width);
}

static short tui_default_fg(void) {
  return tui_default_colors ? -1 : COLOR_WHITE;
}

static short tui_default_bg(void) {
  return tui_default_colors ? -1 : COLOR_BLACK;
}

app_error tui_init(void) {
  if (tui_initialized) {
    return APP_SUCCESS;
  }

  // Set locale for proper Unicode support
  setlocale(LC_ALL, "");

  // Initialize ncurses
  if (initscr() == NULL) {
    LOG_ERROR("Failed to initialize ncurses");
    return APP_ERROR_INTERNAL;
  }

  // Configure ncurses
  cbreak();              // Disable line buffering
  noecho();              // Don't echo input
  keypad(stdscr, TRUE);  // Enable special keys
  curs_set(0);           // Hide cursor by default

  // Initialize colors if supported
  if (has_colors()) {
    if (start_color() != ERR) {
      tui_default_colors = use_default_colors() != ERR;
      (void)tui_init_colors();
    }
  }

  // Clear and refresh
  clear();
  refresh();

  tui_initialized = true;
  LOG_DEBUG("TUI initialized successfully");
  return APP_SUCCESS;
}

void tui_cleanup(void) {
  if (!tui_initialized) {
    return;
  }

  // Reset terminal
  clear();
  refresh();
  endwin();

  tui_initialized = false;
  tui_default_colors = false;
  LOG_DEBUG("TUI cleaned up");
}

bool tui_is_initialized(void) {
  return tui_initialized;
}

app_error tui_init_colors(void) {
  if (!has_colors()) {
    LOG_WARNING("Terminal does not support colors");
    return APP_SUCCESS;  // Not an error, just no colors
  }

  const short default_fg = tui_default_fg();
  const short default_bg = tui_default_bg();

  // Pair 0 is the terminal default and cannot be redefined portably.
  init_pair(TUI_COLOR_HIGHLIGHT, COLOR_BLACK, COLOR_WHITE);
  init_pair(TUI_COLOR_ERROR, COLOR_RED, default_bg);
  init_pair(TUI_COLOR_SUCCESS, COLOR_GREEN, default_bg);
  init_pair(TUI_COLOR_WARNING, COLOR_YELLOW, default_bg);
  init_pair(TUI_COLOR_INFO, COLOR_CYAN, default_bg);
  init_pair(TUI_COLOR_MENU_SELECTED, COLOR_BLACK, COLOR_CYAN);
  init_pair(TUI_COLOR_MENU_NORMAL, default_fg, default_bg);
  init_pair(TUI_COLOR_BORDER, COLOR_BLUE, default_bg);
  init_pair(TUI_COLOR_TITLE, COLOR_MAGENTA, default_bg);

  return APP_SUCCESS;
}

void tui_set_color(WINDOW *win, tui_color_pair_t color) {
  if (win && has_colors() && color > TUI_COLOR_DEFAULT &&
      color < TUI_COLOR_MAX) {
    wattron(win, COLOR_PAIR(color));
  }
}

void tui_unset_color(WINDOW *win, tui_color_pair_t color) {
  if (win && has_colors() && color > TUI_COLOR_DEFAULT &&
      color < TUI_COLOR_MAX) {
    wattroff(win, COLOR_PAIR(color));
  }
}

tui_window_t *tui_create_window(int height, int width, int y, int x) {
  if (height <= 0 || width <= 0) {
    return NULL;
  }

  if (y < 0) {
    y = 0;
  }
  if (x < 0) {
    x = 0;
  }
  if (tui_initialized) {
    const int max_y = getmaxy(stdscr);
    const int max_x = getmaxx(stdscr);
    if (y >= max_y || x >= max_x) {
      return NULL;
    }
    if (height > max_y - y) {
      height = max_y - y;
    }
    if (width > max_x - x) {
      width = max_x - x;
    }
  }

  tui_window_t *window = app_secure_malloc(sizeof(tui_window_t));
  if (!window) {
    return NULL;
  }

  window->height = height;
  window->width = width;
  window->y = y;
  window->x = x;
  window->has_border = false;
  window->title = NULL;

  window->win = newwin(height, width, y, x);
  if (!window->win) {
    app_secure_free(window, sizeof(tui_window_t));
    return NULL;
  }

  keypad(window->win, TRUE);  // Enable special keys for this window
  return window;
}

static void tui_draw_window_title(tui_window_t *window, const char *title) {
  if (!window || !window->win || !title || !window->has_border) {
    return;
  }

  const int max_width = window->width - 4;
  if (max_width <= 0) {
    return;
  }
  const int display_len = tui_clamped_strlen(title, max_width);
  const int x_pos = (window->width - display_len) / 2;
  tui_set_color(window->win, TUI_COLOR_TITLE);
  mvwprintw(window->win, 0, x_pos, " %.*s ", max_width, title);
  tui_unset_color(window->win, TUI_COLOR_TITLE);
}

tui_window_t *tui_create_centered_window(int height, int width) {
  if (!tui_initialized) {
    return NULL;
  }

  const int max_y = getmaxy(stdscr);
  const int max_x = getmaxx(stdscr);
  if (max_y < 3 || max_x < 8) {
    return NULL;
  }

  if (height > max_y - 2) {
    height = max_y - 2;
  }
  if (width > max_x - 2) {
    width = max_x - 2;
  }
  if (height <= 0 || width <= 0) {
    return NULL;
  }

  const int y = (max_y - height) / 2;
  const int x = (max_x - width) / 2;
  return tui_create_window(height, width, y, x);
}

void tui_destroy_window(tui_window_t *window) {
  if (!window) {
    return;
  }

  if (window->win) {
    delwin(window->win);
  }

  if (window->title) {
    app_secure_free(window->title, strlen(window->title) + 1);
  }

  app_secure_free(window, sizeof(tui_window_t));
}

void tui_draw_border(tui_window_t *window) {
  if (!window || !window->win) {
    return;
  }

  tui_set_color(window->win, TUI_COLOR_BORDER);
  box(window->win, 0, 0);
  tui_unset_color(window->win, TUI_COLOR_BORDER);
  window->has_border = true;

  if (window->title) {
    tui_draw_window_title(window, window->title);
  }
}

void tui_set_window_title(tui_window_t *window, const char *title) {
  if (!window || !window->win || !title) {
    return;
  }

  char *title_copy = app_secure_strdup(title);
  if (!title_copy) {
    return;
  }

  if (window->title) {
    app_secure_free(window->title, strlen(window->title) + 1);
  }
  window->title = title_copy;

  tui_draw_window_title(window, window->title);
}

void tui_refresh_window(tui_window_t *window) {
  if (window && window->win) {
    wrefresh(window->win);
  }
}

void tui_clear_window(tui_window_t *window) {
  if (window && window->win) {
    wclear(window->win);
    if (window->has_border) {
      tui_draw_border(window);
    }
  }
}

void tui_draw_status_line(WINDOW *win, const char *left, const char *right) {
  if (!win) {
    return;
  }

  const int max_y = getmaxy(win);
  const int max_x = getmaxx(win);
  if (max_y <= 0 || max_x <= 0) {
    return;
  }

  const int y = max_y - 1;
  tui_set_color(win, TUI_COLOR_INFO);
  mvwhline(win, y, 0, ' ', max_x);

  if (left) {
    tui_write_clamped(win, y, 1, max_x - 2, left);
  }
  if (right) {
    const int right_len = tui_clamped_strlen(right, max_x - 2);
    const int x = max_x - right_len - 1;
    if (x > 1) {
      tui_write_clamped(win, y, x, right_len, right);
    }
  }

  tui_unset_color(win, TUI_COLOR_INFO);
}

void tui_print_centered(WINDOW *win, int y, const char *text) {
  if (!win || !text) {
    return;
  }

  int max_x = getmaxx(win);
  if (max_x <= 0) {
    return;
  }
  int len = tui_clamped_strlen(text, max_x);
  int x = (max_x - len) / 2;
  if (x < 0)
    x = 0;

  mvwprintw(win, y, x, "%.*s", max_x, text);
}

void tui_print_wrapped(WINDOW *win, int y, int x, int width, const char *text) {
  if (!win || !text || width <= 0) {
    return;
  }

  int current_y = y;
  int current_x = x;
  const char *word_start = text;
  const char *p = text;

  while (*p) {
    // Find end of current word
    while (*p && *p != ' ' && *p != '\n') {
      p++;
    }

    ptrdiff_t word_delta = p - word_start;
    int word_len = word_delta > INT_MAX ? INT_MAX : (int)word_delta;

    // Check if word fits on current line without signed overflow.
    if ((long)current_x + (long)word_len > (long)x + (long)width) {
      current_y++;
      current_x = x;
    }

    // Print word
    mvwaddnstr(win, current_y, current_x, word_start, word_len);
    const long advanced_x = (long)current_x + (long)word_len;
    current_x = advanced_x > INT_MAX ? INT_MAX : (int)advanced_x;

    // Handle space or newline
    if (*p == ' ') {
      if (current_x < INT_MAX) {
        current_x++;
      }
      p++;
    } else if (*p == '\n') {
      current_y++;
      current_x = x;
      p++;
    }

    word_start = p;
  }
}

int tui_get_char(void) {
  return getch();
}

app_error tui_get_string(WINDOW *win, char *buffer, size_t size,
                         const char *prompt) {
  if (!win || !buffer || size == 0) {
    return APP_ERROR_INVALID_ARG;
  }

  // Show prompt
  if (prompt) {
    wprintw(win, "%s", prompt);
  }

  // Enable cursor and echo temporarily
  curs_set(1);
  echo();

  // Get string
  const size_t requested_len = size - 1;
  const int max_chars =
      requested_len > (size_t)INT_MAX ? INT_MAX : (int)requested_len;
  int result = wgetnstr(win, buffer, max_chars);

  // Restore settings
  noecho();
  curs_set(0);

  if (result == ERR) {
    return APP_ERROR_IO;
  }

  return APP_SUCCESS;
}

int tui_show_menu(tui_window_t *window, const char *title,
                  const tui_menu_item_t *items, int item_count,
                  int default_selection) {
  if (!window || !items || item_count <= 0) {
    return -1;
  }

  int selected = -1;
  if (default_selection >= 0 && default_selection < item_count &&
      items[default_selection].enabled) {
    selected = default_selection;
  } else {
    for (int i = 0; i < item_count; i++) {
      if (items[i].enabled) {
        selected = i;
        break;
      }
    }
  }

  if (selected < 0) {
    return -1;  // No enabled items
  }

  while (1) {
    tui_clear_window(window);

    // Draw title
    if (title) {
      tui_set_color(window->win, TUI_COLOR_TITLE);
      tui_print_centered(window->win, 1, title);
      wattroff(window->win, COLOR_PAIR(TUI_COLOR_TITLE));
    }

    // Draw menu items
    int start_y = title ? 3 : 1;
    for (int i = 0; i < item_count; i++) {
      int y = start_y + i * 2;  // Space between items

      if (!items[i].enabled) {
        // Disabled item
        mvwprintw(window->win, y, 4, "  %s (disabled)", items[i].label);
      } else if (i == selected) {
        // Selected item
        tui_set_color(window->win, TUI_COLOR_MENU_SELECTED);
        mvwprintw(window->win, y, 2, "> %s", items[i].label);
        wattroff(window->win, COLOR_PAIR(TUI_COLOR_MENU_SELECTED));

        // Show description if available
        if (items[i].description) {
          mvwprintw(window->win, y + 1, 6, "%s", items[i].description);
        }
      } else {
        // Normal item
        mvwprintw(window->win, y, 4, "%s", items[i].label);
      }
    }

    // Instructions
    int bottom_y = window->height - 2;
    tui_set_color(window->win, TUI_COLOR_INFO);
    mvwprintw(window->win, bottom_y, 2,
              "Use ↑/↓ to navigate, Enter to select, q to cancel");
    wattroff(window->win, COLOR_PAIR(TUI_COLOR_INFO));

    tui_refresh_window(window);

    // Handle input
    int ch = tui_get_char();
    switch (ch) {
    case KEY_UP:
    case 'k':
      do {
        selected--;
        if (selected < 0)
          selected = item_count - 1;
      } while (!items[selected].enabled);
      break;

    case KEY_DOWN:
    case 'j':
      do {
        selected++;
        if (selected >= item_count)
          selected = 0;
      } while (!items[selected].enabled);
      break;

    case '\n':
    case KEY_ENTER:
      return items[selected].id;

    case 'q':
    case 27:  // ESC
      return -1;
    }
  }
}

void tui_show_message(const char *title, const char *message) {
  if (!tui_initialized) {
    return;
  }

  int max_y = getmaxy(stdscr);
  int max_x = getmaxx(stdscr);
  if (max_y < 4 || max_x < 8) {
    return;
  }

  int width = 60;
  int height = 10;
  if (width > max_x - 2) {
    width = max_x - 2;
  }
  if (height > max_y - 2) {
    height = max_y - 2;
  }

  int y = (max_y - height) / 2;
  int x = (max_x - width) / 2;

  tui_window_t *window = tui_create_window(height, width, y, x);
  if (!window) {
    return;
  }

  tui_draw_border(window);
  if (title) {
    tui_set_window_title(window, title);
  }

  // Print message
  if (message) {
    tui_print_wrapped(window->win, 2, 2, width - 4, message);
  }

  // Instructions
  tui_set_color(window->win, TUI_COLOR_INFO);
  tui_print_centered(window->win, height - 2, "Press any key to continue");
  wattroff(window->win, COLOR_PAIR(TUI_COLOR_INFO));

  tui_refresh_window(window);
  tui_get_char();

  tui_destroy_window(window);
  touchwin(stdscr);
  refresh();
}

bool tui_confirm(const char *title, const char *question) {
  if (!tui_initialized) {
    return false;
  }

  int max_y = getmaxy(stdscr);
  int max_x = getmaxx(stdscr);
  if (max_y < 4 || max_x < 8) {
    return false;
  }

  int width = 50;
  int height = 8;
  if (width > max_x - 2) {
    width = max_x - 2;
  }
  if (height > max_y - 2) {
    height = max_y - 2;
  }

  int y = (max_y - height) / 2;
  int x = (max_x - width) / 2;

  tui_window_t *window = tui_create_window(height, width, y, x);
  if (!window) {
    return false;
  }

  tui_draw_border(window);
  if (title) {
    tui_set_window_title(window, title);
  }

  // Print question
  if (question) {
    tui_print_wrapped(window->win, 2, 2, width - 4, question);
  }

  // Instructions
  tui_set_color(window->win, TUI_COLOR_INFO);
  tui_print_centered(window->win, height - 2, "y/n");
  wattroff(window->win, COLOR_PAIR(TUI_COLOR_INFO));

  tui_refresh_window(window);

  bool result = false;
  while (1) {
    int ch = tui_get_char();
    if (ch == 'y' || ch == 'Y') {
      result = true;
      break;
    } else if (ch == 'n' || ch == 'N' || ch == 27) {  // ESC
      result = false;
      break;
    }
  }

  tui_destroy_window(window);
  touchwin(stdscr);
  refresh();
  return result;
}

app_error tui_input_dialog(const char *title, const char *prompt, char *buffer,
                           size_t size) {
  if (!tui_initialized || !buffer || size == 0) {
    return APP_ERROR_INVALID_ARG;
  }

  int max_y = getmaxy(stdscr);
  int max_x = getmaxx(stdscr);
  if (max_y < 4 || max_x < 8) {
    return APP_ERROR_INTERNAL;
  }

  int width = 60;
  int height = 8;
  if (width > max_x - 2) {
    width = max_x - 2;
  }
  if (height > max_y - 2) {
    height = max_y - 2;
  }

  int y = (max_y - height) / 2;
  int x = (max_x - width) / 2;

  tui_window_t *window = tui_create_window(height, width, y, x);
  if (!window) {
    return APP_ERROR_INTERNAL;
  }

  tui_draw_border(window);
  if (title) {
    tui_set_window_title(window, title);
  }

  // Print prompt
  if (prompt) {
    mvwprintw(window->win, 2, 2, "%s", prompt);
  }

  // Input field
  mvwprintw(window->win, 4, 2, "> ");
  tui_refresh_window(window);

  app_error result = tui_get_string(window->win, buffer, size, NULL);

  tui_destroy_window(window);
  touchwin(stdscr);
  refresh();
  return result;
}

void tui_beep(void) {
  beep();
}

void tui_flash(void) {
  flash();
}

int tui_get_max_x(void) {
  return getmaxx(stdscr);
}

int tui_get_max_y(void) {
  return getmaxy(stdscr);
}
