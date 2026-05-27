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

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

#include "../utils/logging.h"
#include "tui.h"
#include "tui_internal.h"
#include "tui_menu.h"
#include "tui_menu_internal.h"

#ifdef _WIN32
static int tui_wcwidth(wchar_t wc) {
  if (wc == 0)
    return 0;
  if (wc < 32 || (wc >= 0x7f && wc < 0xa0))
    return -1;
  return wc >= 0x1100 ? 2 : 1;
}
#else
static int tui_wcwidth(wchar_t wc) {
  return wcwidth(wc);
}
#endif

static void tui_menu_write_wchar(WINDOW *w, int y, int x, wchar_t wc) {
  char mb[MB_LEN_MAX];
  mbstate_t state = {0};
  size_t len = wcrtomb(mb, wc, &state);
  if (len == (size_t)-1 || len == 0) {
    mb[0] = '?';
    len = 1;
  }
  mvwaddnstr(w, y, x, mb, (int)len);
}

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

  int used = 0;
  int cur_x = x;
  for (size_t i = 0; s[i]; i++) {
    int w_cols = tui_wcwidth(s[i]);
    if (w_cols < 0)
      w_cols = 1; /* non-printable: best effort */
    if (used + w_cols > cols)
      break;
    tui_menu_write_wchar(w, y, cur_x, s[i]);
    used += w_cols;
    cur_x += w_cols;
  }
}

typedef struct {
  tui_window_t *frame;
  bool owns_frame;
  int item_area_y;
  int item_area_h;
  int detail_y;
  int detail_h;
  int footer_y;
  int desired_h;
  int desired_w;
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

static bool tui_menu_recenter_frame(tui_menu_layout_t *L) {
  if (!L || !L->frame || !L->frame->win)
    return false;

  const int max_y = getmaxy(stdscr);
  const int max_x = getmaxx(stdscr);
  int height = L->desired_h > 0 ? L->desired_h : L->frame->height;
  int width = L->desired_w > 0 ? L->desired_w : L->frame->width;
  if (height > max_y)
    height = max_y;
  if (width > max_x)
    width = max_x;
  if (height < 6 + MENU_FOOTER_ROWS + MENU_MIN_ITEM_ROWS || width < 24)
    return false;

  const int y = (max_y - height) / 2;
  const int x = (max_x - width) / 2;
  if (wresize(L->frame->win, height, width) == ERR)
    return false;
  if (mvwin(L->frame->win, y, x) == ERR)
    return false;

  L->frame->height = height;
  L->frame->width = width;
  L->frame->y = y;
  L->frame->x = x;
  touchwin(stdscr);
  return true;
}

static void tui_menu_render_title(const tui_menu_layout_t *L,
                                  const char *title) {
  /* The window border (drawn separately) already shows the title at row 0
   * via tui_set_window_title, so we only render a separator rule beneath
   * it to demarcate the items area. */
  if (!title || !L->frame)
    return;
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

  for (int row = 0; row < rows; row++) {
    const int v = top + row;
    if (v >= visible)
      break;
    const int idx = tui_menu_state_visible_at(s, v);
    const tui_menu_item_t *it = &cfg->items[idx];
    const int y = L->item_area_y + row;
    const bool is_selected = (v == sel_v);
    const int disabled_suffix_cols = it->disabled ? 13 : 0;
    const int label_max_w =
        L->frame->width - label_text_x - 4 - disabled_suffix_cols;

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
      int cw = tui_wcwidth(lab[k]);
      if (cw < 0)
        cw = 1;
      if (cw > budget)
        break;
      const bool here = !underlined && (wchar_t)towlower(lab[k]) == mn;
      if (here)
        wattron(win, A_UNDERLINE);
      tui_menu_write_wchar(win, y, cur_x, lab[k]);
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
                                        int page_rows, int *out_confirm_index) {
  const tui_menu_config_t *cfg = tui_menu_state_config(s);
  const int page_size = page_rows > 0 ? page_rows : MENU_MIN_ITEM_ROWS;
  if (tui_menu_state_search_active(s)) {
    return menu_handle_key_in_search(s, ch);
  }

  if (ch > 0 && ch < KEY_MIN && iswalnum((wint_t)ch)) {
    bool beep = false;
    int auto_idx = tui_menu_state_mnemonic_jump(s, (wchar_t)ch, &beep);
    if (auto_idx >= 0) {
      *out_confirm_index = auto_idx;
      return TUI_MENU_EV_CONFIRM;
    }
    if (beep) {
      tui_beep();
      return TUI_MENU_EV_NONE;
    }
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
    tui_menu_state_page(s, -1, page_size);
    return TUI_MENU_EV_NONE;
  case KEY_NPAGE:
    tui_menu_state_page(s, 1, page_size);
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
    return TUI_MENU_EV_NONE;
  }
}

#ifdef NCURSES_MOUSE_VERSION
static tui_menu_event_t menu_handle_mouse(tui_menu_state_t *s,
                                          const tui_menu_layout_t *L,
                                          int *out_confirm_index) {
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
    (void)tui_menu_state_select_visible(s, v);
    return TUI_MENU_EV_NONE;
  }
  if (ev.bstate & BUTTON1_DOUBLE_CLICKED) {
    if (!tui_menu_state_select_visible(s, v))
      return TUI_MENU_EV_NONE;
    *out_confirm_index = idx;
    return TUI_MENU_EV_CONFIRM;
  }
  return TUI_MENU_EV_NONE;
}
#endif

tui_menu_result_t tui_show_menu(tui_window_t *window,
                                const tui_menu_config_t *config) {
  tui_menu_result_t result = {.status = TUI_MENU_INVALID_ARG,
                              .selected_id = TUI_MENU_ID_NONE,
                              .selected_index = -1};
  if (!config)
    return result;

  tui_menu_state_t *state = NULL;
  tui_menu_status_t st = tui_menu_state_create(config, &state);
  if (st != TUI_MENU_OK) {
    result.status = st;
    return result;
  }

  tui_menu_layout_t L = {0};
  L.frame = window;
  L.owns_frame = (window == NULL);
  const bool has_requested_frame_size =
      config->frame_height > 0 || config->frame_width > 0;
  L.desired_h = config->frame_height > 0 ? config->frame_height
                                         : (window ? window->height : 22);
  L.desired_w = config->frame_width > 0 ? config->frame_width
                                        : (window ? window->width : 72);
  if (L.owns_frame) {
    const int h = L.desired_h;
    const int w = L.desired_w;
    L.frame = tui_create_centered_window(h, w);
    if (!L.frame) {
      tui_menu_state_destroy(state);
      result.status = TUI_MENU_TOO_SMALL;
      return result;
    }
    tui_draw_border(L.frame);
    if (config->title)
      tui_set_window_title(L.frame, config->title);
  } else if (has_requested_frame_size && !tui_menu_recenter_frame(&L)) {
    tui_menu_state_destroy(state);
    result.status = TUI_MENU_TOO_SMALL;
    return result;
  }

  const bool pushed_background = tui_get_background_window() != L.frame;
  if (pushed_background)
    tui_push_background(L.frame);

#ifdef NCURSES_MOUSE_VERSION
  mmask_t prev_mask = 0;
  if (config->enable_mouse) {
    mousemask(BUTTON1_CLICKED | BUTTON1_DOUBLE_CLICKED | BUTTON4_PRESSED |
                  BUTTON5_PRESSED,
              &prev_mask);
  }
#endif

  bool exit_loop = false;
  while (!exit_loop) {
    if (!tui_menu_layout_compute(&L, L.frame, config)) {
      result.status = TUI_MENU_TOO_SMALL;
      break;
    }
    werase(L.frame->win);
    tui_draw_border(L.frame);
    if (config->title)
      tui_menu_render_title(&L, config->title);

    tui_menu_state_ensure_selection_visible(state, L.item_area_h);

    tui_menu_render_items(&L, state);
    tui_menu_render_scrollbar(&L, state);
    if (L.detail_pane_visible)
      tui_menu_render_detail(&L, state);
    tui_menu_render_footer(&L, state);

    wnoutrefresh(L.frame->win);
    doupdate();

    const int ch = wgetch(L.frame->win);
    int confirm_index = -1;
    tui_menu_event_t ev = TUI_MENU_EV_NONE;

    if (ch == ERR) {
      if (tui_interrupted()) {
        tui_acknowledge_interrupt();
        result.status = TUI_MENU_INTERRUPTED;
        exit_loop = true;
      }
      continue;
    }
    if (ch == KEY_RESIZE) {
      if (L.owns_frame) {
        tui_window_t *old_frame = L.frame;
        tui_window_t *new_frame =
            tui_create_centered_window(L.desired_h, L.desired_w);
        if (!new_frame) {
          tui_replace_background(old_frame, NULL);
          tui_destroy_window(old_frame);
          L.frame = NULL;
          result.status = TUI_MENU_TOO_SMALL;
          break;
        }
        L.frame = new_frame;
        tui_replace_background(old_frame, L.frame);
        tui_destroy_window(old_frame);
        tui_draw_border(L.frame);
        if (config->title)
          tui_set_window_title(L.frame, config->title);
      } else if (!tui_menu_recenter_frame(&L)) {
        result.status = TUI_MENU_TOO_SMALL;
        break;
      }
      clear();
      refresh();
      continue;
    }
#ifdef NCURSES_MOUSE_VERSION
    if (ch == KEY_MOUSE) {
      ev = menu_handle_mouse(state, &L, &confirm_index);
    } else
#endif
    {
      ev = menu_handle_key(state, ch, L.item_area_h, &confirm_index);
    }

    switch (ev) {
    case TUI_MENU_EV_CONFIRM: {
      const int idx = confirm_index >= 0 ? confirm_index
                                         : tui_menu_state_selected_index(state);
      if (idx < 0) {
        tui_beep();
        continue;
      }
      const tui_menu_item_t *it = &config->items[idx];
      if (it->disabled || it->kind == TUI_MENU_ITEM_SEPARATOR) {
        tui_beep();
        continue;
      }
      result.status = TUI_MENU_OK;
      result.selected_id = it->id;
      result.selected_index = idx;
      exit_loop = true;
      break;
    }
    case TUI_MENU_EV_CANCEL:
      result.status = TUI_MENU_CANCELLED;
      exit_loop = true;
      break;
    case TUI_MENU_EV_NONE:
      break;
    }
  }

#ifdef NCURSES_MOUSE_VERSION
  if (config->enable_mouse)
    mousemask(prev_mask, NULL);
#endif

  if (pushed_background)
    tui_pop_background();
  if (L.owns_frame) {
    if (L.frame)
      tui_destroy_window(L.frame);
  }
  tui_menu_state_destroy(state);
  return result;
}
