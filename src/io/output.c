/*
 * Output handling implementation.
 */

#include "output.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "../core/config.h"
#include "../utils/logging.h"

static void app_json_write_separator(FILE *stream, bool *needs_comma) {
  if (!stream || !needs_comma) {
    return;
  }

  if (*needs_comma) {
    fputc(',', stream);
  }
  *needs_comma = true;
}

void app_json_begin_object(FILE *stream) {
  if (stream) {
    fputc('{', stream);
  }
}

void app_json_end_object(FILE *stream) {
  if (stream) {
    fputc('}', stream);
  }
}

void app_json_end_line(FILE *stream) {
  if (stream) {
    fputc('\n', stream);
  }
}

void app_json_write_string(FILE *stream, const char *text) {
  if (!stream) {
    return;
  }

  if (!text) {
    fputs("null", stream);
    return;
  }

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

void app_json_write_string_field(FILE *stream, const char *key,
                                 const char *value, bool *needs_comma) {
  if (!stream || !key || !needs_comma) {
    return;
  }

  app_json_write_separator(stream, needs_comma);
  app_json_write_string(stream, key);
  fputc(':', stream);
  app_json_write_string(stream, value);
}

void app_json_write_bool_field(FILE *stream, const char *key, bool value,
                               bool *needs_comma) {
  if (!stream || !key || !needs_comma) {
    return;
  }

  app_json_write_separator(stream, needs_comma);
  app_json_write_string(stream, key);
  fputc(':', stream);
  fputs(value ? "true" : "false", stream);
}

void app_json_write_raw_field(FILE *stream, const char *key, const char *value,
                              bool *needs_comma) {
  if (!stream || !key || !value || !needs_comma) {
    return;
  }

  app_json_write_separator(stream, needs_comma);
  app_json_write_string(stream, key);
  fputc(':', stream);
  fputs(value, stream);
}

void app_json_write_indent(FILE *stream, int level) {
  if (!stream || level <= 0) {
    return;
  }

  for (int i = 0; i < level; i++) {
    fputs("  ", stream);
  }
}

static void app_json_write_pretty_suffix(FILE *stream, bool comma) {
  if (comma) {
    fputc(',', stream);
  }
  fputc('\n', stream);
}

void app_json_write_pretty_string_field(FILE *stream, int level,
                                        const char *key, const char *value,
                                        bool comma) {
  if (!stream || !key) {
    return;
  }

  app_json_write_indent(stream, level);
  app_json_write_string(stream, key);
  fputs(": ", stream);
  app_json_write_string(stream, value);
  app_json_write_pretty_suffix(stream, comma);
}

void app_json_write_pretty_bool_field(FILE *stream, int level, const char *key,
                                      bool value, bool comma) {
  if (!stream || !key) {
    return;
  }

  app_json_write_indent(stream, level);
  app_json_write_string(stream, key);
  fputs(value ? ": true" : ": false", stream);
  app_json_write_pretty_suffix(stream, comma);
}

void app_json_write_pretty_int_field(FILE *stream, int level, const char *key,
                                     int value, bool comma) {
  if (!stream || !key) {
    return;
  }

  app_json_write_indent(stream, level);
  app_json_write_string(stream, key);
  fprintf(stream, ": %d", value);
  app_json_write_pretty_suffix(stream, comma);
}

void app_json_write_pretty_null_field(FILE *stream, int level, const char *key,
                                      bool comma) {
  if (!stream || !key) {
    return;
  }

  app_json_write_indent(stream, level);
  app_json_write_string(stream, key);
  fputs(": null", stream);
  app_json_write_pretty_suffix(stream, comma);
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
    bool needs_comma = false;
    app_json_begin_object(stream);
    app_json_write_string_field(stream, "format_version", "1.0", &needs_comma);
    app_json_write_string_field(stream, "message", text, &needs_comma);
    app_json_end_object(stream);
    app_json_end_line(stream);
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
