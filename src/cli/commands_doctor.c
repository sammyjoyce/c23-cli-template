/*
 * "doctor" command - emits a short readiness report.
 *
 * The interactive TUI runtime smoke test only runs when the user asks for it
 * via `doctor --deep`. Default `doctor` is non-destructive and never enters
 * ncurses, so it is safe to run mid-pipeline.
 */

#include <limits.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>
#define isatty _isatty
#define fileno _fileno
#else
#include <unistd.h>
#endif

#include "../core/config.h"
#include "../core/error.h"
#include "../core/types.h"
#include "../io/output.h"
#include "../utils/colors.h"
#ifdef ENABLE_TUI
#include "../tui/tui.h"
#endif
#include "commands.h"

app_error app_cmd_doctor(const app_config_t *config, int argc, char *const argv[]);

typedef struct {
  const char *name;
  const char *status;
  const char *detail;
  bool enabled;
  bool has_enabled;
} doctor_check_t;

static bool doctor_has_interactive_terminal(void) {
  return isatty(fileno(stdin)) && isatty(fileno(stdout));
}

static app_error doctor_tui_runtime_smoke(bool terminal_ready) {
#ifdef ENABLE_TUI
  if (!terminal_ready) {
    return APP_ERROR_IO;
  }

  const app_error err = tui_init();
  if (err != APP_SUCCESS) {
    return err;
  }
  if (!tui_terminal_meets_minimum()) {
    tui_cleanup();
    return APP_ERROR_OUT_OF_RANGE;
  }

  tui_cleanup();
  return APP_SUCCESS;
#else
  (void)terminal_ready;
  return APP_ERROR_FEATURE_BASE;
#endif
}

static void doctor_write_json_check(const doctor_check_t *check,
                                    bool *needs_comma) {
  if (*needs_comma) {
    fputc(',', stdout);
  }
  *needs_comma = true;

  bool field_comma = false;
  app_json_begin_object(stdout);
  app_json_write_string_field(stdout, "name", check->name, &field_comma);
  app_json_write_string_field(stdout, "status", check->status, &field_comma);
  app_json_write_string_field(stdout, "detail", check->detail, &field_comma);
  if (check->has_enabled) {
    app_json_write_bool_field(stdout, "enabled", check->enabled, &field_comma);
  }
  app_json_end_object(stdout);
}

app_error app_cmd_doctor(const app_config_t *config, int argc, char *const argv[]) {
  if (app_config_is_quiet(config)) {
    return APP_SUCCESS;
  }

  bool deep = false;
  const app_command_t *doctor_command = app_command_find("doctor");
  for (int i = 0; i < argc; i++) {
    const app_command_option_t *option =
        app_command_option_find(doctor_command, argv[i]);
    if (option && option->id == APP_COMMAND_OPTION_DOCTOR_DEEP) {
      deep = true;
    }
  }

  const bool tui_enabled =
#ifdef ENABLE_TUI
      true;
#else
      false;
#endif
  const bool color_enabled = app_use_colors(config);
  const bool git_known = strcmp(APP_GIT_COMMIT, "unknown") != 0;
  const bool terminal_ready = doctor_has_interactive_terminal();
  const char *config_file = app_config_get_config_file(config);
  const bool config_loaded = config_file && config_file[0] != '\0';

  app_error tui_runtime_err = APP_ERROR_IO;
  bool tui_runtime_ok = false;
  const char *tui_runtime_status = "skip";
  if (deep && tui_enabled && terminal_ready) {
    tui_runtime_err = doctor_tui_runtime_smoke(terminal_ready);
    tui_runtime_ok = (tui_runtime_err == APP_SUCCESS);
    tui_runtime_status = tui_runtime_ok ? "ok" : "warn";
  } else if (!deep) {
    tui_runtime_status = "skip";
  }

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
  if (!deep) {
    snprintf(tui_runtime_detail, sizeof(tui_runtime_detail), "%s",
             "pass --deep to exercise the TUI runtime");
  } else if (!tui_enabled) {
    snprintf(tui_runtime_detail, sizeof(tui_runtime_detail), "%s",
             "TUI support not compiled");
  } else if (!terminal_ready) {
    snprintf(tui_runtime_detail, sizeof(tui_runtime_detail), "%s",
             "runtime smoke skipped without a TTY");
  } else if (tui_runtime_ok) {
    snprintf(tui_runtime_detail, sizeof(tui_runtime_detail), "%s",
             "ncurses initialized successfully");
#ifdef ENABLE_TUI
  } else if (tui_runtime_err == APP_ERROR_OUT_OF_RANGE) {
    snprintf(tui_runtime_detail, sizeof(tui_runtime_detail),
             "terminal is too small (minimum %dx%d)", TUI_MIN_COLS,
             TUI_MIN_ROWS);
#endif
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

  const doctor_check_t checks[] = {
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

    app_json_begin_object(stdout);
    app_json_write_string_field(stdout, "format_version", "1.0", &root_comma);
    app_json_write_raw_field(stdout, "checks", "[", &root_comma);
    for (size_t i = 0; i < sizeof(checks) / sizeof(checks[0]); i++) {
      doctor_write_json_check(&checks[i], &check_comma);
    }
    fputc(']', stdout);
    app_json_end_object(stdout);
    app_json_end_line(stdout);
    return APP_SUCCESS;
  }

  app_output_format(config, false, "%s doctor", APP_NAME);
  for (size_t i = 0; i < sizeof(checks) / sizeof(checks[0]); i++) {
    app_output_format(config, false, "  %-13s %-4s (%s)", checks[i].name,
                      checks[i].status, checks[i].detail);
  }
  return APP_SUCCESS;
}
