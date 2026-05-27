/*
 * Minimal JSON reader for app configuration files.
 */

#include "config_json.h"

#include <ctype.h>
#include <string.h>

#include "../utils/logging.h"

static const char *app_config_skip_json_ws(const char *cursor) {
  while (cursor && *cursor != '\0' && isspace((unsigned char)*cursor) != 0) {
    cursor++;
  }
  return cursor;
}

static bool app_config_json_value_boundary(char ch) {
  return ch == '\0' || ch == ',' || ch == '}' || ch == ']' ||
         isspace((unsigned char)ch) != 0;
}

static bool app_config_match_json_literal(const char *cursor,
                                          const char *literal,
                                          const char **end) {
  size_t i = 0;
  while (literal[i] != '\0') {
    if (cursor[i] == '\0' || cursor[i] != literal[i]) {
      return false;
    }
    i++;
  }
  if (!app_config_json_value_boundary(cursor[i])) {
    return false;
  }
  if (end) {
    *end = cursor + i;
  }
  return true;
}

static app_error app_config_parse_json_string(const char **cursor, char *out,
                                              size_t out_size) {
  if (!cursor || !*cursor || !out || out_size == 0) {
    return APP_ERROR_INVALID_ARG;
  }

  const char *p = app_config_skip_json_ws(*cursor);
  if (!p || *p != '"') {
    return APP_ERROR_CONFIG_PARSE;
  }
  p++;

  size_t used = 0;
  while (*p != '\0') {
    unsigned char ch = (unsigned char)*p++;
    if (ch == '"') {
      out[used] = '\0';
      *cursor = p;
      return APP_SUCCESS;
    }
    if (ch == '\\') {
      ch = (unsigned char)*p++;
      switch (ch) {
      case '"':
      case '\\':
      case '/':
        break;
      case 'b':
        ch = '\b';
        break;
      case 'f':
        ch = '\f';
        break;
      case 'n':
        ch = '\n';
        break;
      case 'r':
        ch = '\r';
        break;
      case 't':
        ch = '\t';
        break;
      case 'u':
        // \uXXXX escapes are valid JSON but would need real UTF-8 decoding
        // to round-trip. Rather than silently replace them with '?', reject
        // the value so users see a parse error and can switch to a literal
        // UTF-8 byte sequence in their config file.
        return APP_ERROR_CONFIG_PARSE;
      default:
        return APP_ERROR_CONFIG_PARSE;
      }
    } else if (ch < 0x20) {
      return APP_ERROR_CONFIG_PARSE;
    }

    if (used + 1 < out_size) {
      out[used++] = (char)ch;
    }
  }

  return APP_ERROR_CONFIG_PARSE;
}

static app_error app_config_skip_json_number(const char **cursor) {
  const char *p = app_config_skip_json_ws(*cursor);
  if (*p == '-') {
    p++;
  }
  if (!isdigit((unsigned char)*p)) {
    return APP_ERROR_CONFIG_PARSE;
  }
  if (*p == '0') {
    p++;
  } else {
    while (isdigit((unsigned char)*p)) {
      p++;
    }
  }
  if (*p == '.') {
    p++;
    if (!isdigit((unsigned char)*p)) {
      return APP_ERROR_CONFIG_PARSE;
    }
    while (isdigit((unsigned char)*p)) {
      p++;
    }
  }
  if (*p == 'e' || *p == 'E') {
    p++;
    if (*p == '+' || *p == '-') {
      p++;
    }
    if (!isdigit((unsigned char)*p)) {
      return APP_ERROR_CONFIG_PARSE;
    }
    while (isdigit((unsigned char)*p)) {
      p++;
    }
  }

  if (!app_config_json_value_boundary(*p)) {
    return APP_ERROR_CONFIG_PARSE;
  }
  *cursor = p;
  return APP_SUCCESS;
}

static app_error app_config_skip_json_literal(const char **cursor,
                                              const char *literal) {
  const char *p = app_config_skip_json_ws(*cursor);
  const char *end = NULL;
  if (!app_config_match_json_literal(p, literal, &end)) {
    return APP_ERROR_CONFIG_PARSE;
  }
  *cursor = end;
  return APP_SUCCESS;
}

static app_error app_config_skip_json_scalar(const char **cursor) {
  const char *p = app_config_skip_json_ws(*cursor);
  if (!p) {
    return APP_ERROR_CONFIG_PARSE;
  }

  if (*p == '"') {
    char ignored[1];
    return app_config_parse_json_string(cursor, ignored, sizeof(ignored));
  }
  if (*p == 't') {
    return app_config_skip_json_literal(cursor, "true");
  }
  if (*p == 'f') {
    return app_config_skip_json_literal(cursor, "false");
  }
  if (*p == 'n') {
    return app_config_skip_json_literal(cursor, "null");
  }
  return app_config_skip_json_number(cursor);
}

static app_error app_config_read_json_bool_value(const char **cursor,
                                                 bool *value) {
  if (!cursor || !*cursor || !value) {
    return APP_ERROR_INVALID_ARG;
  }

  const char *p = app_config_skip_json_ws(*cursor);
  const char *end = NULL;
  if (app_config_match_json_literal(p, "true", &end)) {
    *value = true;
    *cursor = end;
    return APP_SUCCESS;
  }
  if (app_config_match_json_literal(p, "false", &end)) {
    *value = false;
    *cursor = end;
    return APP_SUCCESS;
  }

  return APP_ERROR_CONFIG_PARSE;
}

static bool app_config_apply_json_bool_key(app_config_json_state_t *state,
                                           const char *key, bool value) {
  const app_flag_spec_t *spec = app_flag_find_by_json_key(key);
  if (!spec) {
    return false;
  }

  state->values[spec->id] = value;
  if (value && spec->exclusive_with != spec->id) {
    state->values[spec->exclusive_with] = false;
  }
  LOG_DEBUG("Loaded config key '%s' from file", key);
  return true;
}

static bool app_config_is_known_bool_key(const char *key) {
  return app_flag_find_by_json_key(key) != NULL;
}

static app_error app_config_finish_json_parse(const char *cursor) {
  cursor = app_config_skip_json_ws(cursor);
  if (!cursor || *cursor != '\0') {
    return APP_ERROR_CONFIG_PARSE;
  }

  return APP_SUCCESS;
}

app_error app_config_parse_json_state(app_config_json_state_t *staged,
                                      const char *content) {
  CHECK_NULL(staged, APP_ERROR_INVALID_ARG);
  CHECK_NULL(content, APP_ERROR_INVALID_ARG);

  const char *cursor = app_config_skip_json_ws(content);
  if (!cursor || *cursor == '\0') {
    return APP_SUCCESS;
  }
  if (*cursor != '{') {
    return APP_ERROR_CONFIG_PARSE;
  }

  cursor++;
  cursor = app_config_skip_json_ws(cursor);
  if (*cursor == '}') {
    return app_config_finish_json_parse(cursor + 1);
  }

  while (*cursor != '\0') {
    char key[64];
    app_error err = app_config_parse_json_string(&cursor, key, sizeof(key));
    if (err != APP_SUCCESS) {
      return err;
    }

    cursor = app_config_skip_json_ws(cursor);
    if (*cursor != ':') {
      return APP_ERROR_CONFIG_PARSE;
    }
    cursor++;

    if (app_config_is_known_bool_key(key)) {
      bool value = false;
      err = app_config_read_json_bool_value(&cursor, &value);
      if (err != APP_SUCCESS) {
        LOG_WARNING("Invalid boolean value for config key '%s'", key);
        return err;
      }
      (void)app_config_apply_json_bool_key(staged, key, value);
    } else {
      err = app_config_skip_json_scalar(&cursor);
      if (err != APP_SUCCESS) {
        return err;
      }
    }

    cursor = app_config_skip_json_ws(cursor);
    if (*cursor == ',') {
      cursor++;
      continue;
    }
    if (*cursor == '}') {
      return app_config_finish_json_parse(cursor + 1);
    }
    return APP_ERROR_CONFIG_PARSE;
  }

  return APP_ERROR_CONFIG_PARSE;
}
