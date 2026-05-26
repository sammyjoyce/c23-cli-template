/*
 * TUI starter showcase.
 *
 * Keep this demo in the TUI layer so command handlers can stay focused on
 * routing and error handling.
 */

#include <stdio.h>

#include "tui.h"

static void tui_demo_show_overview(void) {
  tui_show_message(
      "Starter Overview",
      "This template gives you a production-shaped starting point:\n\n"
      "- C23 modules with explicit error codes\n"
      "- Zig build graph with optional ncurses support\n"
      "- CLI output modes for humans and automation\n"
      "- Reusable TUI windows, menus, dialogs, and progress bars");
}

static void tui_demo_show_system_info(void) {
  char info[512];
  snprintf(info, sizeof(info),
           "Application: %s\n"
           "Version: %s\n"
           "Git Commit: %s\n"
           "Build Date: %s\n"
           "Terminal Size: %dx%d\n"
           "Colors Supported: %s",
           APP_NAME, APP_VERSION, APP_GIT_COMMIT, APP_BUILD_DATE,
           tui_get_max_x(), tui_get_max_y(), has_colors() ? "yes" : "no");
  tui_show_message("System Information", info);
}

static void tui_demo_show_input(void) {
  char input_buffer[128] = {0};
  if (tui_input_dialog("Input Dialog", "Enter a display name:", input_buffer,
                       sizeof(input_buffer)) != APP_SUCCESS) {
    return;
  }

  char message[256];
  snprintf(message, sizeof(message),
           "Hello, %s.\n\n"
           "Use this pattern for short prompts. For secrets, keep echo and "
           "cursor state scoped inside a dedicated helper.",
           input_buffer[0] != '\0' ? input_buffer : "World");
  tui_show_message("Input Captured", message);
}

static void tui_demo_show_progress(void) {
  tui_progress_t *progress = tui_progress_create("Progress Pattern", 100);
  if (!progress) {
    tui_show_message("Progress",
                     "The terminal is too small for the progress "
                     "example.");
    return;
  }

  for (int i = 0; i <= 100; i += 5) {
    char status[80];
    snprintf(status, sizeof(status), "Processing step %d of 20", i / 5);
    tui_progress_update(progress, i, status);
    napms(35);
  }

  tui_progress_destroy(progress);
  touchwin(stdscr);
  refresh();
  tui_show_message("Progress Complete",
                   "The progress helper clamps values, "
                   "renders a percentage, and owns its "
                   "window lifecycle.");
}

static void tui_demo_show_layout(void) {
  tui_window_t *window = tui_create_centered_window(14, 72);
  if (!window) {
    return;
  }

  tui_draw_border(window);
  tui_set_window_title(window, "Layout Pattern");
  tui_set_color(window->win, TUI_COLOR_TITLE);
  tui_print_centered(window->win, 2, "Composable terminal UI");
  tui_unset_color(window->win, TUI_COLOR_TITLE);

  tui_print_wrapped(
      window->win, 4, 4, window->width - 8,
      "Prefer small owned windows, bounded writes, and status lines. Keep raw "
      "ncurses calls inside src/tui so command code can stay easy to test.");
  tui_draw_status_line(window->win, "Enter/Esc closes this panel", APP_NAME);
  tui_refresh_window(window);

  while (true) {
    const int ch = tui_get_char();
    if (ch == '\n' || ch == KEY_ENTER || ch == 27 || ch == 'q' || ch == 'Q') {
      break;
    }
  }

  tui_destroy_window(window);
  touchwin(stdscr);
  refresh();
}

app_error tui_run_demo(void) {
  app_error err = tui_init();
  if (err != APP_SUCCESS) {
    return err;
  }

  app_error result = APP_SUCCESS;
  tui_window_t *menu_window = NULL;

  if (!tui_terminal_meets_minimum()) {
    result = APP_ERROR_OUT_OF_RANGE;
    goto cleanup;
  }

  const tui_menu_item_t main_menu[] = {
      {"Overview", "See the starter's CLI and TUI foundation", 1, true},
      {"System Information", "Inspect build, terminal, and color support", 2,
       true},
      {"Input Dialog", "Capture bounded text with scoped echo/cursor state", 3,
       true},
      {"Progress Pattern", "Show a modal progress indicator", 4, true},
      {"Layout Pattern", "Open a reusable panel with a status line", 5, true},
      {"Exit", "Return to the shell", 0, true},
  };

  menu_window = tui_create_centered_window(18, 72);
  if (!menu_window) {
    result = APP_ERROR_MEMORY;
    goto cleanup;
  }
  tui_draw_border(menu_window);
  tui_set_window_title(menu_window, "Main Menu");

  bool running = true;
  while (running) {
    const int choice =
        tui_show_menu(menu_window, "Starter Showcase", main_menu,
                      (int)(sizeof(main_menu) / sizeof(main_menu[0])), 0);

    switch (choice) {
    case 1:
      tui_demo_show_overview();
      break;
    case 2:
      tui_demo_show_system_info();
      break;
    case 3:
      tui_demo_show_input();
      break;
    case 4:
      tui_demo_show_progress();
      break;
    case 5:
      tui_demo_show_layout();
      break;
    case 0:
    case -1:
      running = !tui_confirm("Exit", "Return to the shell?");
      break;
    default:
      tui_beep();
      break;
    }
  }

cleanup:
  if (menu_window) {
    tui_destroy_window(menu_window);
  }
  tui_cleanup();
  return result;
}
