/*
 * "info" command - prints build metadata in plain or JSON form.
 */

#include <stdio.h>

#include "../core/config.h"
#include "../core/error.h"
#include "../core/types.h"
#include "../io/output.h"
#include "commands.h"

app_error app_cmd_info(const app_config_t *config, int argc, char *const argv[]);

app_error app_cmd_info(const app_config_t *config, int argc, char *const argv[]) {
  (void)argc;
  (void)argv;

  if (app_config_is_quiet(config)) {
    return APP_SUCCESS;
  }

  const bool tui_enabled =
#ifdef ENABLE_TUI
      true;
#else
      false;
#endif

  if (app_config_is_json_output(config)) {
    bool root_comma = false;
    bool feature_comma = false;

    app_json_begin_object(stdout);
    app_json_write_string_field(stdout, "format_version", "1.0", &root_comma);
    app_json_write_string_field(stdout, "app", APP_NAME, &root_comma);
    app_json_write_string_field(stdout, "version", APP_VERSION, &root_comma);
    app_json_write_string_field(stdout, "git_commit", APP_GIT_COMMIT,
                                &root_comma);
    app_json_write_string_field(stdout, "build_date", APP_BUILD_DATE,
                                &root_comma);
    app_json_write_raw_field(stdout, "features", "{", &root_comma);
    app_json_write_bool_field(stdout, "tui", tui_enabled, &feature_comma);
    app_json_end_object(stdout);
    app_json_end_object(stdout);
    app_json_end_line(stdout);
    return APP_SUCCESS;
  }

  app_output_format(config, false, "Application: %s", APP_NAME);
  app_output_format(config, false, "Version: %s", APP_VERSION);
  app_output_format(config, false, "Git Commit: %s", APP_GIT_COMMIT);
  app_output_format(config, false, "Build: %s", APP_BUILD_DATE);
  app_output_format(config, false, "TUI Support: %s", app_yes_no(tui_enabled));
  return APP_SUCCESS;
}
