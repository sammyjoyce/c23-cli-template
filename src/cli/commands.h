/*
 * Command registration table.
 *
 * Each command is a small function with a uniform signature. Add a new
 * command by writing the function and adding one row to g_app_commands in
 * commands.c. main() stays out of the way.
 */

#pragma once

#include "../core/config.h"
#include "../core/error.h"

#define APP_ARG_ARITY_UNBOUNDED (-1)

// Handlers only read the argv vector (they never reorder or rewrite entries),
// so argv is passed as char *const argv[]. This matches the char *const *
// returned by app_config_get_command_args and avoids a const-stripping cast at
// the dispatch site.
typedef app_error (*app_command_fn)(const app_config_t *config, int argc,
                                    char *const argv[]);

typedef struct {
  const char *name;
  bool required;
  int arity_minimum;
  int arity_maximum;  // APP_ARG_ARITY_UNBOUNDED means JSON null
  const char *description;
} app_command_arg_t;

typedef enum {
  APP_COMMAND_OPTION_UNKNOWN = 0,
  APP_COMMAND_OPTION_DOCTOR_DEEP,
} app_command_option_id_t;

typedef struct {
  app_command_option_id_t id;
  const char *name;
  const char *description;
} app_command_option_t;

typedef enum {
  APP_BUILTIN_OPTION_HELP,
  APP_BUILTIN_OPTION_VERSION,
} app_builtin_option_id_t;

typedef struct {
  app_builtin_option_id_t id;
  const char *name;
  const char *alias;
  const char *description;
} app_builtin_option_t;

typedef enum {
  APP_GLOBAL_VALUE_OPTION_CONFIG,
} app_global_value_option_id_t;

typedef struct {
  app_global_value_option_id_t id;
  const char *name;
  const char *alias;
  const app_command_arg_t *arguments;
  size_t argument_count;
  const char *description;
} app_global_value_option_t;

typedef struct {
  const char *name;
  const char *summary;
  app_command_fn handler;
  const app_command_option_t *options;
  size_t option_count;
  const app_command_arg_t *arguments;
  size_t argument_count;
  const char *const *examples;
  size_t example_count;
  bool requires_terminal;  // hint: command is interactive (e.g. TUI)
} app_command_t;

// Return the registered command list. count is set to the number of entries.
const app_command_t *app_commands(size_t *count);

// Return global built-in options such as --help and --version.
const app_builtin_option_t *app_builtin_options(size_t *count);

// Look up a built-in option by CLI spelling, for example "-h" or "--help".
const app_builtin_option_t *app_builtin_option_find(const char *arg);

// Return global options that consume a following value, such as --config PATH.
const app_global_value_option_t *app_global_value_options(size_t *count);

// Look up a value-taking global option by CLI spelling.
const app_global_value_option_t *app_global_value_option_find(const char *arg);

// Look up a command by name. Returns NULL when no command matches.
const app_command_t *app_command_find(const char *name);

// Look up a command option by CLI spelling, for example "--deep".
const app_command_option_t *app_command_option_find(
    const app_command_t *command, const char *arg);

// Validate an invocation against command metadata before dispatch. Command
// options are recognized as --name before a -- delimiter; remaining tokens are
// positionals and must satisfy the declared arity.
APP_NODISCARD app_error
app_command_validate_invocation(const app_command_t *command, int argc,
                                char *const argv[], const char *program_name);

static inline const char *app_yes_no(bool value) {
  return value ? "yes" : "no";
}
