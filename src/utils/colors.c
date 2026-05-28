/*
 * Terminal color handling implementation.
 */

#include "colors.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#include "../core/config.h"

static bool app_stderr_is_terminal(void) {
#ifdef _WIN32
  return _isatty(_fileno(stderr)) != 0;
#else
  return isatty(STDERR_FILENO) != 0;
#endif
}

bool app_use_colors(const app_config_t *config) {
  // Explicit plain/no-color modes take precedence over terminal heuristics.
  if (config &&
      (app_config_is_plain_output(config) || app_config_is_no_color(config))) {
    return false;
  }

  // NO_COLOR uses the same table-driven env policy as app_config_load_env().
  if (app_flag_env_enabled(APP_FLAG_NO_COLOR)) {
    return false;
  }

  // Force color if requested
  if (getenv("FORCE_COLOR") != NULL) {
    return true;
  }

  // Check if TERM is dumb
  const char *term = getenv("TERM");
  if (term != NULL && strcmp(term, "dumb") == 0) {
    return false;
  }

  // Check if output is to a terminal
  return app_stderr_is_terminal();
}
