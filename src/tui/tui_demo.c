/*
 * Interactive TUI demo used by the template's `menu` command.
 */

#include <stdio.h>

#include "../core/error.h"
#include "tui.h"

#define TUI_DEMO_MIN_HEIGHT 14
#define TUI_DEMO_MIN_WIDTH 40
#define TUI_DEMO_MAX_HEIGHT 16
#define TUI_DEMO_MAX_WIDTH 64

enum {
  TUI_DEMO_ACTION_HELLO = 1,
  TUI_DEMO_ACTION_PROGRESS = 2,
  TUI_DEMO_ACTION_CONFIRM = 3,
  TUI_DEMO_ACTION_EXIT = 4,
};

static int tui_demo_min_int(int lhs, int rhs) {
  return lhs < rhs ? lhs : rhs;
}

static tui_window_t *tui_demo_create_menu_window(void) {
  const int max_y = tui_get_max_y();
  const int max_x = tui_get_max_x();

  if (max_y < TUI_DEMO_MIN_HEIGHT || max_x < TUI_DEMO_MIN_WIDTH) {
    return nullptr;
  }

  int height = tui_demo_min_int(TUI_DEMO_MAX_HEIGHT, max_y - 2);
  int width = tui_demo_min_int(TUI_DEMO_MAX_WIDTH, max_x - 2);

  if (height < TUI_DEMO_MIN_HEIGHT) {
    height = TUI_DEMO_MIN_HEIGHT;
  }
  if (width < TUI_DEMO_MIN_WIDTH) {
    width = TUI_DEMO_MIN_WIDTH;
  }

  const int y = (max_y - height) / 2;
  const int x = (max_x - width) / 2;

  tui_window_t *window = tui_create_window(height, width, y, x);
  if (window) {
    tui_draw_border(window);
  }

  return window;
}

static void tui_demo_show_progress(void) {
  tui_progress_t *progress = tui_progress_create("Demo Progress", 100);
  if (!progress) {
    tui_show_message("Progress",
                     "The terminal is too small for the progress demo.");
    return;
  }

  for (int value = 0; value <= 100; value += 10) {
    char status[64] = {0};
    (void)snprintf(status, sizeof(status), "Processing... %d%%", value);
    tui_progress_update(progress, value, status);
    napms(80);
  }

  tui_progress_destroy(progress);
  touchwin(stdscr);
  refresh();
  tui_show_message("Progress", "Progress demo complete.");
}

APP_NODISCARD app_error tui_run_demo(void) {
  app_error err = tui_init();
  if (err != APP_SUCCESS) {
    return err;
  }

  static const tui_menu_item_t items[] = {
      {.label = "Hello",
       .description = "Show a simple message dialog",
       .id = TUI_DEMO_ACTION_HELLO,
       .enabled = true},
      {.label = "Progress",
       .description = "Run a short progress-bar animation",
       .id = TUI_DEMO_ACTION_PROGRESS,
       .enabled = true},
      {.label = "Confirm",
       .description = "Show a yes/no confirmation dialog",
       .id = TUI_DEMO_ACTION_CONFIRM,
       .enabled = true},
      {.label = "Exit",
       .description = "Return to the command line",
       .id = TUI_DEMO_ACTION_EXIT,
       .enabled = true},
  };

  char title[64] = {0};
  (void)snprintf(title, sizeof(title), "%s TUI Demo", APP_NAME);

  app_error result = APP_SUCCESS;

  while (true) {
    tui_window_t *menu = tui_demo_create_menu_window();
    if (!menu) {
      result = APP_ERROR_INTERNAL;
      break;
    }

    const int choice = tui_show_menu(
        menu, title, items, (int)(sizeof(items) / sizeof(items[0])), 0);

    tui_destroy_window(menu);
    touchwin(stdscr);
    refresh();

    if (choice == -1 || choice == TUI_DEMO_ACTION_EXIT) {
      break;
    }

    switch (choice) {
    case TUI_DEMO_ACTION_HELLO:
      tui_show_message("Hello", "Hello from the ncurses-powered TUI demo.");
      break;
    case TUI_DEMO_ACTION_PROGRESS:
      tui_demo_show_progress();
      break;
    case TUI_DEMO_ACTION_CONFIRM:
      if (tui_confirm("Confirm", "Do you want to keep exploring the demo?")) {
        tui_show_message("Confirm", "Great — returning to the menu.");
      } else {
        tui_show_message("Confirm", "No problem — returning to the menu.");
      }
      break;
    default:
      tui_beep();
      break;
    }
  }

  tui_cleanup();
  return result;
}
