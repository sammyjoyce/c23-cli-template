/*
 * Output handling implementation.
 */

#include "output.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "../core/config.h"
#include "../utils/colors.h"
#include "../utils/logging.h"

void app_output(const char *text, const app_config_t *config, bool is_error) {
  if (text == nullptr || config == nullptr) {
    LOG_ERROR("Invalid parameters in app_output");
    return;
  }

  if (app_config_is_quiet(config) && !is_error) {
    return;  // Suppress non-error output in quiet mode
  }

  FILE *stream = is_error ? stderr : stdout;

  if (app_config_is_json_output(config)) {
    // Output as JSON string
    fprintf(stream, "{\"message\":\"%s\"}\n", text);
  } else {
    // Plain text output
    fprintf(stream, "%s\n", text);
  }
}

void app_output_format(const app_config_t *config, bool is_error,
                       const char *fmt, ...) {
  if (fmt == nullptr || config == nullptr) {
    LOG_ERROR("Invalid parameters in app_output_format");
    return;
  }

  if (app_config_is_quiet(config) && !is_error) {
    return;  // Suppress non-error output in quiet mode
  }

  char buffer[4096];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buffer, sizeof(buffer), fmt, args);
  va_end(args);

  app_output(buffer, config, is_error);
}

void app_output_json(const char *json_string, const app_config_t *config,
                     bool pretty) {
  if (json_string == nullptr || config == nullptr) {
    LOG_ERROR("Invalid parameters in app_output_json");
    return;
  }

  if (app_config_is_quiet(config)) {
    return;
  }

  // Output JSON (pretty-printing would be implemented here if needed)
  printf("%s\n", json_string);
}
