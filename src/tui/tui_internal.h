/*
 * tui_internal.h - private helpers shared by TUI implementation units.
 *
 * This header is not part of the public TUI API; it exists so modal redraws
 * and menu orchestration can share state without ad-hoc extern declarations.
 */
#pragma once

#include "tui.h"

typedef enum {
  TUI_MODAL_CONTINUE,
  TUI_MODAL_DONE,
} tui_modal_decision_t;

typedef void (*tui_modal_redraw_fn)(tui_window_t *window, void *userdata);
typedef tui_modal_decision_t (*tui_modal_key_fn)(int ch, void *userdata);

void tui_set_background_window(tui_window_t *window);
void tui_clear_background_window(void);
tui_window_t *tui_get_background_window(void);
void tui_push_background(tui_window_t *window);
void tui_pop_background(void);

bool tui_modal_run(int height, int width, const char *title,
                   tui_modal_redraw_fn redraw, tui_modal_key_fn handle,
                   void *userdata);
