/*
 * "menu" command - launches the TUI showcase when compiled in.
 */

#include "../core/app_info.h"
#include "../core/config.h"
#include "../core/error.h"
#include "../core/types.h"
#include "../io/output.h"
#ifdef ENABLE_TUI
#include "../tui/tui.h"
#endif
#include "commands.h"

app_error app_cmd_menu(const app_config_t *config, int argc,
                       char *const argv[]);

app_error app_cmd_menu(const app_config_t *config, int argc,
                       char *const argv[]) {
  (void)argc;
  (void)argv;

  if (app_config_is_json_output(config)) {
    app_output(
        "The menu command is interactive; remove --json to launch the TUI.",
        config, true);
    return APP_ERROR_INVALID_ARG;
  }

#ifdef ENABLE_TUI
  const app_error tui_err = tui_run_app();
  if (tui_err == APP_ERROR_OUT_OF_RANGE) {
    app_output_format(config, true,
                      "TUI failed: terminal is too small (minimum %dx%d).",
                      TUI_MIN_COLS, TUI_MIN_ROWS);
  } else if (tui_err != APP_SUCCESS) {
    app_output_format(config, true, "TUI failed: %s", app_strerror(tui_err));
  }
  return tui_err;
#else
  const app_feature_info_t *feature = app_feature_find(APP_FEATURE_TUI);
  if (feature && feature->build_option) {
    app_output_format(
        config, true,
        "TUI support is not compiled in. Rebuild with 'zig build %s'.",
        feature->build_option);
  } else {
    app_output("TUI support is not compiled in.", config, true);
  }
  return APP_ERROR_CONFIG;
#endif
}
