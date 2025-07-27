/*
 * Help text display implementation.
 */

#include "help.h"

#include <stdio.h>
#include <string.h>

#include "../core/types.h"
#include "../utils/colors.h"
#include "../utils/logging.h"

void app_print_concise_help(const char *program_name) {
  if (program_name == nullptr || strlen(program_name) == 0) {
    LOG_ERROR("Invalid program name");
    program_name = APP_NAME;
  }

  printf("%s - A modern CLI application [version %s]\n\n", APP_NAME,
         APP_VERSION);

  printf("Usage: %s [options] <command> [arguments]\n\n", program_name);

  printf("Commands:\n");
  printf("  hello [name]    Print a greeting message\n");
  printf("  echo [text...]  Echo the provided text\n");
  printf("  info            Display application information\n");
#ifdef ENABLE_TUI
  printf("  menu            Launch interactive TUI menu\n");
#endif
  printf("\n");

  printf("Options:\n");
  printf("  -h, --help      Show this help message\n");
  printf("  --version       Show version information\n");
  printf("  -d, --debug     Enable debug output\n");
  printf("  -q, --quiet     Suppress non-essential output\n\n");

  printf("Examples:\n");
  printf("  $ %s hello\n", program_name);
  printf("  Hello, World!\n\n");

  printf("  $ %s hello Alice\n", program_name);
  printf("  Hello, Alice!\n\n");

  printf("For more options, use %s --help\n", program_name);
}

void app_print_verbose_usage(const char *program_name) {
  if (program_name == nullptr || strlen(program_name) == 0) {
    LOG_ERROR("Invalid program name");
    program_name = APP_NAME;
  }

  const char *bold = app_use_colors(nullptr) ? APP_COLOR_BOLD : "";
  const char *reset = app_use_colors(nullptr) ? APP_COLOR_RESET : "";

  printf("%s%s - A modern CLI application%s\n", bold, APP_NAME, reset);
  printf("Version %s\n\n", APP_VERSION);

  printf("%sUSAGE%s\n", bold, reset);
  printf("  %s [options] <command> [arguments]\n\n", program_name);

  printf("%sDESCRIPTION%s\n", bold, reset);
  printf(
      "  A modern C23 CLI application template with comprehensive tooling.\n");
  printf(
      "  This template provides a solid foundation for building "
      "command-line\n");
  printf("  tools with proper error handling, configuration, and testing.\n\n");

  printf("%sCOMMANDS%s\n", bold, reset);
  printf("  hello [name]       Print a greeting message\n");
  printf("                     If no name is provided, greets 'World'\n\n");

  printf("  echo [text...]     Echo the provided text\n");
  printf("                     Prints all arguments separated by spaces\n\n");

  printf("  info               Display application information\n");
  printf(
      "                     Shows version, build date, and configuration\n\n");

#ifdef ENABLE_TUI
  printf("  menu               Launch interactive TUI menu\n");
  printf(
      "                     Opens an ncurses-based terminal UI with various "
      "options\n\n");
#endif

  printf("%sOPTIONS%s\n", bold, reset);
  printf("  -h, --help         Show this help message and exit\n");
  printf("  --version          Show version information and exit\n");
  printf("  -d, --debug        Enable debug output (DEBUG level logs)\n");
  printf("  -q, --quiet        Suppress non-essential output\n");
  printf("  -v, --verbose      Enable verbose output\n");
  printf("  --json             Output in JSON format\n");
  printf("  --plain            Output in plain text format\n");
  printf("  --no-color         Disable colored output\n");
  printf("  -c, --config PATH  Specify configuration file path\n\n");

  printf("%sENVIRONMENT%s\n", bold, reset);
  printf(
      "  APP_LOG_LEVEL      Set logging level: ERROR, WARNING, INFO, DEBUG\n");
  printf("                     Default: ERROR\n");
  printf("  NO_COLOR           Disable colored output when set\n\n");

  printf("%sCONFIGURATION%s\n", bold, reset);
  printf("  Configuration can be loaded from:\n");
  printf("  - ~/.config/%s/config.json (user-specific)\n", APP_NAME);
  printf("  - /etc/%s/config.json (system-wide)\n", APP_NAME);
  printf("  - Custom path via --config option\n\n");

  printf("  Configuration precedence (highest to lowest):\n");
  printf("  1. Command-line arguments\n");
  printf("  2. Environment variables\n");
  printf("  3. Configuration file\n");
  printf("  4. Default values\n\n");

  printf("%sEXAMPLES%s\n", bold, reset);
  printf("  Basic greeting:\n");
  printf("    $ %s hello\n", program_name);
  printf("    Hello, World!\n\n");

  printf("  Personalized greeting:\n");
  printf("    $ %s hello Alice\n", program_name);
  printf("    Hello, Alice!\n\n");

  printf("  Echo multiple words:\n");
  printf("    $ %s echo Hello from the CLI\n", program_name);
  printf("    Hello from the CLI\n\n");

  printf("  Show application info:\n");
  printf("    $ %s info\n", program_name);
  printf("    Application: %s\n", APP_NAME);
  printf("    Version: %s\n", APP_VERSION);
  printf("    Build: <timestamp>\n\n");

  printf("  Enable debug logging:\n");
  printf("    $ %s -d info\n", program_name);
  printf("    [DEBUG] Debug mode enabled\n");
  printf("    Application: %s\n", APP_NAME);
  printf("    ...\n\n");

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