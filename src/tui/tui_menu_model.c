/*
 * tui_menu_model.c - pure-data model for tui_menu.
 *
 * No ncurses; everything is wchar_t / int / pointer math. Unit-testable.
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

#include "tui_menu_internal.h"

struct tui_menu_state {
  const tui_menu_config_t *cfg;
  wchar_t **label_w;  /* size = cfg->item_count; ampersand-stripped */
  wchar_t *mnemonics; /* size = cfg->item_count; lower-cased; 0 = none */
  int *visible;       /* size = cfg->item_count; visible items[] indices */
  int visible_count;
  int selected_visible; /* index into visible[] */
  int top_visible;
  wchar_t search_buf[64];
  size_t search_len;
  bool search_active;
};

static bool menu_item_selectable(const tui_menu_item_t *it) {
  return it && it->kind == TUI_MENU_ITEM_NORMAL && !it->disabled;
}

static int menu_first_enabled(const tui_menu_config_t *cfg) {
  for (int i = 0; i < cfg->item_count; i++) {
    if (menu_item_selectable(&cfg->items[i]))
      return i;
  }
  return -1;
}

/* Defined later in this task chain. Stub now. */
static void menu_state_parse_labels(struct tui_menu_state *s) {
  (void)s; /* Task 6 fills this in */
}
static void menu_state_apply_filter(struct tui_menu_state *s) {
  /* Default visible set: every item, no filter. */
  s->visible_count = 0;
  for (int i = 0; i < s->cfg->item_count; i++) {
    s->visible[s->visible_count++] = i;
  }
}

tui_menu_status_t tui_menu_state_create(const tui_menu_config_t *cfg,
                                        tui_menu_state_t **out) {
  if (!cfg || !out)
    return TUI_MENU_INVALID_ARG;
  *out = NULL;
  if (!cfg->items || cfg->item_count <= 0)
    return TUI_MENU_INVALID_ARG;
  if ((size_t)cfg->item_count > SIZE_MAX / sizeof(int)) {
    return TUI_MENU_INVALID_ARG;
  }

  struct tui_menu_state *s = calloc(1, sizeof(*s));
  if (!s)
    return TUI_MENU_INVALID_ARG;
  s->cfg = cfg;

  s->visible = calloc((size_t)cfg->item_count, sizeof(int));
  s->label_w = calloc((size_t)cfg->item_count, sizeof(wchar_t *));
  s->mnemonics = calloc((size_t)cfg->item_count, sizeof(wchar_t));
  if (!s->visible || !s->label_w || !s->mnemonics) {
    tui_menu_state_destroy(s);
    return TUI_MENU_INVALID_ARG;
  }

  menu_state_parse_labels(s);
  menu_state_apply_filter(s);

  int initial = -1;
  if (cfg->default_index >= 0 && cfg->default_index < cfg->item_count &&
      menu_item_selectable(&cfg->items[cfg->default_index])) {
    initial = cfg->default_index;
  } else {
    initial = menu_first_enabled(cfg);
  }
  if (initial < 0) {
    tui_menu_state_destroy(s);
    return TUI_MENU_INVALID_ARG;
  }
  /* Find initial in visible[]. */
  for (int v = 0; v < s->visible_count; v++) {
    if (s->visible[v] == initial) {
      s->selected_visible = v;
      break;
    }
  }
  *out = s;
  return TUI_MENU_OK;
}

void tui_menu_state_destroy(tui_menu_state_t *s) {
  if (!s)
    return;
  if (s->label_w) {
    for (int i = 0; i < s->cfg->item_count; i++)
      free(s->label_w[i]);
    free(s->label_w);
  }
  free(s->mnemonics);
  free(s->visible);
  free(s);
}

int tui_menu_state_visible_count(const tui_menu_state_t *s) {
  return s ? s->visible_count : 0;
}

int tui_menu_state_selected_index(const tui_menu_state_t *s) {
  if (!s || s->visible_count == 0)
    return -1;
  return s->visible[s->selected_visible];
}

int tui_menu_state_selected_visible(const tui_menu_state_t *s) {
  return s ? s->selected_visible : -1;
}

int tui_menu_state_top_visible(const tui_menu_state_t *s) {
  return s ? s->top_visible : 0;
}

void tui_menu_state_set_top_visible(tui_menu_state_t *s, int top) {
  if (s && top >= 0)
    s->top_visible = top;
}

bool tui_menu_state_search_active(const tui_menu_state_t *s) {
  return s ? s->search_active : false;
}

const wchar_t *tui_menu_state_search_query(const tui_menu_state_t *s) {
  return s ? s->search_buf : L"";
}

const wchar_t *tui_menu_state_label_wcs(const tui_menu_state_t *s, int idx) {
  if (!s || idx < 0 || idx >= s->cfg->item_count)
    return L"";
  return s->label_w[idx] ? s->label_w[idx] : L"";
}

wchar_t tui_menu_state_mnemonic(const tui_menu_state_t *s, int idx) {
  if (!s || idx < 0 || idx >= s->cfg->item_count)
    return 0;
  return s->mnemonics[idx];
}

/* Step/page/home/end/numeric/mnemonic/search ops are added in subsequent
 * tasks. Stub them so the link succeeds during incremental TDD. */
void tui_menu_state_step(tui_menu_state_t *s, int direction) {
  if (!s || s->visible_count == 0 || direction == 0)
    return;
  int next = s->selected_visible;
  for (int i = 0; i < s->visible_count; i++) {
    next += direction;
    if (next < 0)
      next = s->visible_count - 1;
    else if (next >= s->visible_count)
      next = 0;
    const int item_idx = s->visible[next];
    if (menu_item_selectable(&s->cfg->items[item_idx])) {
      s->selected_visible = next;
      return;
    }
  }
  /* All non-selectable; leave selection in place. */
}
void tui_menu_state_home(tui_menu_state_t *s) {
  if (!s || s->visible_count == 0)
    return;
  for (int v = 0; v < s->visible_count; v++) {
    if (menu_item_selectable(&s->cfg->items[s->visible[v]])) {
      s->selected_visible = v;
      return;
    }
  }
}
void tui_menu_state_end(tui_menu_state_t *s) {
  if (!s || s->visible_count == 0)
    return;
  for (int v = s->visible_count - 1; v >= 0; v--) {
    if (menu_item_selectable(&s->cfg->items[s->visible[v]])) {
      s->selected_visible = v;
      return;
    }
  }
}
void tui_menu_state_page(tui_menu_state_t *s, int direction, int page_size) {
  if (!s || page_size <= 0 || direction == 0)
    return;
  for (int i = 0; i < page_size; i++) {
    const int prev = s->selected_visible;
    tui_menu_state_step(s, direction);
    /* Stop if we wrapped (step is wrap-around but page is clamped) or
     * couldn't move. */
    if (s->selected_visible == prev)
      break;
    if (direction > 0 && s->selected_visible < prev) {
      s->selected_visible = prev;
      break;
    }
    if (direction < 0 && s->selected_visible > prev) {
      s->selected_visible = prev;
      break;
    }
  }
}
void tui_menu_state_search_open(tui_menu_state_t *s) {
  (void)s;
}
void tui_menu_state_search_close(tui_menu_state_t *s) {
  (void)s;
}
void tui_menu_state_search_append(tui_menu_state_t *s, wchar_t ch) {
  (void)s;
  (void)ch;
}
void tui_menu_state_search_backspace(tui_menu_state_t *s) {
  (void)s;
}
int tui_menu_state_mnemonic_jump(tui_menu_state_t *s, wchar_t k, bool *b) {
  (void)s;
  (void)k;
  if (b)
    *b = false;
  return -1;
}
void tui_menu_state_numeric_jump(tui_menu_state_t *s, int row) {
  (void)s;
  (void)row;
}
