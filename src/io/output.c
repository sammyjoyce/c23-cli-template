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

static void app_write_json_string(FILE *stream, const char *text) {
  fputc('"', stream);
  for (const unsigned char *p = (const unsigned char *)text; *p != '\0'; p++) {
    switch (*p) {
    case '"':
      fputs("\\\"", stream);
      break;
    case '\\':
      fputs("\\\\", stream);
      break;
    case '\b':
      fputs("\\b", stream);
      break;
    case '\f':
      fputs("\\f", stream);
      break;
    case '\n':
      fputs("\\n", stream);
      break;
    case '\r':
      fputs("\\r", stream);
      break;
    case '\t':
      fputs("\\t", stream);
      break;
    default:
      if (*p < 0x20) {
        fprintf(stream, "\\u%04x", *p);
      } else {
        fputc(*p, stream);
      }
      break;
    }
  }
  fputc('"', stream);
}

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
    fputs("{\"format_version\":\"1.0\",\"message\":", stream);
    app_write_json_string(stream, text);
    fputs("}\n", stream);
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
  (void)pretty;

  if (json_string == nullptr || config == nullptr) {
    LOG_ERROR("Invalid parameters in app_output_json");
    return;
  }

  if (app_config_is_quiet(config)) {
    return;
  }

  printf("%s\n", json_string);
}
