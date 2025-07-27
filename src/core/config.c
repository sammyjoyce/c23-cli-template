/*
 * Configuration management implementation.
 */

#include "config.h"

#include <limits.h>
#ifndef _WIN32
#include <pwd.h>
#include <unistd.h>
#endif
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

  // Try $HOME/.config/myapp/config.json
  const char *home = getenv("HOME");
  if (home) {
    snprintf(config_path, PATH_MAX, "%s/.config/%s/config.json", home,
             APP_NAME);
    if (access(config_path, R_OK) == 0) {
      return strdup(config_path);
    }
  }

  // Try /etc/myapp/config.json
  snprintf(config_path, PATH_MAX, "/etc/%s/config.json", APP_NAME);
  if (access(config_path, R_OK) == 0) {
    return strdup(config_path);
  }

  return NULL;
}

app_error app_config_load_file(app_config_t *config, const char *path) {
  CHECK_NULL(config, APP_ERROR_INVALID_ARG);

  // Use provided path or find default
  char *config_path = NULL;
  if (path) {
    config_path = strdup(path);
  } else {
    config_path = find_config_file();
  }

  if (!config_path) {
    LOG_DEBUG("No configuration file found");
    return APP_SUCCESS;  // Not an error if no config file exists
  }

  // Read file
  FILE *f = fopen(config_path, "r");
  if (!f) {
    LOG_WARNING("Failed to open config file: %s", config_path);
    free(config_path);
    return APP_ERROR_IO;
  }

  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  fseek(f, 0, SEEK_SET);

  char *content = app_secure_malloc(size + 1);
  if (!content) {
    fclose(f);
    free(config_path);
    return APP_ERROR_MEMORY;
  }

  if (fread(content, 1, size, f) != (size_t)size) {
    app_secure_free(content, size + 1);
    fclose(f);
    free(config_path);
    return APP_ERROR_IO;
  }
  content[size] = '\0';
  fclose(f);

  // Parse JSON
  // Note: In a real implementation, you'd parse JSON here
  // For now, we'll just log that we loaded the file
  LOG_INFO("Loaded configuration from %s", config_path);

  app_secure_free(content, size + 1);
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
  if (config && name) {
    if (config->program_name) {
      free(config->program_name);
    }
    config->program_name = strdup(name);
  }
}

void app_config_set_command(app_config_t *config, const char *command) {
  if (config && command) {
    if (config->command) {
      free(config->command);
    }
    config->command = strdup(command);
  }
}

void app_config_add_command_arg(app_config_t *config, const char *arg) {
  if (config && arg && config->command_arg_count < MAX_COMMAND_ARGS) {
    config->command_args[config->command_arg_count++] = strdup(arg);
  }
}

void app_config_set_config_file(app_config_t *config, const char *path) {
  if (config && path) {
    if (config->config_file) {
      free(config->config_file);
    }
    config->config_file = strdup(path);
  }
}