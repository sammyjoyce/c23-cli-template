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
#define MENU_TITLE_ROWS 2 /* title + separator hline */
#define MENU_DETAIL_MIN 3 /* min rows for detail pane */

static bool tui_menu_layout_compute(tui_menu_layout_t *L, const tui_window_t *w,
                                    const tui_menu_config_t *cfg) {
  if (!L || !w || !cfg)
    return false;
  const int H = w->height;
  const int W = w->width;
  if (H < 6 + MENU_FOOTER_ROWS + MENU_MIN_ITEM_ROWS)
    return false;
  if (W < 24)
    return false;

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

/* Stub - implemented in Task 16. */
tui_menu_result_t tui_show_menu(tui_window_t *window,
                                const tui_menu_config_t *config) {
  (void)window;
  (void)config;
  (void)tui_menu_write_wcs;
  (void)tui_menu_layout_compute;
  return (tui_menu_result_t){.status = TUI_MENU_INVALID_ARG};
}
