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

static void tui_menu_render_title(const tui_menu_layout_t *L,
                                  const char *title) {
  if (!title || !L->frame)
    return;
  tui_set_color(L->frame->win, TUI_COLOR_TITLE);
  tui_print_centered(L->frame->win, 1, title);
  tui_unset_color(L->frame->win, TUI_COLOR_TITLE);
  tui_set_color(L->frame->win, TUI_COLOR_BORDER);
  mvwhline(L->frame->win, 2, 2, ACS_HLINE, L->frame->width - 4);
  tui_unset_color(L->frame->win, TUI_COLOR_BORDER);
}

static void tui_menu_render_items(const tui_menu_layout_t *L,
                                  const tui_menu_state_t *s) {
  WINDOW *win = L->frame->win;
  const tui_menu_config_t *cfg = tui_menu_state_config(s);
  const int visible = tui_menu_state_visible_count(s);
  const int top = tui_menu_state_top_visible(s);
  const int sel_v = tui_menu_state_selected_visible(s);
  const int rows = L->item_area_h;
  const int label_x = 4;
  const int label_text_x = label_x + 3;
  const int label_max_w = L->frame->width - label_text_x - 4;

  for (int row = 0; row < rows; row++) {
    const int v = top + row;
    if (v >= visible)
      break;
    const int idx = tui_menu_state_visible_at(s, v);
    const tui_menu_item_t *it = &cfg->items[idx];
    const int y = L->item_area_y + row;
    const bool is_selected = (v == sel_v);

    if (it->kind == TUI_MENU_ITEM_SEPARATOR) {
      tui_set_color(win, TUI_COLOR_BORDER);
      mvwhline(win, y, 3, ACS_HLINE, L->frame->width - 6);
      tui_unset_color(win, TUI_COLOR_BORDER);
      continue;
    }

    if (is_selected) {
      tui_set_color(win, TUI_COLOR_MENU_SELECTED);
      mvwhline(win, y, 1, ' ', L->frame->width - 2);
      tui_unset_color(win, TUI_COLOR_MENU_SELECTED);
      tui_set_color(win, TUI_COLOR_ACCENT);
      tui_menu_write_wcs(win, y, 2, 2, L"\u25B8 "); /* ▸ */
      tui_unset_color(win, TUI_COLOR_ACCENT);
    }

    /* Numeric prefix shown when numeric keys are enabled and row < 9 in
     * the *visible* slice. */
    if (cfg->show_numeric_keys && v < 9) {
      char num[4];
      snprintf(num, sizeof(num), "%d.", v + 1);
      if (is_selected)
        tui_set_color(win, TUI_COLOR_MENU_SELECTED);
      else
        tui_set_color(win, TUI_COLOR_INFO);
      mvwaddnstr(win, y, label_x, num, 3);
      if (is_selected)
        tui_unset_color(win, TUI_COLOR_MENU_SELECTED);
      else
        tui_unset_color(win, TUI_COLOR_INFO);
    }

    /* Label, with mnemonic underline. */
    if (is_selected)
      tui_set_color(win, TUI_COLOR_MENU_SELECTED);
    else if (it->disabled)
      tui_set_color(win, TUI_COLOR_DIM);
    const wchar_t *lab = tui_menu_state_label_wcs(s, idx);
    const wchar_t mn = tui_menu_state_mnemonic(s, idx);
    /* Render the label, applying A_UNDERLINE to the first wchar matching
     * mn (case-insensitive) - single pass. */
    int cur_x = label_text_x;
    int budget = label_max_w;
    bool underlined = (mn == 0);
    for (size_t k = 0; lab[k] && budget > 0; k++) {
      int cw = wcwidth(lab[k]);
      if (cw < 0)
        cw = 1;
      if (cw > budget)
        break;
      const bool here = !underlined && (wchar_t)towlower(lab[k]) == mn;
      if (here)
        wattron(win, A_UNDERLINE);
      mvwaddnwstr(win, y, cur_x, &lab[k], 1);
      if (here) {
        wattroff(win, A_UNDERLINE);
        underlined = true;
      }
      cur_x += cw;
      budget -= cw;
    }
    if (is_selected)
      tui_unset_color(win, TUI_COLOR_MENU_SELECTED);
    else if (it->disabled)
      tui_unset_color(win, TUI_COLOR_DIM);

    if (it->disabled) {
      tui_set_color(win, TUI_COLOR_DIM);
      tui_menu_write_wcs(win, y, L->frame->width - 13, 11, L"(disabled) ");
      tui_unset_color(win, TUI_COLOR_DIM);
    }
  }
}

static void tui_menu_render_scrollbar(const tui_menu_layout_t *L,
                                      const tui_menu_state_t *s) {
  WINDOW *win = L->frame->win;
  const int visible = tui_menu_state_visible_count(s);
  const int top = tui_menu_state_top_visible(s);
  const int rows = L->item_area_h;
  const int x = L->frame->width - 3;
  if (top > 0) {
    tui_set_color(win, TUI_COLOR_INFO);
    tui_menu_write_wcs(win, L->item_area_y, x, 1, L"\u25B2"); /* ▲ */
    tui_unset_color(win, TUI_COLOR_INFO);
  }
  if (top + rows < visible) {
    tui_set_color(win, TUI_COLOR_INFO);
    tui_menu_write_wcs(win, L->item_area_y + rows - 1, x, 1, L"\u25BC"); /* ▼ */
    tui_unset_color(win, TUI_COLOR_INFO);
  }
}

static void tui_menu_render_detail(const tui_menu_layout_t *L,
                                   const tui_menu_state_t *s) {
  if (!L->detail_pane_visible)
    return;
  WINDOW *win = L->frame->win;
  const tui_menu_config_t *cfg = tui_menu_state_config(s);
  const int sel_v = tui_menu_state_selected_visible(s);
  const int idx = tui_menu_state_visible_at(s, sel_v);
  if (idx < 0)
    return;

  tui_set_color(win, TUI_COLOR_BORDER);
  mvwhline(win, L->detail_y, 2, ACS_HLINE, L->frame->width - 4);
  tui_unset_color(win, TUI_COLOR_BORDER);
  const char *desc = cfg->items[idx].description;
  if (desc) {
    tui_print_wrapped(win, L->detail_y + 1, 3, L->frame->width - 6, desc);
  }
}

static void tui_menu_render_footer(const tui_menu_layout_t *L,
                                   const tui_menu_state_t *s) {
  WINDOW *win = L->frame->win;
  const tui_menu_config_t *cfg = tui_menu_state_config(s);
  tui_set_color(win, TUI_COLOR_INFO);
  mvwhline(win, L->footer_y, 1, ' ', L->frame->width - 2);

  if (tui_menu_state_search_active(s)) {
    char buf[128];
    /* Convert the wchar query to a multibyte string for display. */
    char mb[64];
    const wchar_t *q = tui_menu_state_search_query(s);
    size_t n = wcstombs(mb, q, sizeof(mb) - 1);
    if (n == (size_t)-1)
      n = 0;
    mb[n] = 0;
    snprintf(buf, sizeof(buf), "Search: %s_     Esc cancel", mb);
    mvwaddnstr(win, L->footer_y, 3, buf, L->frame->width - 6);
  } else {
    const char *parts =
        cfg->enable_search && cfg->show_numeric_keys
            ? "  navigate  / search  1-9 jump  Enter ok  q quit"
        : cfg->enable_search     ? "  navigate  / search  Enter ok  q quit"
        : cfg->show_numeric_keys ? "  navigate  1-9 jump  Enter ok  q quit"
                                 : "  navigate  Enter ok  q quit";
    mvwaddnstr(win, L->footer_y, 3, parts, L->frame->width - 6);
  }
  tui_unset_color(win, TUI_COLOR_INFO);
}

typedef enum {
  TUI_MENU_EV_NONE,
  TUI_MENU_EV_CONFIRM,
  TUI_MENU_EV_CANCEL,
  TUI_MENU_EV_INTERRUPT,
  TUI_MENU_EV_REDRAW,
} tui_menu_event_t;

static tui_menu_event_t menu_handle_key_in_search(tui_menu_state_t *s, int ch) {
  switch (ch) {
  case 27: /* Esc */
    tui_menu_state_search_close(s);
    return TUI_MENU_EV_NONE;
  case '\n':
  case KEY_ENTER:
    return TUI_MENU_EV_CONFIRM;
  case KEY_BACKSPACE:
  case 127:
    tui_menu_state_search_backspace(s);
    return TUI_MENU_EV_NONE;
  default:
    if (ch >= 32 && ch < 0x7f) {
      tui_menu_state_search_append(s, (wchar_t)ch);
    }
    return TUI_MENU_EV_NONE;
  }
}

static tui_menu_event_t menu_handle_key(tui_menu_state_t *s, int ch,
                                        int *out_confirm_id) {
  const tui_menu_config_t *cfg = tui_menu_state_config(s);
  if (tui_menu_state_search_active(s)) {
    return menu_handle_key_in_search(s, ch);
  }
  switch (ch) {
  case KEY_UP:
  case 'k':
    tui_menu_state_step(s, -1);
    return TUI_MENU_EV_NONE;
  case KEY_DOWN:
  case 'j':
    tui_menu_state_step(s, 1);
    return TUI_MENU_EV_NONE;
  case KEY_HOME:
    tui_menu_state_home(s);
    return TUI_MENU_EV_NONE;
  case KEY_END:
    tui_menu_state_end(s);
    return TUI_MENU_EV_NONE;
  case KEY_PPAGE:
    tui_menu_state_page(s, -1, 8);
    return TUI_MENU_EV_NONE;
  case KEY_NPAGE:
    tui_menu_state_page(s, 1, 8);
    return TUI_MENU_EV_NONE;
  case '\n':
  case KEY_ENTER:
    return TUI_MENU_EV_CONFIRM;
  case 'q':
  case 27:
    return TUI_MENU_EV_CANCEL;
  case '/':
    if (cfg->enable_search)
      tui_menu_state_search_open(s);
    return TUI_MENU_EV_NONE;
  default:
    if (cfg->show_numeric_keys && ch >= '1' && ch <= '9') {
      tui_menu_state_numeric_jump(s, ch - '1');
      return TUI_MENU_EV_NONE;
    }
    if (iswalnum((wint_t)ch)) {
      bool beep = false;
      int auto_idx = tui_menu_state_mnemonic_jump(s, (wchar_t)ch, &beep);
      if (auto_idx >= 0) {
        *out_confirm_id = cfg->items[auto_idx].id;
        return TUI_MENU_EV_CONFIRM;
      }
      if (beep)
        tui_beep();
    }
    return TUI_MENU_EV_NONE;
  }
}

#ifdef NCURSES_MOUSE_VERSION
static tui_menu_event_t menu_handle_mouse(tui_menu_state_t *s,
                                          const tui_menu_layout_t *L,
                                          int *out_confirm_id) {
  MEVENT ev;
  if (getmouse(&ev) != OK)
    return TUI_MENU_EV_NONE;
  int wy = ev.y, wx = ev.x;
  if (!wmouse_trafo(L->frame->win, &wy, &wx, FALSE))
    return TUI_MENU_EV_NONE;

  if (wy < L->item_area_y || wy >= L->item_area_y + L->item_area_h) {
    return TUI_MENU_EV_NONE;
  }
  const int row = wy - L->item_area_y;
  const int v = tui_menu_state_top_visible(s) + row;
  if (v < 0 || v >= tui_menu_state_visible_count(s))
    return TUI_MENU_EV_NONE;
  const tui_menu_config_t *cfg = tui_menu_state_config(s);
  const int idx = tui_menu_state_visible_at(s, v);
  const tui_menu_item_t *it = &cfg->items[idx];

  if (ev.bstate & BUTTON4_PRESSED) {
    tui_menu_state_step(s, -1);
    return TUI_MENU_EV_NONE;
  }
  if (ev.bstate & BUTTON5_PRESSED) {
    tui_menu_state_step(s, 1);
    return TUI_MENU_EV_NONE;
  }

  if (it->kind == TUI_MENU_ITEM_SEPARATOR || it->disabled) {
    tui_beep();
    return TUI_MENU_EV_NONE;
  }
  if (ev.bstate & BUTTON1_CLICKED) {
    /* Set selection but don't confirm. */
    const int sel_v = tui_menu_state_selected_visible(s);
    if (v != sel_v) {
      /* step to v using the model API by walking */
      const int delta = v - sel_v;
      tui_menu_state_step(s, delta > 0 ? 1 : -1);
    }
    return TUI_MENU_EV_NONE;
  }
  if (ev.bstate & BUTTON1_DOUBLE_CLICKED) {
    *out_confirm_id = it->id;
    return TUI_MENU_EV_CONFIRM;
  }
  return TUI_MENU_EV_NONE;
}
#endif

/* Stub - implemented in Task 16. */
tui_menu_result_t tui_show_menu(tui_window_t *window,
                                const tui_menu_config_t *config) {
  (void)window;
  (void)config;
  (void)tui_menu_write_wcs;
  (void)tui_menu_layout_compute;
  (void)tui_menu_render_title;
  (void)tui_menu_render_items;
  (void)tui_menu_render_scrollbar;
  (void)tui_menu_render_detail;
  (void)tui_menu_render_footer;
  (void)menu_handle_key;
#ifdef NCURSES_MOUSE_VERSION
  (void)menu_handle_mouse;
#endif
  return (tui_menu_result_t){.status = TUI_MENU_INVALID_ARG};
}
