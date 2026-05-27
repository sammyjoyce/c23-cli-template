/*
 * tui_internal.h - private helpers shared by TUI implementation units.
 *
 * This header is not part of the public TUI API; it exists so modal redraws
 * and menu orchestration can share state without ad-hoc extern declarations.
 */
#pragma once

#include "tui.h"

void tui_set_background_window(tui_window_t *window);
void tui_clear_background_window(void);
tui_window_t *tui_get_background_window(void);
