/*
 * Generic CLI Application Template
 *
 * A modern C23 TUI + CLI starter using Zig as the build system and C toolchain.
 * This template provides a solid foundation for building command-line tools
 * with proper error handling, configuration management, and testing support.
 */

#include <limits.h>
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

static const char *app_yes_no(bool value) {
  return value ? "yes" : "no";
}

typedef struct {
  const char *name;
  const char *status;
  const char *detail;
  bool enabled;
  bool has_enabled;
} app_doctor_check_t;

static bool app_has_interactive_terminal(void) {
  return isatty(fileno(stdin)) && isatty(fileno(stdout));
}

static app_error app_doctor_tui_runtime_smoke(bool terminal_ready) {
#ifdef ENABLE_TUI
  if (!terminal_ready) {
    return APP_ERROR_IO;
  }

  const app_error err = tui_init();
  if (err == APP_SUCCESS) {
    tui_cleanup();
  }
  return err;
#else
  (void)terminal_ready;
  return APP_ERROR_FEATURE_BASE;
#endif
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
    bool root_comma = false;
    bool feature_comma = false;

    fputc('{', stdout);
    app_json_write_string_field(stdout, "format_version", "1.0", &root_comma);
    app_json_write_string_field(stdout, "app", APP_NAME, &root_comma);
    app_json_write_string_field(stdout, "version", APP_VERSION, &root_comma);
    app_json_write_string_field(stdout, "git_commit", APP_GIT_COMMIT,
                                &root_comma);
    app_json_write_string_field(stdout, "build_date", APP_BUILD_DATE,
                                &root_comma);
    app_json_write_raw_field(stdout, "features", "{", &root_comma);
    app_json_write_bool_field(stdout, "tui", tui_enabled, &feature_comma);
    fputs("}}\n", stdout);
    return;
  }

  app_output_format(config, false, "Application: %s", APP_NAME);
  app_output_format(config, false, "Version: %s", APP_VERSION);
  app_output_format(config, false, "Git Commit: %s", APP_GIT_COMMIT);
  app_output_format(config, false, "Build: %s", APP_BUILD_DATE);
  app_output_format(config, false, "TUI Support: %s", app_yes_no(tui_enabled));
}

static void print_doctor_json_check(const app_doctor_check_t *check,
                                    bool *needs_comma) {
  if (!check || !needs_comma) {
    return;
  }

  if (*needs_comma) {
    fputc(',', stdout);
  }
  *needs_comma = true;

  bool field_comma = false;
  fputc('{', stdout);
  app_json_write_string_field(stdout, "name", check->name, &field_comma);
  app_json_write_string_field(stdout, "status", check->status, &field_comma);
  app_json_write_string_field(stdout, "detail", check->detail, &field_comma);
  if (check->has_enabled) {
    app_json_write_bool_field(stdout, "enabled", check->enabled, &field_comma);
  }
  fputc('}', stdout);
}

static void print_doctor(const app_config_t *config) {
  const bool tui_enabled =
#ifdef ENABLE_TUI
      true;
#else
      false;
#endif
  const bool color_enabled = app_use_colors(config);
  const bool git_known = strcmp(APP_GIT_COMMIT, "unknown") != 0;
  const bool terminal_ready = app_has_interactive_terminal();
  const char *config_file = app_config_get_config_file(config);
  const bool config_loaded = config_file && config_file[0] != '\0';
  const app_error tui_runtime_err =
      tui_enabled && terminal_ready
          ? app_doctor_tui_runtime_smoke(terminal_ready)
          : APP_ERROR_IO;
  const bool tui_runtime_ok =
      tui_enabled && terminal_ready && tui_runtime_err == APP_SUCCESS;
  const char *tui_runtime_status =
      tui_runtime_ok ? "ok" : (tui_enabled && terminal_ready ? "warn" : "skip");

  char binary_detail[128];
  char git_detail[128];
  char tui_compiled_detail[96];
  char tui_terminal_detail[96];
  char tui_runtime_detail[128];
  char color_detail[96];
  char config_detail[PATH_MAX];
  char quiet_detail[32];
  char json_detail[32];

  snprintf(binary_detail, sizeof(binary_detail), "%s %s", APP_NAME,
           APP_VERSION);
  snprintf(git_detail, sizeof(git_detail), "%s",
           git_known ? APP_GIT_COMMIT : "commit metadata unavailable");
  snprintf(tui_compiled_detail, sizeof(tui_compiled_detail), "%s",
           tui_enabled ? "compiled with TUI support"
                       : "rebuild with -Denable-tui=true");
  snprintf(tui_terminal_detail, sizeof(tui_terminal_detail), "%s",
           terminal_ready ? "stdin/stdout are TTYs" : "stdin/stdout not TTYs");
  if (!tui_enabled) {
    snprintf(tui_runtime_detail, sizeof(tui_runtime_detail), "%s",
             "TUI support not compiled");
  } else if (!terminal_ready) {
    snprintf(tui_runtime_detail, sizeof(tui_runtime_detail), "%s",
             "runtime smoke skipped without a TTY");
  } else if (tui_runtime_ok) {
    snprintf(tui_runtime_detail, sizeof(tui_runtime_detail), "%s",
             "ncurses initialized successfully");
  } else {
    snprintf(tui_runtime_detail, sizeof(tui_runtime_detail), "%s",
             app_strerror(tui_runtime_err));
  }
  snprintf(color_detail, sizeof(color_detail), "%s",
           color_enabled ? "enabled" : "disabled for this output");
  snprintf(config_detail, sizeof(config_detail), "%s",
           config_loaded ? config_file : "no config file loaded");
  snprintf(quiet_detail, sizeof(quiet_detail), "%s",
           app_yes_no(app_config_is_quiet(config)));
  snprintf(json_detail, sizeof(json_detail), "%s",
           app_yes_no(app_config_is_json_output(config)));

  const app_doctor_check_t checks[] = {
      {"binary", "ok", binary_detail, false, false},
      {"git", git_known ? "ok" : "warn", git_detail, false, false},
      {"tui_compiled", tui_enabled ? "ok" : "warn", tui_compiled_detail,
       tui_enabled, true},
      {"tui_terminal", terminal_ready ? "ok" : "warn", tui_terminal_detail,
       terminal_ready, true},
      {"tui_runtime", tui_runtime_status, tui_runtime_detail, tui_runtime_ok,
       true},
      {"color_output", color_enabled ? "ok" : "warn", color_detail,
       color_enabled, true},
      {"config_file", config_loaded ? "ok" : "skip", config_detail,
       config_loaded, true},
      {"quiet_mode", "ok", quiet_detail, app_config_is_quiet(config), true},
      {"json_output", "ok", json_detail, app_config_is_json_output(config),
       true},
  };

  if (app_config_is_json_output(config)) {
    bool root_comma = false;
    bool check_comma = false;

    fputc('{', stdout);
    app_json_write_string_field(stdout, "format_version", "1.0", &root_comma);
    app_json_write_raw_field(stdout, "checks", "[", &root_comma);
    for (size_t i = 0; i < sizeof(checks) / sizeof(checks[0]); i++) {
      print_doctor_json_check(&checks[i], &check_comma);
    }
    fputs("]}\n", stdout);
    return;
  }

  app_output_format(config, false, "%s doctor", APP_NAME);
  for (size_t i = 0; i < sizeof(checks) / sizeof(checks[0]); i++) {
    app_output_format(config, false, "  %-13s %-4s (%s)", checks[i].name,
                      checks[i].status, checks[i].detail);
  }
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

static const char *find_config_path_arg(int argc, char *argv[]) {
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--config") == 0) {
      if (i + 1 < argc) {
        return argv[i + 1];
      }
      return NULL;
    }
  }

  return NULL;
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

  // Load configuration from lower-precedence sources before parsing args.
  const char *explicit_config_path = find_config_path_arg(argc, argv);
  err = app_config_load_file(*config, explicit_config_path);
  if (err != APP_SUCCESS) {
    if (explicit_config_path) {
      app_config_destroy(*config);
      return err;
    }
    LOG_WARNING("Ignoring invalid default configuration");
  }
  err = app_config_load_env(*config);
  if (err != APP_SUCCESS) {
    app_config_destroy(*config);
    return err;
  }

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
