/*
 * Generic CLI Application Template
 *
 * A modern C23 TUI + CLI starter using Zig as the build system and C
 * toolchain. main() handles bootstrap (logging, config, arg parsing) and
 * defers command behaviour to the table in src/cli/commands.c.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <io.h>
#define isatty _isatty
#define fileno _fileno
#else
#include <unistd.h>
#endif

#include "cli/args.h"
#include "cli/commands.h"
#include "cli/help.h"
#include "core/config.h"
#include "core/error.h"
#include "io/output.h"
#include "utils/logging.h"

static int64_t app_now_millis(void) {
  struct timespec now;
  if (timespec_get(&now, TIME_UTC) != TIME_UTC) {
    return 0;
  }

  return (int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

static bool app_has_interactive_terminal(void) {
  return isatty(fileno(stdin)) && isatty(fileno(stdout));
}

static app_error initialize_app(int argc, char *argv[], app_config_t **config) {
  if (argc == 1) {
    app_print_concise_help(argv[0]);
    exit(0);
  }

  app_error err = app_args_handle_immediate_exit(argc, argv);
  if (err != APP_SUCCESS) {
    return err;
  }

  err = app_config_create(config);
  if (err != APP_SUCCESS) {
    return err;
  }

  // Layered configuration: file -> env -> args. CLI flags always win.
  const char *explicit_config_path = NULL;
  err = app_args_find_config_path(argc, argv, &explicit_config_path);
  if (err != APP_SUCCESS) {
    app_config_destroy(*config);
    return err;
  }

  err = app_config_load_file(*config, explicit_config_path);
  if (err != APP_SUCCESS) {
    const char *config_label =
        explicit_config_path ? explicit_config_path : "discovered config";
    fprintf(stderr, "Error: failed to load config '%s': %s\n", config_label,
            app_strerror(err));
    app_config_destroy(*config);
    return err;
  }
  err = app_config_load_env(*config);
  if (err != APP_SUCCESS) {
    app_config_destroy(*config);
    return err;
  }

  err = app_parse_args(argc, argv, *config);
  if (err != APP_SUCCESS) {
    app_config_destroy(*config);
    return err;
  }

  if (app_config_is_quiet(*config)) {
    app_log_set_level(LOG_LEVEL_ERROR);
  } else if (app_config_is_debug(*config)) {
    app_log_set_level(LOG_LEVEL_DEBUG);
    LOG_DEBUG("Debug mode enabled");
  } else if (app_config_is_verbose(*config)) {
    app_log_set_level(LOG_LEVEL_INFO);
  }

  return APP_SUCCESS;
}

int main(int argc, char *argv[]) {
  const int64_t start_ms = app_now_millis();

  app_log_init();

  app_config_t *config = NULL;
  app_error err = initialize_app(argc, argv, &config);
  if (err != APP_SUCCESS) {
    return err;
  }

  const char *command = app_config_get_command(config);
  if (command == NULL) {
    app_print_concise_help(argv[0]);
    app_config_destroy(config);
    return APP_ERROR_INVALID_ARG;
  }

  int cmd_argc = 0;
  char *const *cmd_argv = app_config_get_command_args(config, &cmd_argc);

  const app_command_t *entry = app_command_find(command);
  if (!entry) {
    app_output_format(config, true, "Unknown command: %s", command);
    app_output_format(config, true, "Run '%s --help' for available commands",
                      app_config_get_program_name(config));
    app_config_destroy(config);
    return APP_ERROR_INVALID_COMMAND;
  }

  // Scan for command-local --help/-h, but only before the first standalone
  // "--" delimiter. Tokens after "--" are positionals (matching
  // app_command_validate_invocation), so "myapp echo -- --help" must echo
  // "--help" rather than print help.
  for (int i = 0; i < cmd_argc; i++) {
    if (!cmd_argv[i]) {
      continue;
    }
    if (strcmp(cmd_argv[i], "--") == 0) {
      break;
    }
    if (strcmp(cmd_argv[i], "--help") == 0 || strcmp(cmd_argv[i], "-h") == 0) {
      app_print_command_help(app_config_get_program_name(config), entry);
      app_config_destroy(config);
      return APP_SUCCESS;
    }
  }

  if (entry->requires_terminal && !app_has_interactive_terminal()) {
    app_output_format(config, true,
                      "Command '%s' requires an interactive terminal", command);
    app_config_destroy(config);
    return APP_ERROR_IO;
  }

  err = app_command_validate_invocation(entry, cmd_argc, cmd_argv,
                                        app_config_get_program_name(config));
  if (err != APP_SUCCESS) {
    app_config_destroy(config);
    return err;
  }

  // app_config_get_command_args returns char *const * and handlers take
  // char *const argv[] (read-only argv vector), so no const-stripping cast.
  err = entry->handler(config, cmd_argc, cmd_argv);

  int64_t elapsed_ms = app_now_millis() - start_ms;
  if (elapsed_ms < 0) {
    elapsed_ms = 0;
  }
  LOG_INFO("Command '%s' completed in %ld ms with status %d", command,
           (long)elapsed_ms, err);

  app_config_destroy(config);

  return err;
}
