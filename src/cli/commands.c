/*
 * Command registration and dispatch.
 */

#include "commands.h"

#include <stddef.h>
#include <string.h>

// Forward declarations for handlers defined in their own translation units.
app_error app_cmd_hello(const app_config_t *config, int argc, char **argv);
app_error app_cmd_echo(const app_config_t *config, int argc, char **argv);
app_error app_cmd_info(const app_config_t *config, int argc, char **argv);
app_error app_cmd_doctor(const app_config_t *config, int argc, char **argv);
app_error app_cmd_menu(const app_config_t *config, int argc, char **argv);

static const app_command_t g_app_commands[] = {
    {.name = "hello",
     .summary = "Print a greeting message.",
     .handler = app_cmd_hello,
     .requires_terminal = false},
    {.name = "echo",
     .summary = "Echo the provided text.",
     .handler = app_cmd_echo,
     .requires_terminal = false},
    {.name = "info",
     .summary = "Display application metadata.",
     .handler = app_cmd_info,
     .requires_terminal = false},
    {.name = "doctor",
     .summary = "Run starter diagnostics (add --deep for the TUI smoke test).",
     .handler = app_cmd_doctor,
     .requires_terminal = false},
    {.name = "menu",
     .summary = "Launch the interactive TUI main menu (requires --enable-tui).",
     .handler = app_cmd_menu,
     .requires_terminal = true},
};

#define G_APP_COMMANDS_COUNT \
  (sizeof(g_app_commands) / sizeof(g_app_commands[0]))

const app_command_t *app_commands(size_t *count) {
  if (count) {
    *count = G_APP_COMMANDS_COUNT;
  }
  return g_app_commands;
}

const app_command_t *app_command_find(const char *name) {
  if (!name) {
    return NULL;
  }
  for (size_t i = 0; i < G_APP_COMMANDS_COUNT; i++) {
    if (strcmp(g_app_commands[i].name, name) == 0) {
      return &g_app_commands[i];
    }
  }
  return NULL;
}
