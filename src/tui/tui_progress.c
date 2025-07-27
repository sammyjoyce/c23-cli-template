/*
 * tui_progress.c - progress bar implementation using ncurses windows.
 */

#include "tui_progress.h"

#include <stdlib.h>
#include <string.h>

#include "../utils/memory.h"
#include "tui.h"

struct tui_progress {
  tui_window_t *window;
  int max_value;
  int current_value;
  char *title;
};

static void tui_progress_draw(const tui_progress_t *progress,
                              const char *status) {
  if (!progress || !progress->window) {
    return;
  }

  const tui_window_t *window = progress->window;
  WINDOW *win = window->win;
  tui_clear_window((tui_window_t *)window);  // Cast away const for ncurses API

  // Draw title
  if (progress->title) {
    tui_set_color(win, TUI_COLOR_TITLE);
    tui_print_centered(win, 1, progress->title);
    wattroff(win, COLOR_PAIR(TUI_COLOR_TITLE));
  }

  // Progress bar dimensions
  const int bar_width = window->width - 6;  // padding
  const int bar_y = window->height / 2;
  const int bar_x = 3;

  // Calculate fill ratio and clamp to [0.0, 1.0]
  double ratio = (double)progress->current_value / (double)progress->max_value;
  if (ratio < 0.0)
    ratio = 0.0;
  if (ratio > 1.0)
    ratio = 1.0;

  const int fill_width = (int)(ratio * bar_width);

  // Draw progress bar background
  mvwhline(win, bar_y, bar_x, ' ', bar_width);

  // Draw filled portion
  tui_set_color(win, TUI_COLOR_HIGHLIGHT);
  mvwhline(win, bar_y, bar_x, ' ', fill_width);
  wattroff(win, COLOR_PAIR(TUI_COLOR_HIGHLIGHT));

  // Status text
  if (status) {
    tui_print_centered(win, bar_y + 2, status);
  }

  tui_refresh_window((tui_window_t *)window);
}

// Public API

APP_NODISCARD tui_progress_t *tui_progress_create(const char *title, int max) {
  if (max <= 0) {
    return NULL;
  }

  int max_y = tui_get_max_y();
  int max_x = tui_get_max_x();
  int width = 60;
  int height = 7;
  if (width > max_x - 4)
    width = max_x - 4;
  int y = (max_y - height) / 2;
  int x = (max_x - width) / 2;

  tui_window_t *w = tui_create_window(height, width, y, x);
  if (!w) {
    return NULL;
  }
  tui_draw_border(w);
  tui_set_window_title(w, title ? title : "Progress");

  tui_progress_t *progress = app_secure_malloc(sizeof(tui_progress_t));
  if (!progress) {
    tui_destroy_window(w);
    return NULL;
  }
  progress->window = w;
  progress->max_value = max;
  progress->current_value = 0;
  progress->title = title ? app_secure_strdup(title) : NULL;

  tui_progress_draw(progress, NULL);
  return progress;
}

void tui_progress_update(tui_progress_t *progress, int current,
                         const char *status) {
  if (!progress)
    return;
  progress->current_value = current;
  tui_progress_draw(progress, status);
}

void tui_progress_destroy(tui_progress_t *progress) {
  if (!progress)
    return;
  if (progress->title)
    app_secure_free(progress->title, strlen(progress->title) + 1);
  tui_destroy_window(progress->window);
  app_secure_free(progress, sizeof(tui_progress_t));
}
