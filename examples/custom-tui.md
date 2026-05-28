# Example: Creating a Custom TUI Screen

The terminal UI is opt-in: it only compiles with `-Denable-tui=true` (which defines `ENABLE_TUI`). `tui.h` gives you a window wrapper, dialogs, a progress bar, and one modal menu primitive, so command code never calls ncurses directly. Every screen follows the same rule: call `tui_init()` first, and `tui_cleanup()` before you return, on every path.

## 1. A complete screen

This handler opens a bordered window, draws a live value with a bar, and loops on keypresses. It drops to raw ncurses (`mvwprintw`, `waddch`) for the drawing, which is fine; the wrapper manages lifecycle and layout, not every cell.

```c
#ifdef ENABLE_TUI
static app_error show_data_viewer(void) {
    app_error err = tui_init();
    if (err != APP_SUCCESS) {
        return err;
    }

    int max_y = tui_get_max_y();
    int max_x = tui_get_max_x();

    tui_window_t *main_win = tui_create_window(max_y - 2, max_x - 2, 1, 1);
    if (!main_win) {
        tui_cleanup();
        return APP_ERROR_MEMORY;
    }
    tui_draw_border(main_win);
    tui_set_window_title(main_win, "Data Viewer");

    tui_window_t *data_win = tui_create_window(max_y - 10, max_x - 10, 5, 5);
    if (!data_win) {
        tui_destroy_window(main_win);
        tui_cleanup();
        return APP_ERROR_MEMORY;
    }
    tui_draw_border(data_win);
    tui_set_window_title(data_win, "Live Data");

    bool running = true;
    int data_value = 0;

    while (running) {
        tui_clear_window(data_win);

        tui_set_color(data_win->win, TUI_COLOR_INFO);
        mvwprintw(data_win->win, 2, 2, "Current Value: %d", data_value);
        tui_unset_color(data_win->win, TUI_COLOR_INFO);

        int bar_width = (data_win->width - 4) * data_value / 100;
        mvwprintw(data_win->win, 4, 2, "[");
        tui_set_color(data_win->win, TUI_COLOR_SUCCESS);
        for (int i = 0; i < bar_width; i++) {
            waddch(data_win->win, '=');
        }
        tui_unset_color(data_win->win, TUI_COLOR_SUCCESS);
        mvwprintw(data_win->win, 4, 3 + bar_width, "]");

        mvwprintw(main_win->win, max_y - 4, 2,
                  "Press: [+] Increase  [-] Decrease  [r] Reset  [q] Quit");

        tui_refresh_window(data_win);
        tui_refresh_window(main_win);

        switch (tui_get_char()) {
            case '+': case '=': if (data_value < 100) data_value += 5; break;
            case '-': case '_': if (data_value > 0)   data_value -= 5; break;
            case 'r': case 'R': data_value = 0; break;
            case 'q': case 'Q': case 27 /* ESC */: running = false; break;
        }
    }

    tui_destroy_window(data_win);
    tui_destroy_window(main_win);
    tui_cleanup();
    return APP_SUCCESS;
}
#endif
```

## 2. Wire it to a command

Register a handler in `src/cli/commands.c` with `.requires_terminal = true`, and guard the TUI call so a non-TUI build still gives a clean message instead of failing to link:

```c
app_error app_cmd_viewer(const app_config_t *config, int argc, char **argv) {
    (void)config;
    (void)argc;
    (void)argv;
#ifdef ENABLE_TUI
    return show_data_viewer();
#else
    fprintf(stderr, "Error: TUI support not compiled in.\n");
    fprintf(stderr, "Rebuild with: zig build -Denable-tui=true\n");
    return APP_ERROR_CONFIG;
#endif
}
```

`.requires_terminal = true` makes help and `myapp opencli` mark the command as interactive. See [adding-a-command.md](adding-a-command.md) for the full registration steps.

## 3. Building blocks

### Dialogs

```c
tui_show_message("Success", "Operation completed.");

if (tui_confirm("Delete File", "Are you sure?")) {
    // tui_confirm returns bool
}

char username[256] = {0};
if (tui_input_dialog("Login", "Enter username:", username, sizeof(username)) == APP_SUCCESS) {
    // username is filled in
}
```

### Progress bar

```c
tui_progress_t *progress = tui_progress_create("Processing", 100);
for (int i = 0; i <= 100; i++) {
    char status[64];
    snprintf(status, sizeof(status), "Item %d of 100", i);
    tui_progress_update(progress, i, status);
    napms(50);  // stand-in for real work
}
tui_progress_destroy(progress);
```

### Menu

The menu is the one reusable primitive that is also shipped as a library (`tui-menu-lib`) and covered by the [TUI menu contract](../docs/CONTRACTS.md#tui-menu-contract). All pointers in the config must outlive the call; the menu copies nothing.

```c
enum { MENU_OPTION_ONE = 1, MENU_OPTION_TWO, MENU_EXIT };

const tui_menu_item_t options[] = {
    {.label = "Option &1", .description = "First option",  .id = MENU_OPTION_ONE},
    {.label = "Option &2", .description = "Second option", .id = MENU_OPTION_TWO},
    {.label = "&Disabled", .description = "Unavailable",   .id = 3, .disabled = true},
    {.label = "E&xit",     .description = "Leave the menu", .id = MENU_EXIT},
};

tui_menu_result_t result = tui_show_menu(
    NULL,  // NULL => the menu owns its frame and handles resize
    &(tui_menu_config_t){
        .title = "Select Option",
        .items = options,
        .item_count = (int)(sizeof(options) / sizeof(options[0])),
        .default_index = 0,
        .enable_search = true,
        .show_detail_pane = true,
    });

if (result.status == TUI_MENU_OK) {
    switch (result.selected_id) {
        case MENU_OPTION_ONE: /* ... */ break;
        case MENU_OPTION_TWO: /* ... */ break;
        case MENU_EXIT:       /* ... */ break;
    }
}
```

`&` in a label marks the mnemonic key (`&&` is a literal `&`). `result.status` is one of `TUI_MENU_OK`, `TUI_MENU_CANCELLED`, `TUI_MENU_INTERRUPTED`, `TUI_MENU_TOO_SMALL`, `TUI_MENU_INVALID_ARG`, or `TUI_MENU_NO_MEMORY`.

## 4. Colors

Set a color pair before drawing and unset it after. The pairs are roles, not fixed colors. The theme is defined in `src/utils/colors.c`:

```c
tui_set_color(window->win, TUI_COLOR_ERROR);
mvwprintw(window->win, y, x, "Error: %s", message);
tui_unset_color(window->win, TUI_COLOR_ERROR);
```

Available pairs: `TUI_COLOR_DEFAULT`, `HIGHLIGHT`, `ERROR`, `SUCCESS`, `WARNING`, `INFO`, `MENU_SELECTED`, `MENU_NORMAL`, `BORDER`, `TITLE`, `ACCENT`, `DIM`.

## 5. Practices

- **Pair every `tui_init()` with `tui_cleanup()`**, including on every error path, or the terminal is left in raw mode.
- **Handle a failed init.** `tui_init()` returns an `app_error`; fall back to non-interactive output rather than aborting:

  ```c
  if (tui_init() != APP_SUCCESS) {
      fprintf(stderr, "Terminal does not support the TUI; using plain output.\n");
      return run_plain_version();
  }
  ```

- **Respect interrupts.** Check `tui_interrupted()` in long loops and exit cleanly.
- **Test both builds.** Confirm `zig build` (no TUI) and `zig build -Denable-tui=true` both compile and behave. Drive TUI screens with the scenario harness in [TESTING.md](../docs/TESTING.md).

## See also

- [Public Contracts](../docs/CONTRACTS.md#tui-menu-contract) - the stable menu surface
- [Architecture Overview](../docs/ARCHITECTURE.md) - where `tui/` sits in the codebase
