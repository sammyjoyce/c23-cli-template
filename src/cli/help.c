/*
 * Help text display.
 *
 * Both concise and verbose help iterate over the command and flag tables in
 * commands.c / config.c, so adding a command or flag automatically shows up
 * without editing this file.
 */

#include "help.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "../core/config.h"
#include "../core/types.h"
#include "../utils/colors.h"
#include "../utils/logging.h"
#include "commands.h"

static const char *program_or_default(const char *program_name) {
  if (program_name == nullptr || program_name[0] == '\0') {
    LOG_ERROR("Invalid program name");
    return APP_NAME;
  }
  return program_name;
}

static void print_commands_block(void) {
  size_t count = 0;
  const app_command_t *commands = app_commands(&count);
  printf("Commands:\n");
  for (size_t i = 0; i < count; i++) {
    printf("  %-16s%s\n", commands[i].name,
           commands[i].summary ? commands[i].summary : "");
  }
}

static void print_flag_options(void) {
  size_t count = 0;
  const app_flag_spec_t *flags = app_flag_table(&count);
  for (size_t i = 0; i < count; i++) {
    const app_flag_spec_t *spec = &flags[i];
    char left[64];
    if (spec->cli_short && spec->cli_long) {
      snprintf(left, sizeof(left), "%s, %s", spec->cli_short, spec->cli_long);
    } else if (spec->cli_long) {
      snprintf(left, sizeof(left), "%s", spec->cli_long);
    } else if (spec->cli_short) {
      snprintf(left, sizeof(left), "%s", spec->cli_short);
    } else {
      continue;
    }
    printf("  %-20s(boolean)\n", left);
  }
}

void app_print_concise_help(const char *program_name) {
  program_name = program_or_default(program_name);

  printf("%s - A C23 TUI + CLI starter [version %s]\n\n", APP_NAME,
         APP_VERSION);
  printf("Usage: %s [options] <command> [arguments]\n\n", program_name);

  print_commands_block();
  printf("\n");

  printf("Options:\n");
  printf("  -h, --help          Show this help message\n");
  printf("      --version       Show version information\n");
  print_flag_options();
  printf("  -c, --config PATH   Load configuration from PATH\n\n");

  printf("Examples:\n");
  printf("  $ %s hello\n", program_name);
  printf("  Hello, World!\n\n");

  printf("  $ %s hello Alice\n", program_name);
  printf("  Hello, Alice!\n\n");

  printf("For more options, run %s --help\n", program_name);
}

void app_print_verbose_usage(const char *program_name) {
  program_name = program_or_default(program_name);

  const char *bold = app_use_colors(nullptr) ? APP_COLOR_BOLD : "";
  const char *reset = app_use_colors(nullptr) ? APP_COLOR_RESET : "";

  printf("%s%s - A C23 TUI + CLI starter%s\n", bold, APP_NAME, reset);
  printf("Version %s\n\n", APP_VERSION);

  printf("%sUSAGE%s\n", bold, reset);
  printf("  %s [options] <command> [arguments]\n\n", program_name);

  printf("%sDESCRIPTION%s\n", bold, reset);
  printf(
      "  A ready-to-use C23 starter for command-line and ncurses TUI apps.\n");
  printf(
      "  This template provides a solid foundation for building "
      "command-line\n");
  printf("  tools with proper error handling, configuration, and testing.\n\n");

  printf("%sCOMMANDS%s\n", bold, reset);
  size_t cmd_count = 0;
  const app_command_t *commands = app_commands(&cmd_count);
  for (size_t i = 0; i < cmd_count; i++) {
    printf("  %-18s%s\n", commands[i].name,
           commands[i].summary ? commands[i].summary : "");
  }
  printf("\n");

  printf("%sOPTIONS%s\n", bold, reset);
  printf("  -h, --help          Show this help message and exit\n");
  printf("      --version       Show version information and exit\n");
  size_t flag_count = 0;
  const app_flag_spec_t *flags = app_flag_table(&flag_count);
  for (size_t i = 0; i < flag_count; i++) {
    const app_flag_spec_t *spec = &flags[i];
    char left[64];
    if (spec->cli_short && spec->cli_long) {
      snprintf(left, sizeof(left), "%s, %s", spec->cli_short, spec->cli_long);
    } else if (spec->cli_long) {
      snprintf(left, sizeof(left), "%s", spec->cli_long);
    } else if (spec->cli_short) {
      snprintf(left, sizeof(left), "%s", spec->cli_short);
    } else {
      continue;
    }
    const char *env_hint = spec->env_var ? spec->env_var : "";
    if (env_hint[0] != '\0') {
      printf("  %-20sboolean (env: %s)\n", left, env_hint);
    } else {
      printf("  %-20sboolean\n", left);
    }
  }
  printf("  -c, --config PATH   Specify configuration file path\n\n");

  printf("%sENVIRONMENT%s\n", bold, reset);
  printf(
      "  APP_LOG_LEVEL       Set logging level: ERROR, WARNING, INFO, DEBUG\n");
  printf("                      Default: ERROR\n");
  printf("  APP_CONFIG_PATH     Override the default config file lookup.\n");
  printf("  NO_COLOR            Disable colored output when set.\n\n");

  printf("%sCONFIGURATION%s\n", bold, reset);
  printf("  Loaded from (first hit wins):\n");
  printf("    - the path passed to --config\n");
  printf("    - $APP_CONFIG_PATH\n");
  printf("    - ~/.config/%s/config.json\n", APP_NAME);
  printf("    - /etc/%s/config.json\n\n", APP_NAME);

  printf("  Precedence (highest first):\n");
  printf("    1. Command-line arguments\n");
  printf("    2. Environment variables\n");
  printf("    3. Configuration file\n");
  printf("    4. Built-in defaults\n\n");

  printf("%sEXAMPLES%s\n", bold, reset);
  printf("  Basic greeting:\n");
  printf("    $ %s hello\n", program_name);
  printf("    Hello, World!\n\n");

  printf("  Machine-readable info:\n");
  printf("    $ %s --json info\n", program_name);
  printf("    {\"format_version\":\"1.0\", ...}\n\n");

  printf("  Run diagnostics:\n");
  printf("    $ %s doctor\n", program_name);
  printf("    $ %s doctor --deep    # also exercises the TUI runtime\n\n",
         program_name);

  printf("%sEXIT CODES%s\n", bold, reset);
  printf("  0    Success\n");
  printf("  1    General error\n");
  printf("  2    Invalid command or argument\n");
  printf("  3    Configuration error\n");
  printf("  10   Memory allocation error\n");
  printf("  11   I/O error\n");
  printf("  12   Permission denied\n\n");

  printf("%sAUTHOR%s\n", bold, reset);
  printf("  Written by Your Name\n\n");

  printf("%sREPORTING BUGS%s\n", bold, reset);
  printf(
      "  Report bugs to: "
      "https://github.com/yourusername/yourproject/issues\n\n");

  printf("%sSEE ALSO%s\n", bold, reset);
  printf("  Project homepage: https://github.com/yourusername/yourproject\n");
  printf(
      "  Documentation: https://github.com/yourusername/yourproject#readme\n");
}
