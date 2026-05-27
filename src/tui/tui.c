/*
 * Terminal UI (TUI) implementation using ncurses.
 */

#include "tui.h"

#include <limits.h>
#include <locale.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#define isatty _isatty
#define fileno _fileno
#else
#include <unistd.h>
#endif

#include "../utils/logging.h"

static bool tui_initialized = false;
static bool tui_default_colors = false;
static volatile sig_atomic_t tui_interrupted_flag = 0;
static int tui_saved_cursor_state = 0;
static tui_window_t *tui_background_win = NULL;

/* ---- signal handling ---------------------------------------------------- */

#ifndef _WIN32
static void tui_signal_handler(int signum) {
  (void)signum;
  tui_interrupted_flag = 1;
}
#endif

static void tui_install_signal_handlers(void) {
#ifndef _WIN32
  signal(SIGINT, tui_signal_handler);
  signal(SIGTERM, tui_signal_handler);
#endif
}

static void tui_uninstall_signal_handlers(void) {
#ifndef _WIN32
  signal(SIGINT, SIG_DFL);
  signal(SIGTERM, SIG_DFL);
#endif
}

/* ---- helpers ------------------------------------------------------------- */

static bool tui_has_interactive_terminal(void) {
  return isatty(fileno(stdin)) && isatty(fileno(stdout));
}

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

/* ---- public lifecycle --------------------------------------------------- */

app_error tui_init(void) {
  if (tui_initialized) {
    return APP_SUCCESS;
  }

  setlocale(LC_ALL, "");

  if (!tui_has_interactive_terminal()) {
    LOG_ERROR("TUI requires an interactive terminal");
    return APP_ERROR_IO;
  }

  if (initscr() == NULL) {
    LOG_ERROR("Failed to initialize ncurses");
    return APP_ERROR_INTERNAL;
  }

  tui_install_signal_handlers();

  cbreak();
  noecho();
  keypad(stdscr, TRUE);
  tui_saved_cursor_state = curs_set(0);

  if (has_colors()) {
    if (start_color() != ERR) {
      tui_default_colors = use_default_colors() != ERR;
      (void)tui_init_colors();
    }
  }

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

  clear();
  refresh();
  endwin();
  tui_uninstall_signal_handlers();

  tui_initialized = false;
  tui_default_colors = false;
  tui_background_win = NULL;
  tui_interrupted_flag = 0;
  LOG_DEBUG("TUI cleaned up");
}

bool tui_is_initialized(void) {
  return tui_initialized;
}

bool tui_interrupted(void) {
  return tui_interrupted_flag != 0;
}

void tui_set_background_window(tui_window_t *window) {
  tui_background_win = window;
}
void tui_clear_background_window(void) {
  tui_background_win = NULL;
}

/* ---- color management --------------------------------------------------- */

app_error tui_init_colors(void) {
  if (!has_colors()) {
    LOG_WARNING("Terminal does not support colors");
    return APP_SUCCESS;
  }

  const short default_fg = tui_default_fg();
  const short default_bg = tui_default_bg();

  init_pair(TUI_COLOR_HIGHLIGHT, COLOR_BLACK, COLOR_WHITE);
  init_pair(TUI_COLOR_ERROR, COLOR_RED, default_bg);
  init_pair(TUI_COLOR_SUCCESS, COLOR_GREEN, default_bg);
  init_pair(TUI_COLOR_WARNING, COLOR_YELLOW, default_bg);
  init_pair(TUI_COLOR_INFO, COLOR_CYAN, default_bg);
  init_pair(TUI_COLOR_MENU_SELECTED, COLOR_BLACK, COLOR_CYAN);
  init_pair(TUI_COLOR_MENU_NORMAL, default_fg, default_bg);
  init_pair(TUI_COLOR_BORDER, COLOR_BLUE, default_bg);
  init_pair(TUI_COLOR_TITLE, COLOR_MAGENTA, default_bg);
  init_pair(TUI_COLOR_ACCENT, COLOR_WHITE, COLOR_CYAN);
  init_pair(TUI_COLOR_DIM, COLOR_WHITE, default_bg);

  return APP_SUCCESS;
}

void tui_set_color(WINDOW *win, tui_color_pair_t color) {
  if (win && has_colors() && color > TUI_COLOR_DEFAULT &&
      color < TUI_COLOR_MAX) {
    wattron(win, COLOR_PAIR(color) | (color == TUI_COLOR_DIM ? A_DIM : 0));
  }
}

void tui_unset_color(WINDOW *win, tui_color_pair_t color) {
  if (win && has_colors() && color > TUI_COLOR_DEFAULT &&
      color < TUI_COLOR_MAX) {
    wattroff(win, COLOR_PAIR(color) | (color == TUI_COLOR_DIM ? A_DIM : 0));
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

  tui_window_t *window = calloc(1, sizeof(tui_window_t));
  if (!window) {
    return NULL;
  }

  window->height = height;
  window->width = width;
  window->y = y;
  window->x = x;

  window->win = newwin(height, width, y, x);
  if (!window->win) {
    free(window);
    return NULL;
  }

  keypad(window->win, TRUE);
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

  free(window->title);
  free(window);
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

  char *title_copy = strdup(title);
  if (!title_copy) {
    return;
  }

  free(window->title);
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

  const int y = max_y > 2 ? max_y - 2 : max_y - 1;
  const int x_start = max_x > 2 ? 1 : 0;
  const int line_width = max_x > 2 ? max_x - 2 : max_x;
  tui_set_color(win, TUI_COLOR_INFO);
  mvwhline(win, y, x_start, ' ', line_width);

  if (left) {
    tui_write_clamped(win, y, x_start + 1, line_width - 2, left);
  }
  if (right) {
    const int right_len = tui_clamped_strlen(right, line_width - 2);
    const int x = x_start + line_width - right_len - 1;
    if (x > x_start + 1) {
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

  tui_write_clamped(win, y, x, max_x - x, text);
}

void tui_print_wrapped(WINDOW *win, int y, int x, int width, const char *text) {
  if (!win || !text || width <= 0) {
    return;
  }

  const int max_y = getmaxy(win);
  const int max_x = getmaxx(win);
  if (max_y <= 0 || max_x <= 0) {
    return;
  }
  if (x < 0) {
    x = 0;
  }
  if (x >= max_x) {
    return;
  }
  if (width > max_x - x) {
    width = max_x - x;
  }

  int current_y = y;
  int current_x = x;
  const char *p = text;

  while (*p && current_y < max_y) {
    if (*p == '\n') {
      current_y++;
      current_x = x;
      p++;
      continue;
    }
    if (*p == ' ') {
      if (current_x + 1 >= x + width) {
        current_y++;
        current_x = x;
      } else {
        current_x++;
      }
      p++;
      continue;
    }

    const char *word_start = p;
    while (*p && *p != ' ' && *p != '\n') {
      p++;
    }

    ptrdiff_t word_delta = p - word_start;
    int word_len = word_delta > INT_MAX ? INT_MAX : (int)word_delta;

    if (current_x > x && (long)current_x + (long)word_len > (long)x + width) {
      current_y++;
      current_x = x;
    }

    while (word_len > 0 && current_y < max_y) {
      const int line_end = x + width;
      if (current_x >= line_end) {
        current_y++;
        current_x = x;
        continue;
      }

      const int remaining = line_end - current_x;
      const int chunk = word_len < remaining ? word_len : remaining;
      tui_write_clamped(win, current_y, current_x, chunk, word_start);
      word_start += chunk;
      word_len -= chunk;
      current_x += chunk;
    }
  }
}

int tui_get_char(void) {
  if (!tui_initialized) {
    return ERR;
  }
  return getch();
}

app_error tui_get_string(WINDOW *win, char *buffer, size_t size,
                         const char *prompt) {
  if (!win || !buffer || size == 0) {
    return APP_ERROR_INVALID_ARG;
  }

  if (prompt) {
    wprintw(win, "%s", prompt);
  }

  const int previous_cursor = curs_set(1);
  echo();

  const size_t requested_len = size - 1;
  const int max_chars =
      requested_len > (size_t)INT_MAX ? INT_MAX : (int)requested_len;
  int result = wgetnstr(win, buffer, max_chars);

  noecho();
  if (previous_cursor != ERR) {
    (void)curs_set(previous_cursor);
  } else {
    (void)curs_set(tui_saved_cursor_state);
  }

  if (result == ERR) {
    return APP_ERROR_IO;
  }

  return APP_SUCCESS;
}

static int tui_menu_next_enabled(const tui_menu_item_t *items, int item_count,
                                 int selected, int direction) {
  if (!items || item_count <= 0 || direction == 0) {
    return -1;
  }

  int next = selected;
  for (int i = 0; i < item_count; i++) {
    next += direction;
    if (next < 0) {
      next = item_count - 1;
    } else if (next >= item_count) {
      next = 0;
    }

    if (items[next].enabled) {
      return next;
    }
  }

  return selected;
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
    return -1;
  }

  const bool has_title = (title != nullptr);
  const int separator_y = has_title ? 2 : 0;
  const int start_y = has_title ? 4 : 2;
  const int footer_y = window->height - 2;
  const int available_rows = footer_y - start_y;
  const int visible_count = available_rows > 1 ? (available_rows + 1) / 2 : 0;
  const int inner_w = window->width - 2;

  if (visible_count <= 0 || window->width < 16) {
    return -1;
  }

  int top = 0;

  while (1) {
    if (tui_interrupted()) {
      return -1;
    }
    tui_clear_window(window);

    if (has_title) {
      tui_set_color(window->win, TUI_COLOR_TITLE);
      tui_print_centered(window->win, 1, title);
      tui_unset_color(window->win, TUI_COLOR_TITLE);

      tui_set_color(window->win, TUI_COLOR_BORDER);
      mvwhline(window->win, separator_y, 2, ACS_HLINE, window->width - 4);
      tui_unset_color(window->win, TUI_COLOR_BORDER);
    }

    if (selected < top) {
      top = selected;
    } else if (selected >= top + visible_count) {
      top = selected - visible_count + 1;
    }
    if (top < 0) {
      top = 0;
    }

    const bool can_scroll_up = (top > 0);
    const bool can_scroll_down = (top + visible_count < item_count);

    for (int row = 0; row < visible_count && top + row < item_count; row++) {
      const int idx = top + row;
      const int y = start_y + row * 2;
      const bool is_selected = (idx == selected);
      const bool row_shows_up = (row == 0 && can_scroll_up);
      const bool row_shows_down = (row == visible_count - 1 && can_scroll_down);

      const int label_x = 4;
      const int label_text_x = label_x + 3;
      const int label_max_w = window->width - label_text_x - 3;
      const int desc_max_w = window->width - label_text_x - 3;

      if (is_selected) {
        tui_set_color(window->win, TUI_COLOR_MENU_SELECTED);
        mvwhline(window->win, y, 1, ' ', window->width - 2);
        tui_unset_color(window->win, TUI_COLOR_MENU_SELECTED);

        tui_set_color(window->win, TUI_COLOR_ACCENT);
        tui_write_clamped(window->win, y, 2, 2, "▶ ");
        tui_unset_color(window->win, TUI_COLOR_ACCENT);

        if (row_shows_up || row_shows_down) {
          const char *arrow = row_shows_up ? "▲" : "▼";
          tui_set_color(window->win, TUI_COLOR_MENU_SELECTED);
          tui_write_clamped(window->win, y, window->width - 5, 3, arrow);
          tui_unset_color(window->win, TUI_COLOR_MENU_SELECTED);
        }
      } else {
        if (row_shows_up || row_shows_down) {
          const char *arrow = row_shows_up ? "▲" : "▼";
          tui_set_color(window->win, TUI_COLOR_INFO);
          tui_write_clamped(window->win, y, window->width - 5, 3, arrow);
          tui_unset_color(window->win, TUI_COLOR_INFO);
        }
      }

      if (idx < 9) {
        char num_buf[4];
        snprintf(num_buf, sizeof(num_buf), "%d.", idx + 1);
        if (is_selected) {
          tui_set_color(window->win, TUI_COLOR_MENU_SELECTED);
          tui_write_clamped(window->win, y, label_x, 3, num_buf);
          tui_unset_color(window->win, TUI_COLOR_MENU_SELECTED);
        } else {
          tui_set_color(window->win, TUI_COLOR_INFO);
          tui_write_clamped(window->win, y, label_x, 3, num_buf);
          tui_unset_color(window->win, TUI_COLOR_INFO);
        }
      } else if (idx < 35) {
        char num_buf[4];
        char letter = (char)('a' + (idx - 9));
        snprintf(num_buf, sizeof(num_buf), "%c.", letter);
        if (is_selected) {
          tui_set_color(window->win, TUI_COLOR_MENU_SELECTED);
          tui_write_clamped(window->win, y, label_x, 3, num_buf);
          tui_unset_color(window->win, TUI_COLOR_MENU_SELECTED);
        } else {
          tui_set_color(window->win, TUI_COLOR_INFO);
          tui_write_clamped(window->win, y, label_x, 3, num_buf);
          tui_unset_color(window->win, TUI_COLOR_INFO);
        }
      }

      if (is_selected) {
        tui_set_color(window->win, TUI_COLOR_MENU_SELECTED);
      }
      tui_write_clamped(window->win, y, label_text_x, label_max_w,
                        items[idx].label ? items[idx].label : "(untitled)");
      if (is_selected) {
        tui_unset_color(window->win, TUI_COLOR_MENU_SELECTED);
      }

      if (items[idx].description) {
        if (is_selected) {
          tui_set_color(window->win, TUI_COLOR_INFO);
        }
        tui_write_clamped(window->win, y + 1, label_text_x, desc_max_w,
                          items[idx].description);
        if (is_selected) {
          tui_unset_color(window->win, TUI_COLOR_INFO);
        }
      }
    }

    char footer[128];
    snprintf(footer, sizeof(footer),
             "  j/k navigate  1-%d select  Enter=ok  q/Esc=quit",
             item_count < 10 ? item_count : 9);
    tui_set_color(window->win, TUI_COLOR_HIGHLIGHT);
    mvwhline(window->win, footer_y, 1, ' ', window->width - 2);
    tui_write_clamped(window->win, footer_y, 3, window->width - 6, footer);
    tui_unset_color(window->win, TUI_COLOR_HIGHLIGHT);

    tui_refresh_window(window);

    int ch = wgetch(window->win);
    switch (ch) {
    case KEY_UP:
    case 'k':
      selected = tui_menu_next_enabled(items, item_count, selected, -1);
      break;

    case KEY_DOWN:
    case 'j':
      selected = tui_menu_next_enabled(items, item_count, selected, 1);
      break;

    case KEY_HOME:
      selected = tui_menu_next_enabled(items, item_count, item_count - 1, 1);
      break;

    case KEY_END:
      selected = tui_menu_next_enabled(items, item_count, 0, -1);
      break;

    case KEY_NPAGE:
      for (int i = 0; i < visible_count; i++) {
        selected = tui_menu_next_enabled(items, item_count, selected, 1);
      }
      break;

    case KEY_PPAGE:
      for (int i = 0; i < visible_count; i++) {
        selected = tui_menu_next_enabled(items, item_count, selected, -1);
      }
      break;

    case '\n':
    case KEY_ENTER:
      return items[selected].id;

    case 'q':
    case 27:
      return -1;

    case KEY_RESIZE:
      touchwin(stdscr);
      refresh();
      break;

    case ERR:
      break;

    default:
      if (ch >= '1' && ch <= '9') {
        int num = ch - '1';
        if (num < item_count && items[num].enabled) {
          selected = num;
        }
      } else if (ch >= 'a' && ch <= 'z') {
        int num = 9 + (ch - 'a');
        if (num < item_count && items[num].enabled) {
          selected = num;
        }
      }
      break;
    }
  }
}

static tui_window_t *tui_modal_open(int preferred_height, int preferred_width,
                                    const char *title) {
  if (!tui_initialized) {
    return NULL;
  }

  const int max_y = getmaxy(stdscr);
  const int max_x = getmaxx(stdscr);
  if (max_y < 4 || max_x < 8) {
    return NULL;
  }

  int width = preferred_width;
  int height = preferred_height;
  if (width < 8) {
    width = 8;
  }
  if (height < 4) {
    height = 4;
  }
  if (width > max_x - 2) {
    width = max_x - 2;
  }
  if (height > max_y - 2) {
    height = max_y - 2;
  }

  touchwin(stdscr);
  werase(stdscr);
  wnoutrefresh(stdscr);

  tui_window_t *window = tui_create_centered_window(height, width);
  if (!window) {
    return NULL;
  }

  tui_draw_border(window);
  if (title) {
    tui_set_window_title(window, title);
  }

  return window;
}

static void tui_modal_close(tui_window_t *window) {
  tui_destroy_window(window);
  touchwin(stdscr);
  if (tui_background_win && tui_background_win->win) {
    tui_clear_window(tui_background_win);
    if (tui_background_win->has_border) {
      tui_draw_border(tui_background_win);
    }
    if (tui_background_win->title) {
      tui_set_window_title(tui_background_win, tui_background_win->title);
    }
    tui_refresh_window(tui_background_win);
  }
  refresh();
}

static void tui_modal_redraw_background(void);

typedef enum {
  TUI_MODAL_CONTINUE,
  TUI_MODAL_DONE,
} tui_modal_decision_t;

typedef void (*tui_modal_redraw_fn)(tui_window_t *window, void *userdata);
typedef tui_modal_decision_t (*tui_modal_key_fn)(int ch, void *userdata);

// Run a modal dialog loop until the key handler reports DONE or the user
// triggers a SIGINT. Handles window setup, redraw on resize, and teardown.
// Returns true if the loop completed normally and false on open failure.
static bool tui_modal_run(int height, int width, const char *title,
                          tui_modal_redraw_fn redraw, tui_modal_key_fn handle,
                          void *userdata) {
  tui_window_t *window = tui_modal_open(height, width, title);
  if (!window) {
    return false;
  }
  redraw(window, userdata);
  tui_refresh_window(window);

  while (1) {
    if (tui_interrupted()) {
      break;
    }
    const int ch = wgetch(window->win);
    if (ch == KEY_RESIZE) {
      tui_modal_redraw_background();
      tui_destroy_window(window);
      window = tui_modal_open(height, width, title);
      if (!window) {
        return false;
      }
      redraw(window, userdata);
      tui_refresh_window(window);
      continue;
    }
    if (ch == ERR) {
      continue;
    }
    if (handle(ch, userdata) == TUI_MODAL_DONE) {
      break;
    }
  }

  tui_modal_close(window);
  return true;
}

static void tui_modal_redraw_background(void) {
  touchwin(stdscr);
  refresh();
  if (tui_background_win && tui_background_win->win) {
    tui_clear_window(tui_background_win);
    if (tui_background_win->has_border) {
      tui_draw_border(tui_background_win);
    }
    if (tui_background_win->title) {
      tui_set_window_title(tui_background_win, tui_background_win->title);
    }
    tui_refresh_window(tui_background_win);
  }
}

typedef struct {
  const char *message;
} tui_message_state_t;

static void tui_message_redraw(tui_window_t *window, void *userdata) {
  const tui_message_state_t *state = userdata;
  if (state->message) {
    tui_print_wrapped(window->win, 2, 2, window->width - 4, state->message);
  }
  tui_set_color(window->win, TUI_COLOR_INFO);
  tui_print_centered(window->win, window->height - 2, "Press any key");
  tui_unset_color(window->win, TUI_COLOR_INFO);
}

static tui_modal_decision_t tui_message_key(int ch, void *userdata) {
  (void)ch;
  (void)userdata;
  return TUI_MODAL_DONE;
}

void tui_show_message(const char *title, const char *message) {
  int height = 8;
  if (message) {
    height = 6;
    for (const char *p = message; *p != '\0'; p++) {
      if (*p == '\n') {
        height++;
      }
    }
  }

  tui_message_state_t state = {.message = message};
  (void)tui_modal_run(height, 60, title, tui_message_redraw, tui_message_key,
                      &state);
}

typedef struct {
  const char *question;
  bool result;
} tui_confirm_state_t;

static void tui_confirm_redraw(tui_window_t *window, void *userdata) {
  const tui_confirm_state_t *state = userdata;
  if (state->question) {
    tui_print_wrapped(window->win, 2, 2, window->width - 4, state->question);
  }
  tui_set_color(window->win, TUI_COLOR_INFO);
  tui_print_centered(window->win, window->height - 2, "y/n, Esc cancels");
  tui_unset_color(window->win, TUI_COLOR_INFO);
}

static tui_modal_decision_t tui_confirm_key(int ch, void *userdata) {
  tui_confirm_state_t *state = userdata;
  if (ch == 'y' || ch == 'Y') {
    state->result = true;
    return TUI_MODAL_DONE;
  }
  if (ch == 'n' || ch == 'N' || ch == 'q' || ch == 'Q' || ch == 27) {
    state->result = false;
    return TUI_MODAL_DONE;
  }
  return TUI_MODAL_CONTINUE;
}

bool tui_confirm(const char *title, const char *question) {
  tui_confirm_state_t state = {.question = question, .result = false};
  if (!tui_modal_run(8, 50, title, tui_confirm_redraw, tui_confirm_key,
                     &state)) {
    return false;
  }
  return state.result;
}

app_error tui_input_dialog(const char *title, const char *prompt, char *buffer,
                           size_t size) {
  if (!tui_initialized || !buffer || size == 0) {
    return APP_ERROR_INVALID_ARG;
  }

  tui_window_t *window = tui_modal_open(8, 60, title);
  if (!window) {
    return APP_ERROR_INTERNAL;
  }

  if (prompt) {
    tui_write_clamped(window->win, 2, 2, window->width - 4, prompt);
  }

  mvwprintw(window->win, 4, 2, "> ");
  tui_refresh_window(window);

  app_error result = tui_get_string(window->win, buffer, size, NULL);

  tui_modal_close(window);
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

bool tui_terminal_meets_minimum(void) {
  return tui_get_max_x() >= TUI_MIN_COLS && tui_get_max_y() >= TUI_MIN_ROWS;
}