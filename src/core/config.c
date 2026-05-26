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

static bool app_config_json_value_ended(const char *cursor) {
  cursor = app_config_skip_json_ws(cursor);
  return cursor && (*cursor == '\0' || *cursor == ',' || *cursor == '}');
}

static app_error app_config_read_json_bool(const char *content, const char *key,
                                           bool *value, bool *found) {
  if (!content || !key || !value || !found) {
    return APP_ERROR_INVALID_ARG;
  }

  *found = false;

  char pattern[64];
  const int pattern_len = snprintf(pattern, sizeof(pattern), "\"%s\"", key);
  if (pattern_len <= 0 || (size_t)pattern_len >= sizeof(pattern)) {
    return APP_ERROR_CONFIG_INVALID;
  }

  const char *cursor = content;
  while ((cursor = strstr(cursor, pattern)) != NULL) {
    cursor += pattern_len;
    cursor = app_config_skip_json_ws(cursor);
    if (!cursor || *cursor != ':') {
      continue;
    }

    cursor = app_config_skip_json_ws(cursor + 1);
    if (!cursor) {
      return APP_ERROR_CONFIG_PARSE;
    }

    if (strncmp(cursor, "true", 4) == 0 &&
        app_config_json_value_ended(cursor + 4)) {
      *value = true;
      *found = true;
      return APP_SUCCESS;
    }
    if (strncmp(cursor, "false", 5) == 0 &&
        app_config_json_value_ended(cursor + 5)) {
      *value = false;
      *found = true;
      return APP_SUCCESS;
    }

    return APP_ERROR_CONFIG_PARSE;
  }

  return APP_SUCCESS;
}

static app_error app_config_apply_json_bool(const char *content,
                                            const char *key, bool *target) {
  bool value = false;
  bool found = false;
  const app_error err = app_config_read_json_bool(content, key, &value, &found);
  if (err != APP_SUCCESS) {
    LOG_WARNING("Invalid boolean value for config key '%s'", key);
    return err;
  }

  if (found) {
    *target = value;
    LOG_DEBUG("Loaded config key '%s' from file", key);
  }
  return APP_SUCCESS;
}

static app_error app_config_parse_json(app_config_t *config,
                                       const char *content) {
  CHECK_NULL(config, APP_ERROR_INVALID_ARG);
  CHECK_NULL(content, APP_ERROR_INVALID_ARG);

  const char *cursor = app_config_skip_json_ws(content);
  if (!cursor || (*cursor != '\0' && *cursor != '{')) {
    return APP_ERROR_CONFIG_PARSE;
  }

  app_error err = APP_SUCCESS;
  if ((err = app_config_apply_json_bool(content, "debug", &config->debug)) !=
      APP_SUCCESS) {
    return err;
  }
  if ((err = app_config_apply_json_bool(content, "quiet", &config->quiet)) !=
      APP_SUCCESS) {
    return err;
  }
  if ((err = app_config_apply_json_bool(content, "verbose",
                                        &config->verbose)) != APP_SUCCESS) {
    return err;
  }
  if ((err = app_config_apply_json_bool(content, "no_color",
                                        &config->no_color)) != APP_SUCCESS) {
    return err;
  }
  if ((err = app_config_apply_json_bool(content, "json_output",
                                        &config->json_output)) != APP_SUCCESS) {
    return err;
  }
  if ((err = app_config_apply_json_bool(
           content, "plain_output", &config->plain_output)) != APP_SUCCESS) {
    return err;
  }

  return APP_SUCCESS;
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
  return config ? config->json_output : false;
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
  }
}

void app_config_set_plain_output(app_config_t *config, bool plain) {
  if (config) {
    config->plain_output = plain;
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
