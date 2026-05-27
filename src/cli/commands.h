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

typedef app_error (*app_command_fn)(const app_config_t *config, int argc,
                                    char **argv);

typedef struct {
  const char *name;
  const char *summary;  // one-line description for help output
  app_command_fn handler;
  bool requires_terminal;  // hint: command is interactive (e.g. TUI)
} app_command_t;

// Return the registered command list. count is set to the number of entries.
const app_command_t *app_commands(size_t *count);

// Look up a command by name. Returns NULL when no command matches.
const app_command_t *app_command_find(const char *name);

static inline const char *app_yes_no(bool value) {
  return value ? "yes" : "no";
}
