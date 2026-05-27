/*
 * tui_menu.c - view + controller + orchestrator for the tui_menu module.
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
  if (!w || !s || cols <= 0)
    return;
  const int max_y = getmaxy(w);
  const int max_x = getmaxx(w);
  if (y < 0 || y >= max_y || x < 0 || x >= max_x)
    return;
  if (cols > max_x - x)
    cols = max_x - x;
  if (cols <= 0)
    return;

  /* Walk s, accumulating wchars whose total column width fits. */
  int used = 0;
  size_t count = 0;
  for (; s[count]; count++) {
    int w_cols = wcwidth(s[count]);
    if (w_cols < 0)
      w_cols = 1; /* non-printable: best effort */
    if (used + w_cols > cols)
      break;
    used += w_cols;
  }
  mvwaddnwstr(w, y, x, s, (int)count);
}

/* Stub - implemented in Task 16. */
tui_menu_result_t tui_show_menu(tui_window_t *window,
                                const tui_menu_config_t *config) {
  (void)window;
  (void)config;
  (void)tui_menu_write_wcs;
  return (tui_menu_result_t){.status = TUI_MENU_INVALID_ARG};
}
