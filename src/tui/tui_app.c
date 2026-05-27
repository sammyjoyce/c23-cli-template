/*
 * tui_app.c - main menu entry point for this template's TUI.
 *
 * This file is the starter shell. The functions below are organized so you can:
 *
 *   1. Edit `main_menu[]` to replace the seed items with your commands.
 *   2. Replace or delete the `app_show_*` handlers that you don't need.
 *   3. Keep `tui_run_app()` as-is - the resize loop, exit confirmation, and
 *      cancel/interrupt handling are part of the reference implementation
 *      and Just Work for any item list you supply.
 *
 * The handlers below also serve as worked examples of every TUI primitive
 * (message dialog, confirm, input dialog, progress bar, custom layout,
 * sub-menu). Treat them as documentation you can delete or copy from.
 */
#include <stdio.h>

#include "tui.h"

/* ============================================================
 * Section 1: Handlers - replace these with your app's actions.
 * ============================================================ */

static void app_show_overview(void) {
  tui_show_message(
      "Starter Overview",
      "This template gives you a production-shaped starting point:\n\n"
      "  C23 modules with explicit error codes\n"
      "  Zig build graph with optional ncurses support\n"
      "  CLI output modes for humans and automation\n"
      "  Reusable TUI windows, menus, dialogs, and progress bars");
}

static void app_show_system_info(void) {
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

static void app_show_input(void) {
  char buf[128] = {0};
  if (tui_input_dialog("Input Dialog", "Enter a display name:", buf,
                       sizeof(buf)) != APP_SUCCESS) {
    return;
  }
  char msg[256];
  snprintf(msg, sizeof(msg),
           "Hello, %s.\n\n"
           "Use this pattern for short prompts. For secrets, keep echo "
           "and cursor state scoped inside a dedicated helper.",
           buf[0] != '\0' ? buf : "World");
  tui_show_message("Input Captured", msg);
}

static void app_show_progress(void) {
  tui_progress_t *progress = tui_progress_create("Progress Pattern", 100);
  if (!progress) {
    tui_show_message("Progress",
                     "The terminal is too small for the progress example.");
    return;
  }
  for (int i = 0; i <= 100; i += 5) {
    char status[80];
    snprintf(status, sizeof(status), "Processing step %d of 20", i / 5);
    tui_progress_update(progress, i, status);
    napms(35);
  }
  tui_progress_destroy(progress);
  tui_show_message("Progress Complete",
                   "The progress helper clamps values, renders a "
                   "percentage, and owns its window lifecycle.");
}

static void app_show_layout(void) {
  tui_window_t *win = tui_create_centered_window(14, 72);
  if (!win)
    return;
  tui_draw_border(win);
  tui_set_window_title(win, "Layout Pattern");
  tui_set_color(win->win, TUI_COLOR_TITLE);
  tui_print_centered(win->win, 2, "Composable terminal UI");
  tui_unset_color(win->win, TUI_COLOR_TITLE);
  tui_print_wrapped(win->win, 4, 4, win->width - 8,
                    "Prefer small owned windows, bounded writes, and "
                    "status lines. Keep raw ncurses calls inside src/tui so "
                    "command code can stay easy to test.");
  tui_draw_status_line(win->win, "Enter/Esc closes this panel", APP_NAME);
  tui_refresh_window(win);
  while (true) {
    int ch = tui_get_char();
    if (ch == '\n' || ch == KEY_ENTER || ch == 27 || ch == 'q' || ch == 'Q')
      break;
  }
  tui_destroy_window(win);
  touchwin(stdscr);
  refresh();
}

static void app_show_config_menu(void) {
  static const tui_menu_item_t cfg_items[] = {
      {.label = "Output &mode",
       .description = "Plain, JSON, or colorized output",
       .id = 1},
      {.label = "&Log level",
       .description = "Quiet, normal, debug verbosity",
       .id = 2},
      {.label = "&Terminal settings",
       .description = "Color, minimum dimensions, fallbacks",
       .id = 3},
      {.label = "&Reset to defaults",
       .description = "Restore all configuration",
       .id = 4},
      {.label = "&Export config",
       .description = "Write current settings to disk",
       .id = 5,
       .disabled = true},
      {.kind = TUI_MENU_ITEM_SEPARATOR},
      {.label = "&Back", .description = "Return to the main menu", .id = 0},
  };
  bool sub_running = true;
  while (sub_running) {
    tui_menu_result_t r = tui_show_menu(
        NULL, &(tui_menu_config_t){
                  .title = "Configuration",
                  .items = cfg_items,
                  .item_count = (int)(sizeof(cfg_items) / sizeof(cfg_items[0])),
                  .default_index = 0,
                  .frame_height = 16,
                  .frame_width = 60,
                  .enable_search = true,
                  .show_detail_pane = true,
                  .show_numeric_keys = true,
              });
    if (r.status != TUI_MENU_OK) {
      sub_running = false;
      continue;
    }
    switch (r.selected_id) {
    case 0:
      sub_running = false;
      break;
    case 1:
      tui_show_message("Output Mode",
                       "Set via --json, --plain, or --quiet flags.");
      break;
    case 2:
      tui_show_message("Log Level", "Use --debug for verbose logging.");
      break;
    case 3:
      tui_show_message("Terminal Settings", "Minimum terminal: 48 x 12.");
      break;
    case 4:
      tui_show_message("Reset to Defaults", "All settings reverted.");
      break;
    default:
      tui_beep();
      break;
    }
  }
}

/* ============================================================
 * Section 2: Menu definition - your items + dispatch table.
 * ============================================================ */

static const tui_menu_item_t main_menu[] = {
    {.label = "&Overview",
     .description =
         "See the starter's CLI, TUI foundation, and core architecture",
     .id = 1},
    {.label = "&System Information",
     .description =
         "Inspect build metadata, terminal dimensions, and color support",
     .id = 2},
    {.label = "&Input Dialog",
     .description = "Capture bounded text with scoped echo and cursor state",
     .id = 3},
    {.kind = TUI_MENU_ITEM_SEPARATOR},
    {.label = "&Progress Pattern",
     .description =
         "Show a modal progress indicator with percentage and status",
     .id = 4},
    {.label = "&Layout Pattern",
     .description = "Open a reusable bordered panel with a status line",
     .id = 5},
    {.label = "&Configuration",
     .description = "Adjust output mode, log level, and terminal settings",
     .id = 6},
    {.kind = TUI_MENU_ITEM_SEPARATOR},
    {.label = "E&xit", .description = "Return to the shell", .id = 0},
};

static void app_dispatch(int id) {
  switch (id) {
  case 1:
    app_show_overview();
    break;
  case 2:
    app_show_system_info();
    break;
  case 3:
    app_show_input();
    break;
  case 4:
    app_show_progress();
    break;
  case 5:
    app_show_layout();
    break;
  case 6:
    app_show_config_menu();
    break;
  default:
    tui_beep();
    break;
  }
}

/* ============================================================
 * Section 3: Entry point - usually no edits needed here.
 * ============================================================ */

app_error tui_run_app(void) {
  app_error err = tui_init();
  if (err != APP_SUCCESS)
    return err;

  /* Own the menu frame here so it remains visible behind dialog modals
   * that handlers may open. tui_show_menu recenters this caller-owned frame
   * on KEY_RESIZE and reports TUI_MENU_TOO_SMALL if it no longer fits. */
  tui_window_t *menu_frame = tui_create_centered_window(22, 72);
  if (!menu_frame) {
    tui_cleanup();
    return APP_ERROR_OUT_OF_RANGE;
  }
  tui_draw_border(menu_frame);
  tui_set_window_title(menu_frame, "Starter Showcase");

  bool running = true;
  while (running) {
    tui_menu_result_t r = tui_show_menu(
        menu_frame,
        &(tui_menu_config_t){
            .title = "Starter Showcase",
            .items = main_menu,
            .item_count = (int)(sizeof(main_menu) / sizeof(main_menu[0])),
            .default_index = 0,
            .enable_search = true,
            .enable_mouse = true,
            .show_detail_pane = true,
            .show_numeric_keys = true,
        });
    switch (r.status) {
    case TUI_MENU_OK:
      if (r.selected_id == 0) {
        running = !tui_confirm("Exit", "Return to the shell?");
      } else {
        app_dispatch(r.selected_id);
      }
      break;
    case TUI_MENU_CANCELLED:
      running = !tui_confirm("Exit", "Return to the shell?");
      break;
    case TUI_MENU_INTERRUPTED:
      running = false;
      break;
    case TUI_MENU_TOO_SMALL:
    case TUI_MENU_INVALID_ARG:
      running = false;
      err = APP_ERROR_OUT_OF_RANGE;
      break;
    }
  }

  tui_destroy_window(menu_frame);
  tui_cleanup();
  return err;
}
