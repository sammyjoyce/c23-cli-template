/*
 * Configuration management for the application.
 *
 * Implements a layered configuration system where values can come from files,
 * environment variables, and command-line arguments, with later sources
 * overriding earlier ones. This approach allows users to set defaults in config
 * files while overriding specific values for individual runs via command line.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "error.h"
#include "types.h"

// The opaque app_config_t type is declared in types.h.

// Identifiers for every boolean flag the application understands.
// Adding a new flag is a three-step operation:
//   1. add an enum value here
//   2. add a row in g_app_flag_table (config.c)
//   3. add a getter via the APP_FLAG_GETTERS macro pattern, if needed
typedef enum {
  APP_FLAG_DEBUG,
  APP_FLAG_QUIET,
  APP_FLAG_VERBOSE,
  APP_FLAG_JSON_OUTPUT,
  APP_FLAG_PLAIN_OUTPUT,
  APP_FLAG_NO_COLOR,
  APP_FLAG_COUNT,
} app_flag_id;

// Metadata about a flag: how it appears on the command line, in env vars,
// in JSON config files, and which other flag (if any) it should reset when
// set to true.
typedef struct {
  app_flag_id id;
  const char *json_key;
  const char *env_var;         // NULL when no env override is supported
  const char *env_match;       // NULL means env-var presence enables the flag
  const char *cli_short;       // NULL when there is no short option
  const char *cli_long;        // long option, including leading "--"
  app_flag_id exclusive_with;  // set to id itself when there is no conflict
} app_flag_spec_t;

// Iterate over every flag spec. count is non-NULL on return.
const app_flag_spec_t *app_flag_table(size_t *count);

// Look up by JSON key (used while loading config files).
const app_flag_spec_t *app_flag_find_by_json_key(const char *key);

// Create and destroy configuration objects with proper lifecycle management.
// The create function allocates and initializes with defaults, while destroy
// ensures all allocated resources are properly freed to prevent leaks.
APP_NODISCARD app_error app_config_create(app_config_t **config);
void app_config_destroy(app_config_t *config);

// Load configuration from various sources with cumulative override semantics.
// Each load function merges new values with existing configuration, allowing
// users to build up configuration in layers: file -> environment -> command
// line. This precedence order ensures command-line args always win for maximum
// flexibility.
APP_NODISCARD app_error app_config_load_file(app_config_t *config,
                                             const char *path);
APP_NODISCARD app_error app_config_load_env(app_config_t *config);
APP_NODISCARD app_error app_config_load_args(app_config_t *config, int argc,
                                             char *argv[]);

// Configuration getters provide read-only access to ensure thread safety.
// By returning const pointers and values, we prevent accidental modification
// of shared configuration state and enable safe concurrent access from multiple
// threads.
const char *app_config_get_program_name(const app_config_t *config);
const char *app_config_get_command(const app_config_t *config);
char **app_config_get_command_args(const app_config_t *config, int *count);
const char *app_config_get_config_file(const app_config_t *config);
bool app_config_get_flag(const app_config_t *config, app_flag_id id);

bool app_config_is_quiet(const app_config_t *config);
bool app_config_is_debug(const app_config_t *config);
bool app_config_is_json_output(const app_config_t *config);
bool app_config_is_plain_output(const app_config_t *config);
bool app_config_is_no_color(const app_config_t *config);
bool app_config_is_verbose(const app_config_t *config);

// Generic flag setter that also enforces exclusivity (e.g. setting json
// clears plain). Prefer this from new code; the named setters below remain
// for source compatibility.
void app_config_set_flag(app_config_t *config, app_flag_id id, bool value);

void app_config_set_debug(app_config_t *config, bool debug);
void app_config_set_quiet(app_config_t *config, bool quiet);
void app_config_set_verbose(app_config_t *config, bool verbose);
void app_config_set_json_output(app_config_t *config, bool json);
void app_config_set_plain_output(app_config_t *config, bool plain);
void app_config_set_no_color(app_config_t *config, bool no_color);
void app_config_set_program_name(app_config_t *config, const char *name);
void app_config_set_command(app_config_t *config, const char *command);
void app_config_add_command_arg(app_config_t *config, const char *arg);
void app_config_set_config_file(app_config_t *config, const char *path);
