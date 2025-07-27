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

#include "error.h"
#include "types.h"

// Opaque configuration structure hides implementation details from callers.
// This allows us to change the internal representation without breaking API
// compatibility, and prevents direct manipulation that could leave config in
// an inconsistent state.
typedef struct app_config app_config_t;

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
bool app_config_is_quiet(const app_config_t *config);
bool app_config_is_debug(const app_config_t *config);
bool app_config_is_json_output(const app_config_t *config);
bool app_config_is_plain_output(const app_config_t *config);
bool app_config_is_no_color(const app_config_t *config);
bool app_config_is_verbose(const app_config_t *config);

// Configuration setters for programmatic use during initialization.
// These are primarily used by the load functions and testing code.
// Application code should prefer using the load functions to ensure
// proper validation and consistent behavior.
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
