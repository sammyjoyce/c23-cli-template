/*
 * Generic CLI Application Template
 *
 * A modern C23 TUI + CLI starter using Zig as the build system and C toolchain.
 * This template provides a solid foundation for building command-line tools
 * with proper error handling, configuration management, and testing support.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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
static _Thread_local int app_thread_id = 0;
#endif

static const char *app_bool_text(bool value) {
  return value ? "true" : "false";
}

static const char *app_yes_no(bool value) {
  return value ? "yes" : "no";
}

static int64_t app_now_millis(void) {
  struct timespec now;
  if (timespec_get(&now, TIME_UTC) != TIME_UTC) {
    return 0;
  }

  return (int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

static void print_info(const app_config_t *config) {
  const bool tui_enabled =
#ifdef ENABLE_TUI
      true;
#else
      false;
#endif

  if (app_config_is_json_output(config)) {
    printf(
        "{\"format_version\":\"1.0\",\"app\":\"%s\",\"version\":\"%s\","
        "\"git_commit\":\"%s\",\"build_date\":\"%s\",\"features\":{\"tui\":%s}}"
        "\n",
        APP_NAME, APP_VERSION, APP_GIT_COMMIT, APP_BUILD_DATE,
        app_bool_text(tui_enabled));
    return;
  }

  app_output_format(config, false, "Application: %s", APP_NAME);
  app_output_format(config, false, "Version: %s", APP_VERSION);
  app_output_format(config, false, "Git Commit: %s", APP_GIT_COMMIT);
  app_output_format(config, false, "Build: %s", APP_BUILD_DATE);
  app_output_format(config, false, "TUI Support: %s", app_yes_no(tui_enabled));
}

static void print_doctor(const app_config_t *config) {
  const bool tui_enabled =
#ifdef ENABLE_TUI
      true;
#else
      false;
#endif
  const bool color_enabled = app_use_colors(config);

  if (app_config_is_json_output(config)) {
    printf(
        "{\"format_version\":\"1.0\",\"checks\":["
        "{\"name\":\"binary\",\"status\":\"ok\",\"detail\":\"%s %s\"},"
        "{\"name\":\"tui_compiled\",\"status\":\"ok\",\"enabled\":%s},"
        "{\"name\":\"color_output\",\"status\":\"ok\",\"enabled\":%s},"
        "{\"name\":\"quiet_mode\",\"status\":\"ok\",\"enabled\":%s}"
        "]}\n",
        APP_NAME, APP_VERSION, app_bool_text(tui_enabled),
        app_bool_text(color_enabled),
        app_bool_text(app_config_is_quiet(config)));
    return;
  }

  app_output_format(config, false, "%s doctor", APP_NAME);
  app_output_format(config, false, "  binary        ok (%s %s)", APP_NAME,
                    APP_VERSION);
  app_output_format(config, false, "  git           %s", APP_GIT_COMMIT);
  app_output_format(config, false, "  tui           %s",
                    tui_enabled ? "compiled" : "not compiled");
  app_output_format(config, false, "  color         %s",
                    color_enabled ? "enabled" : "disabled");
  app_output_format(config, false, "  quiet         %s",
                    app_yes_no(app_config_is_quiet(config)));
  app_output_format(config, false, "  json          %s",
                    app_yes_no(app_config_is_json_output(config)));
}

static void output_joined_args(const app_config_t *config, int argc,
                               char *argv[]) {
  char buffer[4096] = {0};
  size_t used = 0;

  for (int i = 0; i < argc && used < sizeof(buffer) - 1; i++) {
    const char *separator = i == 0 ? "" : " ";
    const int written = snprintf(buffer + used, sizeof(buffer) - used, "%s%s",
                                 separator, argv[i] ? argv[i] : "");
    if (written < 0) {
      break;
    }
    if ((size_t)written >= sizeof(buffer) - used) {
      break;
    }
    used += (size_t)written;
  }

  app_output(buffer, config, false);
}

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
  if (strcmp(command, "hello") == 0) {
    const char *name = argc > 0 ? argv[0] : "World";
    app_output_format(config, false, "Hello, %s!", name);
    return APP_SUCCESS;
  }

  if (strcmp(command, "echo") == 0) {
    output_joined_args(config, argc, argv);
    return APP_SUCCESS;
  }

  if (strcmp(command, "info") == 0) {
    print_info(config);
    return APP_SUCCESS;
  }

  if (strcmp(command, "doctor") == 0) {
    print_doctor(config);
    return APP_SUCCESS;
  }

  if (strcmp(command, "menu") == 0) {
    if (app_config_is_json_output(config)) {
      app_output(
          "The menu command is interactive; remove --json to launch the "
          "TUI.",
          config, true);
      return APP_ERROR_INVALID_ARG;
    }

#ifdef ENABLE_TUI
    return tui_run_demo();
#else
    app_output(
        "TUI support is not compiled in. Rebuild with "
        "'zig build -Denable-tui=true'.",
        config, true);
    return APP_ERROR_CONFIG;
#endif
  }

  app_output_format(config, true, "Unknown command: %s", command);
  app_output_format(config, true, "Run '%s --help' for available commands",
                    app_config_get_program_name(config));
  return APP_ERROR_INVALID_COMMAND;
}

int main(int argc, char *argv[]) {
#if __STDC_VERSION__ >= 201112L
  (void)app_thread_id;  // Suppress unused variable warning
#endif

  const int64_t start_ms = app_now_millis();

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

  int64_t elapsed_ms = app_now_millis() - start_ms;
  if (elapsed_ms < 0) {
    elapsed_ms = 0;
  }
  LOG_INFO("Command '%s' completed in %ld ms with status %d", command,
           (long)elapsed_ms, err);

  // Cleanup
  app_config_destroy(config);

  return err;
}
