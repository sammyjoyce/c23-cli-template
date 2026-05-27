/*
 * "opencli" command - prints the OpenCLI contract from shared metadata tables.
 */

#include <stdio.h>
#include <string.h>

#include "../core/config.h"
#include "../core/error.h"
#include "../core/types.h"
#include "../io/output.h"
#include "commands.h"
#include "opencli_contract.h"

app_error app_cmd_opencli(const app_config_t *config, int argc, char **argv);

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

static void opencli_print_aliases(FILE *stream, int level, const char *alias) {
  app_json_write_indent(stream, level);
  fputs("\"aliases\": [", stream);
  if (alias && alias[0] != '\0') {
    app_json_write_string(stream, alias);
  }
  fputs("],\n", stream);
}

static void opencli_print_arguments(FILE *stream, int level,
                                    const app_command_arg_t *arguments,
                                    size_t count, bool comma) {
  app_json_write_indent(stream, level);
  fputs("\"arguments\": [", stream);
  if (count > 0) {
    fputc('\n', stream);
    for (size_t i = 0; i < count; i++) {
      const app_command_arg_t *arg = &arguments[i];
      const int item_level = level + 1;
      app_json_write_indent(stream, item_level);
      fputs("{\n", stream);
      app_json_write_pretty_string_field(stream, item_level + 1, "name",
                                         arg->name, true);
      app_json_write_pretty_bool_field(stream, item_level + 1, "required",
                                       arg->required, true);
      app_json_write_pretty_int_field(stream, item_level + 1, "ordinal",
                                      arg->ordinal, true);
      app_json_write_indent(stream, item_level + 1);
      fputs("\"arity\": {\n", stream);
      app_json_write_pretty_int_field(stream, item_level + 2, "minimum",
                                      arg->arity_minimum, true);
      if (arg->arity_maximum == APP_ARG_ARITY_UNBOUNDED) {
        app_json_write_pretty_null_field(stream, item_level + 2, "maximum",
                                         false);
      } else {
        app_json_write_pretty_int_field(stream, item_level + 2, "maximum",
                                        arg->arity_maximum, false);
      }
      app_json_write_indent(stream, item_level + 1);
      fputs("},\n", stream);
      app_json_write_pretty_string_field(stream, item_level + 1, "description",
                                         arg->description, false);
      app_json_write_indent(stream, item_level);
      fputc('}', stream);
      if (i + 1 < count) {
        fputc(',', stream);
      }
      fputc('\n', stream);
    }
    app_json_write_indent(stream, level);
  }
  fputc(']', stream);
  if (comma) {
    fputc(',', stream);
  }
  fputc('\n', stream);
}

static void opencli_print_option(FILE *stream, int level, const char *name,
                                 bool required, const char *alias,
                                 const app_command_arg_t *arguments,
                                 size_t argument_count, const char *description,
                                 bool comma) {
  app_json_write_indent(stream, level);
  fputs("{\n", stream);
  app_json_write_pretty_string_field(stream, level + 1, "name", name, true);
  app_json_write_pretty_bool_field(stream, level + 1, "required", required,
                                   true);
  opencli_print_aliases(stream, level + 1, alias);
  opencli_print_arguments(stream, level + 1, arguments, argument_count, true);
  app_json_write_pretty_string_field(stream, level + 1, "description",
                                     description, false);
  app_json_write_indent(stream, level);
  fputc('}', stream);
  if (comma) {
    fputc(',', stream);
  }
  fputc('\n', stream);
}

static void opencli_print_contract_option(FILE *stream,
                                          const app_opencli_option_t *option,
                                          bool comma) {
  opencli_print_option(stream, 2, option->name, option->required, option->alias,
                       option->arguments, option->argument_count,
                       option->description, comma);
}

static void opencli_print_builtin_option(FILE *stream,
                                         const app_builtin_option_t *option,
                                         bool comma) {
  opencli_print_option(stream, 2, option->name, false, option->alias, NULL, 0,
                       option->description, comma);
}

static void opencli_print_flag_option(FILE *stream, const app_flag_spec_t *flag,
                                      bool comma) {
  opencli_print_option(stream, 2, opencli_option_name(flag->cli_long), false,
                       opencli_short_alias(flag->cli_short), NULL, 0,
                       flag->description, comma);
}

static void opencli_print_options(FILE *stream,
                                  const app_opencli_contract_t *contract) {
  size_t builtin_count = 0;
  const app_builtin_option_t *builtins = app_builtin_options(&builtin_count);
  size_t flag_count = 0;
  const app_flag_spec_t *flags = app_flag_table(&flag_count);
  const size_t total =
      builtin_count + flag_count + contract->trailing_option_count;
  size_t printed = 0;

  fputs("  \"options\": [\n", stream);
  for (size_t i = 0; i < builtin_count; i++) {
    opencli_print_builtin_option(stream, &builtins[i], ++printed < total);
  }
  for (size_t i = 0; i < flag_count; i++) {
    opencli_print_flag_option(stream, &flags[i], ++printed < total);
  }
  for (size_t i = 0; i < contract->trailing_option_count; i++) {
    opencli_print_contract_option(stream, &contract->trailing_options[i],
                                  ++printed < total);
  }
  fputs("  ],\n", stream);
}

static void opencli_print_examples_array(FILE *stream, int level,
                                         const char *const *examples,
                                         size_t count, bool comma) {
  app_json_write_indent(stream, level);
  fputs("\"examples\": [", stream);
  if (count > 0) {
    fputc('\n', stream);
    for (size_t i = 0; i < count; i++) {
      app_json_write_indent(stream, level + 1);
      app_json_write_string(stream, examples[i]);
      if (i + 1 < count) {
        fputc(',', stream);
      }
      fputc('\n', stream);
    }
    app_json_write_indent(stream, level);
  }
  fputc(']', stream);
  if (comma) {
    fputc(',', stream);
  }
  fputc('\n', stream);
}

static void opencli_print_command(FILE *stream, const app_command_t *command,
                                  bool comma) {
  app_json_write_indent(stream, 2);
  fputs("{\n", stream);
  app_json_write_pretty_string_field(stream, 3, "name", command->name, true);
  app_json_write_pretty_string_field(
      stream, 3, "description", command->summary ? command->summary : "", true);

  app_json_write_indent(stream, 3);
  fputs("\"options\": [", stream);
  if (command->option_count > 0) {
    fputc('\n', stream);
    for (size_t i = 0; i < command->option_count; i++) {
      const app_command_option_t *option = &command->options[i];
      opencli_print_option(stream, 4, option->name, false, NULL, NULL, 0,
                           option->description, i + 1 < command->option_count);
    }
    app_json_write_indent(stream, 3);
  }
  fputs("],\n", stream);

  opencli_print_arguments(stream, 3, command->arguments,
                          command->argument_count, true);
  opencli_print_examples_array(stream, 3, command->examples,
                               command->example_count,
                               command->requires_terminal);
  if (command->requires_terminal) {
    app_json_write_indent(stream, 3);
    fputs("\"metadata\": {\n", stream);
    app_json_write_pretty_string_field(stream, 4, "requires", "ncurses", true);
    app_json_write_pretty_bool_field(stream, 4, "interactive", true, false);
    app_json_write_indent(stream, 3);
    fputs("}\n", stream);
  }

  app_json_write_indent(stream, 2);
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
  size_t count = 0;
  const app_error_info_t *errors = app_error_table(&count);

  fputs("  \"exitCodes\": [\n", stream);
  for (size_t i = 0; i < count; i++) {
    app_json_write_indent(stream, 2);
    fputs("{\n", stream);
    app_json_write_pretty_int_field(stream, 3, "code", errors[i].code, true);
    app_json_write_pretty_string_field(stream, 3, "description",
                                       errors[i].description, false);
    app_json_write_indent(stream, 2);
    fputc('}', stream);
    if (i + 1 < count) {
      fputc(',', stream);
    }
    fputc('\n', stream);
  }
  fputs("  ],\n", stream);
}

static size_t opencli_top_example_count(
    const app_opencli_contract_t *contract) {
  size_t count = contract->extra_example_count;
  size_t command_count = 0;
  const app_command_t *commands = app_commands(&command_count);
  for (size_t i = 0; i < command_count; i++) {
    count += commands[i].example_count;
  }
  return count;
}

static void opencli_print_top_examples(FILE *stream,
                                       const app_opencli_contract_t *contract) {
  const size_t total = opencli_top_example_count(contract);
  size_t printed = 0;
  size_t command_count = 0;
  const app_command_t *commands = app_commands(&command_count);

  fputs("  \"examples\": [", stream);
  if (total > 0) {
    fputc('\n', stream);
    for (size_t i = 0; i < command_count; i++) {
      for (size_t j = 0; j < commands[i].example_count; j++) {
        app_json_write_indent(stream, 2);
        app_json_write_string(stream, commands[i].examples[j]);
        if (++printed < total) {
          fputc(',', stream);
        }
        fputc('\n', stream);
      }
    }
    for (size_t i = 0; i < contract->extra_example_count; i++) {
      app_json_write_indent(stream, 2);
      app_json_write_string(stream, contract->extra_examples[i]);
      if (++printed < total) {
        fputc(',', stream);
      }
      fputc('\n', stream);
    }
    app_json_write_indent(stream, 1);
  }
  fputs("],\n", stream);
}

static void opencli_print_metadata(FILE *stream,
                                   const app_opencli_contract_t *contract) {
  fputs("  \"metadata\": [\n", stream);
  for (size_t i = 0; i < contract->metadata_count; i++) {
    const app_opencli_metadata_group_t *group = &contract->metadata[i];
    app_json_write_indent(stream, 2);
    fputs("{\n", stream);
    app_json_write_pretty_string_field(stream, 3, "name", group->name, true);
    app_json_write_indent(stream, 3);
    fputs("\"value\": {\n", stream);
    for (size_t j = 0; j < group->field_count; j++) {
      app_json_write_pretty_string_field(stream, 4, group->fields[j].name,
                                         group->fields[j].description,
                                         j + 1 < group->field_count);
    }
    app_json_write_indent(stream, 3);
    fputs("}\n", stream);
    app_json_write_indent(stream, 2);
    fputc('}', stream);
    if (i + 1 < contract->metadata_count) {
      fputc(',', stream);
    }
    fputc('\n', stream);
  }
  fputs("  ]\n", stream);
}

static void opencli_print_info(FILE *stream,
                               const app_opencli_contract_t *contract) {
  fputs("  \"info\": {\n", stream);
  app_json_write_pretty_string_field(stream, 2, "title", contract->info.title,
                                     true);
  app_json_write_pretty_string_field(stream, 2, "description",
                                     contract->info.description, true);
  app_json_write_pretty_string_field(stream, 2, "version",
                                     contract->info.version, true);
  fputs("    \"contact\": {\n", stream);
  app_json_write_pretty_string_field(stream, 3, "name",
                                     contract->info.contact.name, true);
  app_json_write_pretty_string_field(stream, 3, "url",
                                     contract->info.contact.url, false);
  fputs("    },\n", stream);
  fputs("    \"license\": {\n", stream);
  app_json_write_pretty_string_field(stream, 3, "name",
                                     contract->info.license.name, true);
  app_json_write_pretty_string_field(stream, 3, "identifier",
                                     contract->info.license.identifier, false);
  fputs("    }\n", stream);
  fputs("  },\n", stream);
}

static void opencli_print_conventions(FILE *stream,
                                      const app_opencli_contract_t *contract) {
  fputs("  \"conventions\": {\n", stream);
  app_json_write_pretty_bool_field(stream, 2, "groupOptions",
                                   contract->conventions.group_options, true);
  app_json_write_pretty_string_field(
      stream, 2, "optionArgumentSeparator",
      contract->conventions.option_argument_separator, false);
  fputs("  },\n", stream);
}

app_error app_cmd_opencli(const app_config_t *config, int argc, char **argv) {
  (void)config;
  (void)argc;
  (void)argv;

  const app_opencli_contract_t *contract = app_opencli_contract();

  fputs("{\n", stdout);
  app_json_write_pretty_string_field(stdout, 1, "opencli",
                                     contract->opencli_version, true);
  opencli_print_info(stdout, contract);
  opencli_print_conventions(stdout, contract);
  opencli_print_arguments(stdout, 1, contract->root_arguments,
                          contract->root_argument_count, true);
  opencli_print_options(stdout, contract);
  opencli_print_commands(stdout);
  opencli_print_exit_codes(stdout);
  opencli_print_top_examples(stdout, contract);
  app_json_write_pretty_bool_field(stdout, 1, "interactive",
                                   contract->interactive, true);
  opencli_print_metadata(stdout, contract);
  fputs("}\n", stdout);

  return APP_SUCCESS;
}
