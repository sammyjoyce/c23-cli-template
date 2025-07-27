/*
 * Generic CLI Application Template
 *
 * A modern C23 CLI application starter using Zig build system and Aro compiler.
 * This template provides a solid foundation for building command-line tools
 * with proper error handling, configuration management, and testing support.
 */

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "cli/args.h"
#include "cli/help.h"
#include "core/config.h"
#include "core/error.h"
#include "core/types.h"
#include "io/input.h"
#include "io/output.h"
#ifdef ENABLE_TUI
#include "tui/tui.h"
#endif
#include "utils/colors.h"
#include "utils/logging.h"
#include "utils/memory.h"

#if __STDC_VERSION__ >= 201112L
static _Thread_local int thread_id = 0;
#endif

static app_error initialize_app(int argc, char *argv[], app_config_t **config) {
  // Show help if no arguments
  if (argc == 1) {
    app_print_concise_help(argv[0]);
    exit(0);
  }

  // Create configuration
  app_error err = app_config_create(config);
  if (err != APP_SUCCESS) {
    return err;
  }

  // Load configuration from various sources
  (void)app_config_load_file(*config, NULL);  // Load from default locations
  (void)app_config_load_env(*config);

  // Parse command line arguments (may exit for --help or --version)
  err = app_parse_args(argc, argv, *config);
  if (err != APP_SUCCESS) {
    app_config_destroy(*config);
    return err;
  }

  // Set up debug logging if requested
  if (app_config_is_debug(*config)) {
    app_log_set_level(LOG_LEVEL_DEBUG);
    LOG_DEBUG("Debug mode enabled");
  }

  return APP_SUCCESS;
}

static app_error handle_command(const app_config_t *config, const char *command,
                                int argc, char *argv[]) {
  // Example command handling
  if (strcmp(command, "hello") == 0) {
    const char *name = argc > 0 ? argv[0] : "World";
    printf("Hello, %s!\n", name);
    return APP_SUCCESS;
  }

  if (strcmp(command, "echo") == 0) {
    for (int i = 0; i < argc; i++) {
      printf("%s%s", argv[i], i < argc - 1 ? " " : "\n");
    }
    return APP_SUCCESS;
  }

  if (strcmp(command, "info") == 0) {
    printf("Application: %s\n", APP_NAME);
    printf("Version: %s\n", APP_VERSION);
    printf("Build: %s\n", __DATE__ " " __TIME__);
    return APP_SUCCESS;
  }

#ifdef ENABLE_TUI
  if (strcmp(command, "menu") == 0) {
    // Interactive menu example using ncurses
    app_error err = tui_init();
    if (err != APP_SUCCESS) {
      fprintf(stderr, "Failed to initialize TUI\n");
      return err;
    }

    // Create main menu items
    tui_menu_item_t main_menu[] = {
        {"File Operations", "Create, read, or modify files", 1, true},
        {"System Information", "View system and application info", 2, true},
        {"Settings", "Configure application settings", 3, true},
        {"Run Tests", "Execute test suite", 4, true},
        {"About", "About this application", 5, true},
        {"Exit", "Exit the application", 0, true}};

    // Create centered window for menu
    int max_y = tui_get_max_y();
    int max_x = tui_get_max_x();
    int width = 60;
    int height = 20;
    int y = (max_y - height) / 2;
    int x = (max_x - width) / 2;

    tui_window_t *menu_window = tui_create_window(height, width, y, x);
    if (!menu_window) {
      tui_cleanup();
      return APP_ERROR_MEMORY;
    }

    tui_draw_border(menu_window);
    tui_set_window_title(menu_window, "Main Menu");

    bool running = true;
    while (running) {
      int choice =
          tui_show_menu(menu_window, "Select an option:", main_menu, 6, 0);

      switch (choice) {
      case 1:
        tui_show_message("File Operations",
                         "File operations would be implemented here.\n\n"
                         "This could include:\n"
                         "• Create new files\n"
                         "• Read existing files\n"
                         "• Edit file contents\n"
                         "• Delete files");
        break;

      case 2: {
        char info_msg[512];
        snprintf(info_msg, sizeof(info_msg),
                 "Application: %s\n"
                 "Version: %s\n"
                 "Build Date: %s\n"
                 "Terminal Size: %dx%d\n"
                 "Colors Supported: %s",
                 APP_NAME, APP_VERSION, __DATE__ " " __TIME__, max_x, max_y,
                 has_colors() ? "Yes" : "No");
        tui_show_message("System Information", info_msg);
        break;
      }

      case 3: {
        char input_buffer[256] = {0};
        if (tui_input_dialog("Settings", "Enter your name:", input_buffer,
                             sizeof(input_buffer)) == APP_SUCCESS) {
          char msg[512];
          snprintf(msg, sizeof(msg),
                   "Hello, %s!\n\nYour settings have been saved.",
                   input_buffer);
          tui_show_message("Settings Updated", msg);
        }
        break;
      }

      case 4: {
        // Show progress bar example
        tui_progress_t *progress = tui_progress_create("Running Tests", 100);
        if (progress) {
          for (int i = 0; i <= 100; i += 10) {
            char status[64];
            snprintf(status, sizeof(status), "Running test %d of 10...",
                     i / 10 + 1);
            tui_progress_update(progress, i, status);
            usleep(100000);  // 100ms delay for demo
          }
          tui_progress_destroy(progress);
          tui_show_message("Tests Complete", "All tests passed successfully!");
        }
        break;
      }

      case 5:
        tui_show_message("About",
                         "CLI Application Template\n\n"
                         "A modern C23 application with:\n"
                         "• NCurses TUI support\n"
                         "• Zig build system\n"
                         "• Comprehensive error handling\n"
                         "• Configuration management\n\n"
                         "Built with ❤️ for developers");
        break;

      case 0:
      case -1:  // User pressed 'q' or ESC
        if (tui_confirm("Exit", "Are you sure you want to exit?")) {
          running = false;
        }
        break;
      }
    }

    tui_destroy_window(menu_window);
    tui_cleanup();
    return APP_SUCCESS;
  }
#endif

  fprintf(stderr, "Unknown command: %s\n", command);
  fprintf(stderr, "Run '%s --help' for available commands\n",
          app_config_get_program_name(config));
  return APP_ERROR_INVALID_COMMAND;
}

int main(int argc, char *argv[]) {
#if __STDC_VERSION__ >= 201112L
  (void)thread_id;  // Suppress unused variable warning
#endif

  // Start performance timing
  struct timespec start_time, end_time;
  clock_gettime(CLOCK_MONOTONIC, &start_time);

  // Initialize logging
  app_log_init();

  // Initialize configuration
  app_config_t *config = NULL;
  app_error err = initialize_app(argc, argv, &config);
  if (err != APP_SUCCESS) {
    return err;
  }

  // Get command from arguments
  const char *command = app_config_get_command(config);
  if (command == NULL) {
    app_print_concise_help(argv[0]);
    app_config_destroy(config);
    return APP_ERROR_INVALID_ARG;
  }

  // Handle the command
  int cmd_argc = 0;
  char **cmd_argv = app_config_get_command_args(config, &cmd_argc);
  err = handle_command(config, command, cmd_argc, cmd_argv);

  // Calculate total processing time
  clock_gettime(CLOCK_MONOTONIC, &end_time);
  int64_t elapsed_ms = (end_time.tv_sec - start_time.tv_sec) * 1000 +
                       (end_time.tv_nsec - start_time.tv_nsec) / 1000000;
  LOG_INFO("Command '%s' completed in %ld ms with status %d", command,
           (long)elapsed_ms, err);

  // Cleanup
  app_config_destroy(config);

  return err;
}