/*
 * Terminal UI (TUI) implementation using ncurses.
 */

#include "tui.h"

#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tui_internal.h"
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
#define TUI_BACKGROUND_STACK_MAX 16
static tui_window_t *tui_background_stack[TUI_BACKGROUND_STACK_MAX];
static size_t tui_background_stack_depth = 0;

void tui_clear_background_window(void) {
  tui_background_win = NULL;
  for (size_t i = 0; i < tui_background_stack_depth; i++) {
    tui_background_stack[i] = NULL;
  }
  tui_background_stack_depth = 0;
}

tui_window_t *tui_get_background_window(void) {
  return tui_background_win;
}

void tui_push_background(tui_window_t *window) {
  if (tui_background_stack_depth < TUI_BACKGROUND_STACK_MAX) {
    tui_background_stack[tui_background_stack_depth++] = tui_background_win;
  } else {
    for (size_t i = 1; i < TUI_BACKGROUND_STACK_MAX; i++) {
      tui_background_stack[i - 1] = tui_background_stack[i];
    }
    tui_background_stack[TUI_BACKGROUND_STACK_MAX - 1] = tui_background_win;
    LOG_WARNING("TUI background stack overflow; oldest background dropped");
  }
  tui_background_win = window;
}

void tui_pop_background(void) {
  if (tui_background_stack_depth == 0) {
    tui_background_win = NULL;
    return;
  }
  tui_background_stack_depth--;
  tui_background_win = tui_background_stack[tui_background_stack_depth];
  tui_background_stack[tui_background_stack_depth] = NULL;
}

void tui_replace_background(tui_window_t *old_window,
                            tui_window_t *new_window) {
  if (tui_background_win == old_window) {
    tui_background_win = new_window;
  }
  for (size_t i = 0; i < tui_background_stack_depth; i++) {
    if (tui_background_stack[i] == old_window) {
      tui_background_stack[i] = new_window;
    }
  }
}

/* ---- signal handling ---------------------------------------------------- */

#ifndef _WIN32
static void tui_signal_handler(int signum) {
  (void)signum;
  const int saved_errno = errno;
  tui_interrupted_flag = 1;
  errno = saved_errno;
}
#endif

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
  tui_clear_background_window();
  tui_interrupted_flag = 0;
  LOG_DEBUG("TUI cleaned up");
}

bool tui_is_initialized(void) {
  return tui_initialized;
}

bool tui_interrupted(void) {
  return tui_interrupted_flag != 0;
}

void tui_acknowledge_interrupt(void) {
  tui_interrupted_flag = 0;
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

static void tui_modal_restore_background(void) {
  touchwin(stdscr);
  tui_window_t *bg = tui_get_background_window();
  if (bg && bg->win) {
    tui_clear_window(bg);
    if (bg->has_border) {
      tui_draw_border(bg);
    }
    if (bg->title) {
      tui_set_window_title(bg, bg->title);
    }
    tui_refresh_window(bg);
  }
  refresh();
}

static void tui_modal_close(tui_window_t *window) {
  tui_destroy_window(window);
  tui_modal_restore_background();
}

static void tui_modal_redraw_background(void);

// Run a modal dialog loop until the key handler reports DONE or the user
// triggers a SIGINT. Handles window setup, redraw on resize, and teardown.
// Returns true if the loop completed normally and false on open failure.
bool tui_modal_run(int height, int width, const char *title,
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
      tui_acknowledge_interrupt();
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
    if (handle(window, ch, userdata) == TUI_MODAL_DONE) {
      break;
    }
  }

  tui_modal_close(window);
  return true;
}

static void tui_modal_redraw_background(void) {
  tui_modal_restore_background();
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

static tui_modal_decision_t tui_message_key(tui_window_t *window, int ch,
                                            void *userdata) {
  (void)window;
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

static tui_modal_decision_t tui_confirm_key(tui_window_t *window, int ch,
                                            void *userdata) {
  (void)window;
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

typedef struct {
  const char *prompt;
  char *buffer;
  size_t size;
  size_t len;
  app_error result;
} tui_input_state_t;

static void tui_input_redraw(tui_window_t *window, void *userdata) {
  tui_input_state_t *state = userdata;
  if (state->prompt) {
    tui_write_clamped(window->win, 2, 2, window->width - 4, state->prompt);
  }

  mvwprintw(window->win, 4, 2, "> ");
  const int max_cols = window->width > 6 ? window->width - 6 : 0;
  size_t start = 0;
  if (max_cols > 0 && state->len > (size_t)max_cols) {
    start = state->len - (size_t)max_cols;
  }
  if (max_cols > 0) {
    tui_write_clamped(window->win, 4, 4, max_cols, state->buffer + start);
  }
  wmove(window->win, 4, 4 + (int)(state->len - start));
}

static tui_modal_decision_t tui_input_key(tui_window_t *window, int ch,
                                          void *userdata) {
  tui_input_state_t *state = userdata;
  switch (ch) {
  case '\n':
  case KEY_ENTER:
    state->result = APP_SUCCESS;
    return TUI_MODAL_DONE;
  case 27:
    state->result = APP_ERROR_IO;
    return TUI_MODAL_DONE;
  case KEY_BACKSPACE:
  case 127:
  case 8:
    if (state->len > 0) {
      state->buffer[--state->len] = '\0';
    } else {
      tui_beep();
    }
    break;
  default:
    if (ch >= 32 && ch < 0x7f) {
      if (state->len + 1 < state->size) {
        state->buffer[state->len++] = (char)ch;
        state->buffer[state->len] = '\0';
      } else {
        tui_beep();
      }
    }
    break;
  }

  werase(window->win);
  tui_draw_border(window);
  tui_input_redraw(window, state);
  tui_refresh_window(window);
  return TUI_MODAL_CONTINUE;
}

app_error tui_input_dialog(const char *title, const char *prompt, char *buffer,
                           size_t size) {
  if (!tui_initialized || !buffer || size == 0) {
    return APP_ERROR_INVALID_ARG;
  }

  buffer[0] = '\0';
  const int previous_cursor = curs_set(1);
  tui_input_state_t state = {.prompt = prompt,
                             .buffer = buffer,
                             .size = size,
                             .len = 0,
                             .result = APP_ERROR_IO};

  const bool opened =
      tui_modal_run(8, 60, title, tui_input_redraw, tui_input_key, &state);
  if (previous_cursor != ERR) {
    (void)curs_set(previous_cursor);
  } else {
    (void)curs_set(tui_saved_cursor_state);
  }
  return opened ? state.result : APP_ERROR_INTERNAL;
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
