/*
 * Terminal color handling implementation.
 */

#include "colors.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../core/config.h"

bool app_use_colors(const app_config_t *config) {
  // Explicit plain/no-color modes take precedence over terminal heuristics.
  if (config &&
      (app_config_is_plain_output(config) || app_config_is_no_color(config))) {
    return false;
  }

  // NO_COLOR is presence-based and applies even when a caller passed a config
  // that has not loaded environment variables.
  if (getenv("NO_COLOR") != NULL) {
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
  return isatty(STDERR_FILENO);
}
