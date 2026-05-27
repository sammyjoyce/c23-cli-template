/*
 * Command-line argument parsing implementation.
 *
 * Bool flags are looked up against g_app_flag_table (see config.c) so adding a
 * new flag does not require a change here.
 */

#include "args.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../core/config.h"
#include "../utils/logging.h"
#include "help.h"

typedef struct {
  const char *config_path;
  int command_index;
} app_global_args_t;

static bool app_args_try_bool_flag(const char *arg, app_config_t *config) {
  size_t count = 0;
  const app_flag_spec_t *specs = app_flag_table(&count);
  for (size_t i = 0; i < count; i++) {
    const app_flag_spec_t *spec = &specs[i];
    const bool short_match =
        spec->cli_short && strcmp(arg, spec->cli_short) == 0;
    const bool long_match = spec->cli_long && strcmp(arg, spec->cli_long) == 0;
    if (short_match || long_match) {
      if (config) {
        app_config_set_flag(config, spec->id, true);
      }
      return true;
    }
  }
  return false;
}

static app_error app_scan_global_args(int argc, char *argv[],
                                      app_config_t *config,
                                      app_global_args_t *args) {
  CHECK_NULL(argv, APP_ERROR_INVALID_ARG);

  if (args) {
    args->config_path = NULL;
    args->command_index = -1;
  }

  for (int i = 1; i < argc; i++) {
    if (!argv[i] || argv[i][0] != '-') {
      if (args) {
        args->command_index = i;
      }
      break;
    }

    if (app_args_try_bool_flag(argv[i], config)) {
      continue;
    }

    if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--config") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr, "Error: %s requires an argument\n", argv[i]);
        return APP_ERROR_MISSING_ARG;
      }

      const char *config_path = argv[++i];
      if (args) {
        args->config_path = config_path;
      }
      if (config) {
        app_config_set_config_file(config, config_path);
      }
      continue;
    }

    fprintf(stderr, "Error: Unknown option '%s'\n", argv[i]);
    fprintf(stderr, "Run '%s --help' for usage information\n", argv[0]);
    return APP_ERROR_UNKNOWN_OPTION;
  }

  return APP_SUCCESS;
}

app_error app_args_handle_immediate_exit(int argc, char *argv[]) {
  CHECK_NULL(argv, APP_ERROR_INVALID_ARG);

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
      app_print_verbose_usage(argv[0]);
      exit(0);
    }
  }

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--version") == 0) {
      printf("%s %s\n", APP_NAME, APP_VERSION);
      printf("A C23 TUI + CLI starter application\n");
      printf("Built with: Zig, C23, ncurses/PDCurses\n");
      exit(0);
    }
  }

  return APP_SUCCESS;
}

app_error app_args_find_config_path(int argc, char *argv[],
                                    const char **config_path) {
  CHECK_NULL(config_path, APP_ERROR_INVALID_ARG);

  app_global_args_t args;
  const app_error err = app_scan_global_args(argc, argv, NULL, &args);
  if (err != APP_SUCCESS) {
    return err;
  }

  *config_path = args.config_path;
  return APP_SUCCESS;
}

app_error app_parse_args(int argc, char *argv[], app_config_t *config) {
  CHECK_NULL(argv, APP_ERROR_INVALID_ARG);
  CHECK_NULL(config, APP_ERROR_INVALID_ARG);

  // Store program name
  app_config_set_program_name(config, argv[0]);

  app_error err = app_args_handle_immediate_exit(argc, argv);
  if (err != APP_SUCCESS) {
    return err;
  }

  app_global_args_t args;
  err = app_scan_global_args(argc, argv, config, &args);
  if (err != APP_SUCCESS) {
    return err;
  }

  // Set command and its arguments
  if (args.command_index >= 0) {
    app_config_set_command(config, argv[args.command_index]);

    // Add remaining arguments as command arguments
    for (int i = args.command_index + 1; i < argc; i++) {
      app_config_add_command_arg(config, argv[i]);
    }
  }

  return APP_SUCCESS;
}
