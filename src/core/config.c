/*
 * Configuration management implementation.
 */

#include "config.h"

#include <ctype.h>
#include <limits.h>
#ifndef _WIN32
#include <pwd.h>
#include <unistd.h>
#else
#include <io.h>
#define R_OK 4
#define access _access
#endif
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../utils/logging.h"
#include "../utils/memory.h"

#define MAX_COMMAND_ARGS 100

struct app_config {
  char *program_name;
  char *command;
  char *command_args[MAX_COMMAND_ARGS];
  int command_arg_count;
  char *config_file;
  bool quiet;
  bool debug;
  bool verbose;
  bool json_output;
  bool plain_output;
  bool no_color;
};

static bool app_config_set_string(char **slot, const char *value) {
  if (!slot || !value) {
    return false;
  }

  char *copy = strdup(value);
  if (!copy) {
    return false;
  }

  if (*slot) {
    free(*slot);
  }
  *slot = copy;
  return true;
}

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
        for (int i = 0; i < 4; i++) {
          if (p[i] == '\0' || !isxdigit((unsigned char)p[i])) {
            return APP_ERROR_CONFIG_PARSE;
          }
        }
        p += 4;
        ch = '?';
        break;
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

static app_error app_config_skip_json_value(const char **cursor);

static app_error app_config_skip_json_object(const char **cursor) {
  const char *p = app_config_skip_json_ws(*cursor);
  if (!p || *p != '{') {
    return APP_ERROR_CONFIG_PARSE;
  }
  p++;
  p = app_config_skip_json_ws(p);
  if (*p == '}') {
    *cursor = p + 1;
    return APP_SUCCESS;
  }

  while (*p != '\0') {
    char key[64];
    app_error err = app_config_parse_json_string(&p, key, sizeof(key));
    if (err != APP_SUCCESS) {
      return err;
    }
    p = app_config_skip_json_ws(p);
    if (*p != ':') {
      return APP_ERROR_CONFIG_PARSE;
    }
    p++;
    err = app_config_skip_json_value(&p);
    if (err != APP_SUCCESS) {
      return err;
    }
    p = app_config_skip_json_ws(p);
    if (*p == ',') {
      p++;
      continue;
    }
    if (*p == '}') {
      *cursor = p + 1;
      return APP_SUCCESS;
    }
    return APP_ERROR_CONFIG_PARSE;
  }

  return APP_ERROR_CONFIG_PARSE;
}

static app_error app_config_skip_json_array(const char **cursor) {
  const char *p = app_config_skip_json_ws(*cursor);
  if (!p || *p != '[') {
    return APP_ERROR_CONFIG_PARSE;
  }
  p++;
  p = app_config_skip_json_ws(p);
  if (*p == ']') {
    *cursor = p + 1;
    return APP_SUCCESS;
  }

  while (*p != '\0') {
    app_error err = app_config_skip_json_value(&p);
    if (err != APP_SUCCESS) {
      return err;
    }
    p = app_config_skip_json_ws(p);
    if (*p == ',') {
      p++;
      continue;
    }
    if (*p == ']') {
      *cursor = p + 1;
      return APP_SUCCESS;
    }
    return APP_ERROR_CONFIG_PARSE;
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

static app_error app_config_skip_json_value(const char **cursor) {
  const char *p = app_config_skip_json_ws(*cursor);
  if (!p) {
    return APP_ERROR_CONFIG_PARSE;
  }

  if (*p == '"') {
    char ignored[1];
    return app_config_parse_json_string(cursor, ignored, sizeof(ignored));
  }
  if (*p == '{') {
    return app_config_skip_json_object(cursor);
  }
  if (*p == '[') {
    return app_config_skip_json_array(cursor);
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

static bool app_config_apply_json_bool_key(app_config_t *config,
                                           const char *key, bool value) {
  if (strcmp(key, "debug") == 0) {
    app_config_set_debug(config, value);
  } else if (strcmp(key, "quiet") == 0) {
    app_config_set_quiet(config, value);
  } else if (strcmp(key, "verbose") == 0) {
    app_config_set_verbose(config, value);
  } else if (strcmp(key, "no_color") == 0) {
    app_config_set_no_color(config, value);
  } else if (strcmp(key, "json_output") == 0) {
    app_config_set_json_output(config, value);
  } else if (strcmp(key, "plain_output") == 0) {
    app_config_set_plain_output(config, value);
  } else {
    return false;
  }

  LOG_DEBUG("Loaded config key '%s' from file", key);
  return true;
}

static bool app_config_is_known_bool_key(const char *key) {
  return strcmp(key, "debug") == 0 || strcmp(key, "quiet") == 0 ||
         strcmp(key, "verbose") == 0 || strcmp(key, "no_color") == 0 ||
         strcmp(key, "json_output") == 0 || strcmp(key, "plain_output") == 0;
}

static app_error app_config_parse_json(app_config_t *config,
                                       const char *content) {
  CHECK_NULL(config, APP_ERROR_INVALID_ARG);
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
    cursor = app_config_skip_json_ws(cursor + 1);
    return *cursor == '\0' ? APP_SUCCESS : APP_ERROR_CONFIG_PARSE;
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
      (void)app_config_apply_json_bool_key(config, key, value);
    } else {
      err = app_config_skip_json_value(&cursor);
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
      cursor = app_config_skip_json_ws(cursor + 1);
      return *cursor == '\0' ? APP_SUCCESS : APP_ERROR_CONFIG_PARSE;
    }
    return APP_ERROR_CONFIG_PARSE;
  }

  return APP_ERROR_CONFIG_PARSE;
}

app_error app_config_create(app_config_t **config) {
  CHECK_NULL(config, APP_ERROR_INVALID_ARG);

  *config = calloc(1, sizeof(app_config_t));
  if (*config == NULL) {
    return APP_ERROR_MEMORY;
  }

  // Set defaults
  (*config)->quiet = false;
  (*config)->debug = false;
  (*config)->verbose = false;
  (*config)->json_output = false;
  (*config)->plain_output = false;
  (*config)->no_color = false;
  (*config)->command_arg_count = 0;

  return APP_SUCCESS;
}

void app_config_destroy(app_config_t *config) {
  if (config == NULL) {
    return;
  }

  // Free allocated strings
  if (config->program_name) {
    free(config->program_name);
  }
  if (config->command) {
    free(config->command);
  }
  if (config->config_file) {
    free(config->config_file);
  }

  // Free command arguments
  for (int i = 0; i < config->command_arg_count; i++) {
    if (config->command_args[i]) {
      free(config->command_args[i]);
    }
  }

  free(config);
}

// Find default config file path
static char *find_config_file(void) {
  static char config_path[PATH_MAX];

  // Check environment variable first
  const char *config_env = getenv("APP_CONFIG_PATH");
  if (config_env && access(config_env, R_OK) == 0) {
    return strdup(config_env);
  }

  // Try user config directory
#ifdef _WIN32
  const char *home = getenv("USERPROFILE");
  if (home) {
    snprintf(config_path, PATH_MAX, "%s\\AppData\\Local\\%s\\config.json", home,
             APP_NAME);
    if (access(config_path, R_OK) == 0) {
      return strdup(config_path);
    }
  }
#else
  const char *home = getenv("HOME");
  if (home) {
    snprintf(config_path, PATH_MAX, "%s/.config/%s/config.json", home,
             APP_NAME);
    if (access(config_path, R_OK) == 0) {
      return strdup(config_path);
    }
  }
#endif

  // Try system config directory
#ifdef _WIN32
  snprintf(config_path, PATH_MAX, "C:\\ProgramData\\%s\\config.json", APP_NAME);
#else
  snprintf(config_path, PATH_MAX, "/etc/%s/config.json", APP_NAME);
#endif
  if (access(config_path, R_OK) == 0) {
    return strdup(config_path);
  }

  return NULL;
}

app_error app_config_load_file(app_config_t *const config, const char *path) {
  CHECK_NULL(config, APP_ERROR_INVALID_ARG);

  char *config_path = path ? strdup(path) : find_config_file();
  if (path && !config_path) {
    return APP_ERROR_MEMORY;
  }

  if (!config_path) {
    LOG_DEBUG("No configuration file found");
    return APP_SUCCESS;  // Not an error if no config file exists
  }

  FILE *f = fopen(config_path, "rb");
  if (!f) {
    LOG_WARNING("Failed to open config file: %s", config_path);
    free(config_path);
    return APP_ERROR_IO;
  }

  if (fseek(f, 0, SEEK_END) != 0) {
    fclose(f);
    free(config_path);
    return APP_ERROR_IO;
  }

  long file_size = ftell(f);
  if (file_size < 0 || (uintmax_t)file_size > (uintmax_t)SIZE_MAX - 1U) {
    fclose(f);
    free(config_path);
    return APP_ERROR_IO;
  }

  if (fseek(f, 0, SEEK_SET) != 0) {
    fclose(f);
    free(config_path);
    return APP_ERROR_IO;
  }

  const size_t content_size = (size_t)file_size;
  char *content = app_secure_malloc(content_size + 1);
  if (!content) {
    fclose(f);
    free(config_path);
    return APP_ERROR_MEMORY;
  }

  if (content_size > 0 && fread(content, 1, content_size, f) != content_size) {
    app_secure_free(content, content_size + 1);
    fclose(f);
    free(config_path);
    return APP_ERROR_IO;
  }
  content[content_size] = '\0';
  fclose(f);

  app_error err = app_config_parse_json(config, content);
  if (err != APP_SUCCESS) {
    app_secure_free(content, content_size + 1);
    free(config_path);
    return err;
  }

  LOG_INFO("Loaded configuration from %s", config_path);
  if (!app_config_set_string(&config->config_file, config_path)) {
    app_secure_free(content, content_size + 1);
    free(config_path);
    return APP_ERROR_MEMORY;
  }

  app_secure_free(content, content_size + 1);
  free(config_path);
  return APP_SUCCESS;
}

app_error app_config_load_env(app_config_t *config) {
  CHECK_NULL(config, APP_ERROR_INVALID_ARG);

  // Check NO_COLOR environment variable
  if (getenv("NO_COLOR")) {
    config->no_color = true;
  }

  // Check APP_LOG_LEVEL
  const char *log_level = getenv("APP_LOG_LEVEL");
  if (log_level) {
    if (strcmp(log_level, "DEBUG") == 0) {
      config->debug = true;
    }
  }

  return APP_SUCCESS;
}

app_error app_config_load_args(app_config_t *config, int argc, char *argv[]) {
  // This is handled by args.c
  (void)config;
  (void)argc;
  (void)argv;
  return APP_SUCCESS;
}

// Getters
const char *app_config_get_program_name(const app_config_t *config) {
  return config ? config->program_name : APP_NAME;
}

const char *app_config_get_command(const app_config_t *config) {
  return config ? config->command : NULL;
}

char **app_config_get_command_args(const app_config_t *config, int *count) {
  if (!config || !count) {
    return NULL;
  }
  *count = config->command_arg_count;
  return (char **)config->command_args;
}

const char *app_config_get_config_file(const app_config_t *config) {
  return config ? config->config_file : NULL;
}

bool app_config_is_quiet(const app_config_t *config) {
  return config ? config->quiet : false;
}

bool app_config_is_debug(const app_config_t *config) {
  return config ? config->debug : false;
}

bool app_config_is_json_output(const app_config_t *config) {
  return config ? config->json_output && !config->plain_output : false;
}

bool app_config_is_plain_output(const app_config_t *config) {
  return config ? config->plain_output : false;
}

bool app_config_is_no_color(const app_config_t *config) {
  return config ? config->no_color : false;
}

bool app_config_is_verbose(const app_config_t *config) {
  return config ? config->verbose : false;
}

// Setters
void app_config_set_debug(app_config_t *config, bool debug) {
  if (config) {
    config->debug = debug;
  }
}

void app_config_set_quiet(app_config_t *config, bool quiet) {
  if (config) {
    config->quiet = quiet;
  }
}

void app_config_set_verbose(app_config_t *config, bool verbose) {
  if (config) {
    config->verbose = verbose;
  }
}

void app_config_set_json_output(app_config_t *config, bool json) {
  if (config) {
    config->json_output = json;
    if (json) {
      config->plain_output = false;
    }
  }
}

void app_config_set_plain_output(app_config_t *config, bool plain) {
  if (config) {
    config->plain_output = plain;
    if (plain) {
      config->json_output = false;
    }
  }
}

void app_config_set_no_color(app_config_t *config, bool no_color) {
  if (config) {
    config->no_color = no_color;
  }
}

void app_config_set_program_name(app_config_t *config, const char *name) {
  if (config) {
    app_config_set_string(&config->program_name, name);
  }
}

void app_config_set_command(app_config_t *config, const char *command) {
  if (config) {
    app_config_set_string(&config->command, command);
  }
}

void app_config_add_command_arg(app_config_t *config, const char *arg) {
  if (config && arg && config->command_arg_count < MAX_COMMAND_ARGS) {
    config->command_args[config->command_arg_count++] = strdup(arg);
  }
}

void app_config_set_config_file(app_config_t *config, const char *path) {
  if (config) {
    app_config_set_string(&config->config_file, path);
  }
}
