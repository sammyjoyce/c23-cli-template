/*
 * "menu" command - launches the TUI showcase when compiled in.
 */

#include "../core/config.h"
#include "../core/error.h"
#include "../core/types.h"
#include "../io/output.h"
#ifdef ENABLE_TUI
#include "../tui/tui.h"
#endif
#include "commands.h"

app_error app_cmd_menu(const app_config_t *config, int argc, char **argv);

app_error app_cmd_menu(const app_config_t *config, int argc, char **argv) {
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
  app_output(
      "TUI support is not compiled in. Rebuild with "
      "'zig build -Denable-tui=true'.",
      config, true);
  return APP_ERROR_CONFIG;
#endif
}
