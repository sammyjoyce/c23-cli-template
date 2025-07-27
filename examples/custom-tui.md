# Example: Creating a Custom TUI Screen

This example shows how to create a custom TUI screen using the ncurses wrapper.

## 1. Create Your TUI Function

Add a new function in your command handler or create a separate file:

```c
#ifdef ENABLE_TUI
static app_error show_data_viewer(void) {
    // Initialize TUI if not already done
    app_error err = tui_init();
    if (err != APP_SUCCESS) {
        return err;
    }

    // Get terminal dimensions
    int max_y = tui_get_max_y();
    int max_x = tui_get_max_x();

    // Create main window (leave space for borders)
    tui_window_t *main_win = tui_create_window(max_y - 2, max_x - 2, 1, 1);
    if (!main_win) {
        tui_cleanup();
        return APP_ERROR_MEMORY;
    }

    tui_draw_border(main_win);
    tui_set_window_title(main_win, "Data Viewer");

    // Create a data display area
    tui_window_t *data_win = tui_create_window(max_y - 10, max_x - 10, 5, 5);
    if (!data_win) {
        tui_destroy_window(main_win);
        tui_cleanup();
        return APP_ERROR_MEMORY;
    }

    tui_draw_border(data_win);
    tui_set_window_title(data_win, "Live Data");

    // Main loop
    bool running = true;
    int data_value = 0;

    while (running) {
        // Clear data window content
        tui_clear_window(data_win);

        // Display some data
        tui_set_color(data_win->win, TUI_COLOR_INFO);
        mvwprintw(data_win->win, 2, 2, "Current Value: %d", data_value);
        wattroff(data_win->win, COLOR_PAIR(TUI_COLOR_INFO));

        // Draw a simple bar graph
        int bar_width = (data_win->width - 4) * data_value / 100;
        mvwprintw(data_win->win, 4, 2, "[");
        tui_set_color(data_win->win, TUI_COLOR_SUCCESS);
        for (int i = 0; i < bar_width; i++) {
            waddch(data_win->win, '=');
        }
        wattroff(data_win->win, COLOR_PAIR(TUI_COLOR_SUCCESS));
        mvwprintw(data_win->win, 4, 3 + bar_width, "]");

        // Instructions
        tui_set_color(main_win->win, TUI_COLOR_INFO);
        mvwprintw(main_win->win, max_y - 4, 2,
                  "Press: [+] Increase  [-] Decrease  [r] Reset  [q] Quit");
        wattroff(main_win->win, COLOR_PAIR(TUI_COLOR_INFO));

        // Refresh windows
        tui_refresh_window(data_win);
        tui_refresh_window(main_win);

        // Handle input
        int ch = tui_get_char();
        switch (ch) {
            case '+':
            case '=':
                if (data_value < 100) data_value += 5;
                break;
            case '-':
            case '_':
                if (data_value > 0) data_value -= 5;
                break;
            case 'r':
            case 'R':
                data_value = 0;
                break;
            case 'q':
            case 'Q':
            case 27:  // ESC
                running = false;
                break;
        }
    }

    // Cleanup
    tui_destroy_window(data_win);
    tui_destroy_window(main_win);
    tui_cleanup();

    return APP_SUCCESS;
}
#endif
```

## 2. Add Command Handler

In your `handle_command` function:

```c
if (strcmp(command, "viewer") == 0) {
#ifdef ENABLE_TUI
    return show_data_viewer();
#else
    fprintf(stderr, "Error: TUI support not compiled in\n");
    fprintf(stderr, "Rebuild with: zig build -Denable-tui=true\n");
    return APP_ERROR_UNSUPPORTED;
#endif
}
```

## 3. Using TUI Components

### Message Boxes

```c
tui_show_message("Success", "Operation completed successfully!");
```

### Confirmation Dialogs

```c
if (tui_confirm("Delete File", "Are you sure you want to delete this file?")) {
    // Perform deletion
}
```

### Input Dialogs

```c
char username[256] = {0};
if (tui_input_dialog("Login", "Enter username:", username, sizeof(username)) == APP_SUCCESS) {
    printf("Welcome, %s!\n", username);
}
```

### Progress Bars

```c
tui_progress_t *progress = tui_progress_create("Processing Files", 100);
for (int i = 0; i <= 100; i++) {
    char status[64];
    snprintf(status, sizeof(status), "Processing file %d of 100", i);
    tui_progress_update(progress, i, status);
    usleep(50000);  // Simulate work
}
tui_progress_destroy(progress);
```

### Menus

```c
tui_menu_item_t options[] = {
    {"Option 1", "First option description", 1, true},
    {"Option 2", "Second option description", 2, true},
    {"Disabled", "This option is disabled", 3, false},
    {"Exit", "Return to main menu", 0, true}
};

int choice = tui_show_menu(window, "Select Option", options, 4, 0);
switch (choice) {
    case 1: /* Handle option 1 */ break;
    case 2: /* Handle option 2 */ break;
    case 0: /* Exit */ break;
    case -1: /* User cancelled */ break;
}
```

## 4. Color Usage

The TUI wrapper provides predefined color pairs:

- `TUI_COLOR_DEFAULT` - Default terminal colors
- `TUI_COLOR_HIGHLIGHT` - Highlighted text
- `TUI_COLOR_ERROR` - Error messages (red)
- `TUI_COLOR_SUCCESS` - Success messages (green)
- `TUI_COLOR_WARNING` - Warnings (yellow)
- `TUI_COLOR_INFO` - Information (cyan)
- `TUI_COLOR_MENU_SELECTED` - Selected menu items
- `TUI_COLOR_MENU_NORMAL` - Normal menu items
- `TUI_COLOR_BORDER` - Window borders (blue)
- `TUI_COLOR_TITLE` - Window titles (magenta)

Use them like:

```c
tui_set_color(window->win, TUI_COLOR_ERROR);
mvwprintw(window->win, y, x, "Error: %s", error_message);
wattroff(window->win, COLOR_PAIR(TUI_COLOR_ERROR));
```

## 5. Best Practices

1. **Always check initialization**: TUI functions will fail if not initialized
2. **Clean up properly**: Always call `tui_cleanup()` before exiting
3. **Handle terminal resize**: Consider refreshing on `KEY_RESIZE`
4. **Provide keyboard shortcuts**: Make UI keyboard-friendly
5. **Show help**: Display available keys/commands
6. **Test without TUI**: Ensure your app works with `-Denable-tui=false`

## 6. Error Handling

Always handle TUI errors gracefully:

```c
app_error err = tui_init();
if (err != APP_SUCCESS) {
    // Fall back to non-TUI interface
    fprintf(stderr, "Failed to initialize TUI, falling back to CLI\n");
    return handle_cli_version();
}
```

This ensures your application remains functional even if the terminal doesn't support ncurses.
