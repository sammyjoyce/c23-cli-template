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
app_error app_cmd_opencli(const app_config_t *config, int argc, char **argv);

static const app_command_arg_t hello_args[] = {
    {.name = "name",
     .required = false,
     .ordinal = 1,
     .arity_minimum = 0,
     .arity_maximum = 1,
     .description = "Name to greet (default: World)"},
};

static const char *const hello_examples[] = {
    APP_NAME " hello",
    APP_NAME " hello Alice",
};

static const app_command_arg_t echo_args[] = {
    {.name = "text",
     .required = false,
     .ordinal = 1,
     .arity_minimum = 0,
     .arity_maximum = APP_ARG_ARITY_UNBOUNDED,
     .description = "Text to echo"},
};

static const char *const echo_examples[] = {
    APP_NAME " echo Hello World",
    APP_NAME " echo",
};

static const char *const info_examples[] = {
    APP_NAME " info",
    APP_NAME " --json info",
};

static const app_command_option_t doctor_options[] = {
    {.name = "deep",
     .description =
         "Also exercise the optional TUI runtime when a TTY is available"},
};

static const char *const doctor_examples[] = {
    APP_NAME " doctor",
    APP_NAME " --json doctor",
};

static const char *const menu_examples[] = {
    "zig build -Denable-tui=true run -- menu",
};

static const char *const opencli_examples[] = {
    APP_NAME " opencli",
};

static const app_builtin_option_t g_app_builtin_options[] = {
    {.id = APP_BUILTIN_OPTION_HELP,
     .name = "help",
     .alias = "h",
     .description = "Show help message and exit"},
    {.id = APP_BUILTIN_OPTION_VERSION,
     .name = "version",
     .alias = NULL,
     .description = "Show version information and exit"},
};

static const app_command_t g_app_commands[] = {
    {.name = "hello",
     .summary = "Print a greeting message.",
     .handler = app_cmd_hello,
     .arguments = hello_args,
     .argument_count = sizeof(hello_args) / sizeof(hello_args[0]),
     .examples = hello_examples,
     .example_count = sizeof(hello_examples) / sizeof(hello_examples[0]),
     .requires_terminal = false},
    {.name = "echo",
     .summary = "Echo the provided text.",
     .handler = app_cmd_echo,
     .arguments = echo_args,
     .argument_count = sizeof(echo_args) / sizeof(echo_args[0]),
     .examples = echo_examples,
     .example_count = sizeof(echo_examples) / sizeof(echo_examples[0]),
     .requires_terminal = false},
    {.name = "info",
     .summary = "Display application metadata.",
     .handler = app_cmd_info,
     .examples = info_examples,
     .example_count = sizeof(info_examples) / sizeof(info_examples[0]),
     .requires_terminal = false},
    {.name = "doctor",
     .summary = "Run starter diagnostics (add --deep for the TUI smoke test).",
     .handler = app_cmd_doctor,
     .options = doctor_options,
     .option_count = sizeof(doctor_options) / sizeof(doctor_options[0]),
     .examples = doctor_examples,
     .example_count = sizeof(doctor_examples) / sizeof(doctor_examples[0]),
     .requires_terminal = false},
    {.name = "menu",
     .summary = "Launch the interactive TUI main menu (requires --enable-tui).",
     .handler = app_cmd_menu,
     .examples = menu_examples,
     .example_count = sizeof(menu_examples) / sizeof(menu_examples[0]),
     .requires_terminal = true},
    {.name = "opencli",
     .summary = "Print the OpenCLI contract as JSON.",
     .handler = app_cmd_opencli,
     .examples = opencli_examples,
     .example_count = sizeof(opencli_examples) / sizeof(opencli_examples[0]),
     .requires_terminal = false},
};

#define G_APP_COMMANDS_COUNT \
  (sizeof(g_app_commands) / sizeof(g_app_commands[0]))
#define G_APP_BUILTIN_OPTIONS_COUNT \
  (sizeof(g_app_builtin_options) / sizeof(g_app_builtin_options[0]))

const app_command_t *app_commands(size_t *count) {
  if (count) {
    *count = G_APP_COMMANDS_COUNT;
  }
  return g_app_commands;
}

const app_builtin_option_t *app_builtin_options(size_t *count) {
  if (count) {
    *count = G_APP_BUILTIN_OPTIONS_COUNT;
  }
  return g_app_builtin_options;
}

const app_builtin_option_t *app_builtin_option_find(const char *arg) {
  if (!arg) {
    return NULL;
  }

  for (size_t i = 0; i < G_APP_BUILTIN_OPTIONS_COUNT; i++) {
    const app_builtin_option_t *option = &g_app_builtin_options[i];
    const bool long_match =
        strncmp(arg, "--", 2) == 0 && strcmp(arg + 2, option->name) == 0;
    const bool short_match = option->alias && arg[0] == '-' && arg[1] != '-' &&
                             strcmp(arg + 1, option->alias) == 0;
    if (long_match || short_match) {
      return option;
    }
  }
  return NULL;
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
