/*
 * "opencli" command - prints the checked-in OpenCLI contract from live command
 * and flag metadata.
 */

#include <stdio.h>
#include <string.h>

#include "../core/config.h"
#include "../core/error.h"
#include "../core/types.h"
#include "../io/output.h"
#include "commands.h"

app_error app_cmd_opencli(const app_config_t *config, int argc, char **argv);

static void opencli_indent(FILE *stream, int level) {
  for (int i = 0; i < level; i++) {
    fputs("  ", stream);
  }
}

static void opencli_string_field(FILE *stream, int level, const char *key,
                                 const char *value, bool comma) {
  opencli_indent(stream, level);
  app_json_write_string(stream, key);
  fputs(": ", stream);
  app_json_write_string(stream, value);
  if (comma) {
    fputc(',', stream);
  }
  fputc('\n', stream);
}

static void opencli_bool_field(FILE *stream, int level, const char *key,
                               bool value, bool comma) {
  opencli_indent(stream, level);
  app_json_write_string(stream, key);
  fputs(value ? ": true" : ": false", stream);
  if (comma) {
    fputc(',', stream);
  }
  fputc('\n', stream);
}

static void opencli_int_field(FILE *stream, int level, const char *key,
                              int value, bool comma) {
  opencli_indent(stream, level);
  app_json_write_string(stream, key);
  fprintf(stream, ": %d", value);
  if (comma) {
    fputc(',', stream);
  }
  fputc('\n', stream);
}

static void opencli_null_field(FILE *stream, int level, const char *key,
                               bool comma) {
  opencli_indent(stream, level);
  app_json_write_string(stream, key);
  fputs(": null", stream);
  if (comma) {
    fputc(',', stream);
  }
  fputc('\n', stream);
}

static const char *opencli_option_name(const char *cli_long) {
  if (!cli_long) {
    return "";
  }
  if (strncmp(cli_long, "--", 2) == 0) {
    return cli_long + 2;
  }
  return cli_long;
}

static const char *opencli_short_alias(const char *cli_short) {
  if (!cli_short) {
    return NULL;
  }
  if (cli_short[0] == '-' && cli_short[1] != '\0') {
    return cli_short + 1;
  }
  return cli_short;
}

static void opencli_print_aliases(FILE *stream, int level,
                                  const char *alias) {
  opencli_indent(stream, level);
  fputs("\"aliases\": [", stream);
  if (alias && alias[0] != '\0') {
    app_json_write_string(stream, alias);
  }
  fputs("],\n", stream);
}

static void opencli_print_empty_args(FILE *stream, int level, bool comma) {
  opencli_indent(stream, level);
  fputs("\"arguments\": []", stream);
  if (comma) {
    fputc(',', stream);
  }
  fputc('\n', stream);
}

static void opencli_print_global_option(FILE *stream, const char *name,
                                        const char *alias,
                                        const char *description, bool comma) {
  opencli_indent(stream, 2);
  fputs("{\n", stream);
  opencli_string_field(stream, 3, "name", name, true);
  opencli_bool_field(stream, 3, "required", false, true);
  opencli_print_aliases(stream, 3, alias);
  opencli_print_empty_args(stream, 3, true);
  opencli_string_field(stream, 3, "description", description, false);
  opencli_indent(stream, 2);
  fputc('}', stream);
  if (comma) {
    fputc(',', stream);
  }
  fputc('\n', stream);
}

static void opencli_print_config_option(FILE *stream) {
  opencli_indent(stream, 2);
  fputs("{\n", stream);
  opencli_string_field(stream, 3, "name", "config", true);
  opencli_bool_field(stream, 3, "required", false, true);
  opencli_print_aliases(stream, 3, "c");
  opencli_indent(stream, 3);
  fputs("\"arguments\": [\n", stream);
  opencli_indent(stream, 4);
  fputs("{\n", stream);
  opencli_string_field(stream, 5, "name", "path", true);
  opencli_bool_field(stream, 5, "required", true, true);
  opencli_int_field(stream, 5, "ordinal", 1, true);
  opencli_indent(stream, 5);
  fputs("\"arity\": {\n", stream);
  opencli_int_field(stream, 6, "minimum", 1, true);
  opencli_int_field(stream, 6, "maximum", 1, false);
  opencli_indent(stream, 5);
  fputs("},\n", stream);
  opencli_string_field(stream, 5, "description", "Path to configuration file",
                       false);
  opencli_indent(stream, 4);
  fputs("}\n", stream);
  opencli_indent(stream, 3);
  fputs("],\n", stream);
  opencli_string_field(stream, 3, "description", "Specify configuration file path",
                       false);
  opencli_indent(stream, 2);
  fputs("}\n", stream);
}

static void opencli_print_options(FILE *stream) {
  fputs("  \"options\": [\n", stream);
  opencli_print_global_option(stream, "help", "h", "Show help message and exit",
                              true);
  opencli_print_global_option(stream, "version", NULL,
                              "Show version information and exit", true);

  size_t count = 0;
  const app_flag_spec_t *flags = app_flag_table(&count);
  for (size_t i = 0; i < count; i++) {
    const app_flag_spec_t *flag = &flags[i];
    opencli_print_global_option(
        stream, opencli_option_name(flag->cli_long),
        opencli_short_alias(flag->cli_short), flag->description, true);
  }

  opencli_print_config_option(stream);
  fputs("  ],\n", stream);
}

static void opencli_print_command_option(FILE *stream,
                                         const app_command_option_t *option,
                                         bool comma) {
  opencli_indent(stream, 4);
  fputs("{\n", stream);
  opencli_string_field(stream, 5, "name", option->name, true);
  opencli_bool_field(stream, 5, "required", false, true);
  opencli_indent(stream, 5);
  fputs("\"aliases\": [],\n", stream);
  opencli_print_empty_args(stream, 5, true);
  opencli_string_field(stream, 5, "description", option->description, false);
  opencli_indent(stream, 4);
  fputc('}', stream);
  if (comma) {
    fputc(',', stream);
  }
  fputc('\n', stream);
}

static void opencli_print_command_arg(FILE *stream,
                                      const app_command_arg_t *arg,
                                      bool comma) {
  opencli_indent(stream, 4);
  fputs("{\n", stream);
  opencli_string_field(stream, 5, "name", arg->name, true);
  opencli_bool_field(stream, 5, "required", arg->required, true);
  opencli_int_field(stream, 5, "ordinal", arg->ordinal, true);
  opencli_indent(stream, 5);
  fputs("\"arity\": {\n", stream);
  opencli_int_field(stream, 6, "minimum", arg->arity_minimum, true);
  if (arg->arity_maximum == APP_ARG_ARITY_UNBOUNDED) {
    opencli_null_field(stream, 6, "maximum", false);
  } else {
    opencli_int_field(stream, 6, "maximum", arg->arity_maximum, false);
  }
  opencli_indent(stream, 5);
  fputs("},\n", stream);
  opencli_string_field(stream, 5, "description", arg->description, false);
  opencli_indent(stream, 4);
  fputc('}', stream);
  if (comma) {
    fputc(',', stream);
  }
  fputc('\n', stream);
}

static void opencli_print_examples(FILE *stream, int level,
                                   const char *const *examples,
                                   size_t count, bool comma) {
  opencli_indent(stream, level);
  fputs("\"examples\": [", stream);
  if (count > 0) {
    fputc('\n', stream);
    for (size_t i = 0; i < count; i++) {
      opencli_indent(stream, level + 1);
      app_json_write_string(stream, examples[i]);
      if (i + 1 < count) {
        fputc(',', stream);
      }
      fputc('\n', stream);
    }
    opencli_indent(stream, level);
  }
  fputc(']', stream);
  if (comma) {
    fputc(',', stream);
  }
  fputc('\n', stream);
}

static void opencli_print_command(FILE *stream, const app_command_t *command,
                                  bool comma) {
  opencli_indent(stream, 2);
  fputs("{\n", stream);
  opencli_string_field(stream, 3, "name", command->name, true);
  opencli_string_field(stream, 3, "description", command->description, true);

  opencli_indent(stream, 3);
  fputs("\"options\": [", stream);
  if (command->option_count > 0) {
    fputc('\n', stream);
    for (size_t i = 0; i < command->option_count; i++) {
      opencli_print_command_option(stream, &command->options[i],
                                   i + 1 < command->option_count);
    }
    opencli_indent(stream, 3);
  }
  fputs("],\n", stream);

  opencli_indent(stream, 3);
  fputs("\"arguments\": [", stream);
  if (command->argument_count > 0) {
    fputc('\n', stream);
    for (size_t i = 0; i < command->argument_count; i++) {
      opencli_print_command_arg(stream, &command->arguments[i],
                                i + 1 < command->argument_count);
    }
    opencli_indent(stream, 3);
  }
  fputs("],\n", stream);

  opencli_print_examples(stream, 3, command->examples, command->example_count,
                         command->requires_terminal);
  if (command->requires_terminal) {
    opencli_indent(stream, 3);
    fputs("\"metadata\": {\n", stream);
    opencli_string_field(stream, 4, "requires", "ncurses", true);
    opencli_bool_field(stream, 4, "interactive", true, false);
    opencli_indent(stream, 3);
    fputs("}\n", stream);
  }

  opencli_indent(stream, 2);
  fputc('}', stream);
  if (comma) {
    fputc(',', stream);
  }
  fputc('\n', stream);
}

static void opencli_print_commands(FILE *stream) {
  size_t count = 0;
  const app_command_t *commands = app_commands(&count);

  fputs("  \"commands\": [\n", stream);
  for (size_t i = 0; i < count; i++) {
    opencli_print_command(stream, &commands[i], i + 1 < count);
  }
  fputs("  ],\n", stream);
}

static void opencli_print_exit_codes(FILE *stream) {
  fputs("  \"exitCodes\": [\n", stream);
  const struct {
    int code;
    const char *description;
  } codes[] = {
      {0, "Success"},
      {1, "General error"},
      {2, "Invalid command or argument"},
      {3, "Configuration error"},
      {10, "Memory allocation error"},
      {11, "I/O error"},
      {12, "Permission denied"},
  };

  for (size_t i = 0; i < sizeof(codes) / sizeof(codes[0]); i++) {
    opencli_indent(stream, 2);
    fputs("{\n", stream);
    opencli_int_field(stream, 3, "code", codes[i].code, true);
    opencli_string_field(stream, 3, "description", codes[i].description,
                         false);
    opencli_indent(stream, 2);
    fputc('}', stream);
    if (i + 1 < sizeof(codes) / sizeof(codes[0])) {
      fputc(',', stream);
    }
    fputc('\n', stream);
  }
  fputs("  ],\n", stream);
}

static void opencli_print_top_examples(FILE *stream) {
  const char *const examples[] = {
      APP_NAME " hello",       APP_NAME " hello Alice",
      APP_NAME " echo Hello World", APP_NAME " info",
      APP_NAME " --json info", APP_NAME " doctor",
      APP_NAME " opencli",     APP_NAME " --help",
      APP_NAME " --version",
  };

  opencli_print_examples(stream, 1, examples,
                         sizeof(examples) / sizeof(examples[0]), true);
}

static void opencli_print_metadata(FILE *stream) {
  fputs("  \"metadata\": [\n", stream);
  fputs("    {\n", stream);
  opencli_string_field(stream, 3, "name", "environment", true);
  fputs("      \"value\": {\n", stream);
  opencli_string_field(stream, 4, "APP_LOG_LEVEL",
                       "Set logging verbosity: ERROR, WARNING, INFO, DEBUG (default: ERROR)",
                       true);
  opencli_string_field(stream, 4, "NO_COLOR", "Disable colored output when set",
                       false);
  fputs("      }\n", stream);
  fputs("    },\n", stream);
  fputs("    {\n", stream);
  opencli_string_field(stream, 3, "name", "configuration", true);
  fputs("      \"value\": {\n", stream);
  opencli_string_field(stream, 4, "location",
                       "~/.config/" APP_NAME "/config.json", true);
  opencli_string_field(
      stream, 4, "format",
      "Flat JSON object with boolean debug, quiet, verbose, no_color, json_output, and plain_output keys",
      true);
  opencli_string_field(stream, 4, "precedence",
                       "CLI args > Environment > Config file > Defaults",
                       false);
  fputs("      }\n", stream);
  fputs("    },\n", stream);
  fputs("    {\n", stream);
  opencli_string_field(stream, 3, "name", "build", true);
  fputs("      \"value\": {\n", stream);
  opencli_string_field(stream, 4, "system", "Zig build system", true);
  opencli_string_field(stream, 4, "compiler", "Zig cc (Clang/LLVM)", true);
  opencli_string_field(stream, 4, "standard", "C23", false);
  fputs("      }\n", stream);
  fputs("    }\n", stream);
  fputs("  ]\n", stream);
}

app_error app_cmd_opencli(const app_config_t *config, int argc, char **argv) {
  (void)config;
  (void)argc;
  (void)argv;

  fputs("{\n", stdout);
  opencli_string_field(stdout, 1, "opencli", "0.1", true);
  fputs("  \"info\": {\n", stdout);
  opencli_string_field(stdout, 2, "title", "C23 TUI + CLI Starter", true);
  opencli_string_field(
      stdout, 2, "description",
      "A ready-to-use C23 starter for command-line tools and ncurses terminal UIs.",
      true);
  opencli_string_field(stdout, 2, "version", APP_VERSION, true);
  fputs("    \"contact\": {\n", stdout);
  opencli_string_field(stdout, 3, "name", "Your Name", true);
  opencli_string_field(stdout, 3, "url",
                       "https://github.com/yourusername/yourproject", false);
  fputs("    },\n", stdout);
  fputs("    \"license\": {\n", stdout);
  opencli_string_field(stdout, 3, "name", "MIT License", true);
  opencli_string_field(stdout, 3, "identifier", "MIT", false);
  fputs("    }\n", stdout);
  fputs("  },\n", stdout);
  fputs("  \"conventions\": {\n", stdout);
  opencli_bool_field(stdout, 2, "groupOptions", false, true);
  opencli_string_field(stdout, 2, "optionArgumentSeparator", " ", false);
  fputs("  },\n", stdout);
  fputs("  \"arguments\": [\n", stdout);
  fputs("    {\n", stdout);
  opencli_string_field(stdout, 3, "name", "command", true);
  opencli_bool_field(stdout, 3, "required", true, true);
  opencli_int_field(stdout, 3, "ordinal", 1, true);
  fputs("      \"arity\": {\n", stdout);
  opencli_int_field(stdout, 4, "minimum", 1, true);
  opencli_int_field(stdout, 4, "maximum", 1, false);
  fputs("      },\n", stdout);
  opencli_string_field(stdout, 3, "description", "The command to execute",
                       false);
  fputs("    }\n", stdout);
  fputs("  ],\n", stdout);
  opencli_print_options(stdout);
  opencli_print_commands(stdout);
  opencli_print_exit_codes(stdout);
  opencli_print_top_examples(stdout);
  opencli_bool_field(stdout, 1, "interactive", false, true);
  opencli_print_metadata(stdout);
  fputs("}\n", stdout);

  return APP_SUCCESS;
}
