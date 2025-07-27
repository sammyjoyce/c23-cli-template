/*
 * Command-line argument parsing implementation.
 */

#include "args.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../core/config.h"
#include "../utils/logging.h"
#include "help.h"

app_error app_parse_args(int argc, char *argv[], app_config_t *config) {
  CHECK_NULL(argv, APP_ERROR_INVALID_ARG);
  CHECK_NULL(config, APP_ERROR_INVALID_ARG);

  // Store program name
  app_config_set_program_name(config, argv[0]);

  // Check for help flag first
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
      app_print_verbose_usage(argv[0]);
      exit(0);
    }
  }

  // Check for version flag
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--version") == 0) {
      printf("%s %s\n", APP_NAME, APP_VERSION);
      printf("A modern C23 CLI application\n");
      printf("Built with: Zig, C23, Aro compiler\n");
      exit(0);
    }
  }

  // Parse options and find command
  int command_index = -1;

  for (int i = 1; i < argc; i++) {
    if (argv[i][0] == '-') {
      // Handle options
      if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--debug") == 0) {
        app_config_set_debug(config, true);
      } else if (strcmp(argv[i], "-q") == 0 ||
                 strcmp(argv[i], "--quiet") == 0) {
        app_config_set_quiet(config, true);
      } else if (strcmp(argv[i], "-v") == 0 ||
                 strcmp(argv[i], "--verbose") == 0) {
        app_config_set_verbose(config, true);
      } else if (strcmp(argv[i], "--json") == 0) {
        app_config_set_json_output(config, true);
      } else if (strcmp(argv[i], "--plain") == 0) {
        app_config_set_plain_output(config, true);
      } else if (strcmp(argv[i], "--no-color") == 0) {
        app_config_set_no_color(config, true);
      } else if (strcmp(argv[i], "-c") == 0 ||
                 strcmp(argv[i], "--config") == 0) {
        if (i + 1 >= argc) {
          fprintf(stderr, "Error: %s requires an argument\n", argv[i]);
          return APP_ERROR_MISSING_ARG;
        }
        app_config_set_config_file(config, argv[++i]);
      } else {
        fprintf(stderr, "Error: Unknown option '%s'\n", argv[i]);
        fprintf(stderr, "Run '%s --help' for usage information\n", argv[0]);
        return APP_ERROR_UNKNOWN_OPTION;
      }
    } else {
      // First non-option argument is the command
      command_index = i;
      break;
    }
  }

  // Set command and its arguments
  if (command_index >= 0) {
    app_config_set_command(config, argv[command_index]);

    // Add remaining arguments as command arguments
    for (int i = command_index + 1; i < argc; i++) {
      app_config_add_command_arg(config, argv[i]);
    }
  }

  return APP_SUCCESS;
}
