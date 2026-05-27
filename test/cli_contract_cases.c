/*
 * CLI contract test cases. Each function returns true on pass, false on fail
 * and prints a TAP-friendly diagnostic on failure.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/core/error.h"
#include "cli_contract.h"

#ifdef _WIN32
#include <io.h>
#define unlink _unlink
#else
#include <unistd.h>
#endif

static bool test_installed_binary_starts(test_context_t *ctx) {
  if (!cc_file_exists(ctx->binary)) {
    fprintf(stderr, "binary does not exist: %s\n", ctx->binary);
    return false;
  }

  const char *args[] = {"--version"};
  command_result_t result = cc_run_cli(ctx, args, ARRAY_LEN(args), NULL, 0);
  const bool ok =
      cc_expect_exit(&result, 0) && result.out && result.out[0] != '\0';
  if (!ok && result.out && result.out[0] == '\0') {
    fprintf(stderr, "expected version output\n");
  }
  cc_command_result_free(&result);
  return ok;
}

static bool test_help_is_human_readable(test_context_t *ctx) {
  const char *args[] = {"--help"};
  command_result_t result = cc_run_cli(ctx, args, ARRAY_LEN(args), NULL, 0);
  const bool ok = cc_expect_exit(&result, 0) &&
                  cc_expect_stdout_contains(&result, "USAGE") &&
                  cc_expect_stdout_contains(&result, "COMMANDS") &&
                  cc_expect_stdout_contains(&result, "doctor");
  cc_command_result_free(&result);
  return ok;
}

static bool test_builtins_render_expected_output(test_context_t *ctx) {
  bool ok = true;

  {
    const char *args[] = {"hello"};
    command_result_t result = cc_run_cli(ctx, args, ARRAY_LEN(args), NULL, 0);
    ok = cc_expect_exit(&result, 0) &&
         cc_expect_stdout_contains(&result, "Hello, World!") && ok;
    cc_command_result_free(&result);
  }

  {
    const char *args[] = {"hello", "Alice"};
    command_result_t result = cc_run_cli(ctx, args, ARRAY_LEN(args), NULL, 0);
    ok = cc_expect_exit(&result, 0) &&
         cc_expect_stdout_contains(&result, "Hello, Alice!") && ok;
    cc_command_result_free(&result);
  }

  {
    const char *args[] = {"echo", "test", "message"};
    command_result_t result = cc_run_cli(ctx, args, ARRAY_LEN(args), NULL, 0);
    ok = cc_expect_exit(&result, 0) &&
         cc_expect_stdout_contains(&result, "test message") && ok;
    cc_command_result_free(&result);
  }

  {
    const char *args[] = {"info"};
    command_result_t result = cc_run_cli(ctx, args, ARRAY_LEN(args), NULL, 0);
    ok = cc_expect_exit(&result, 0) &&
         cc_expect_stdout_contains(&result, "Application:") &&
         cc_expect_stdout_contains(&result, "Version:") && ok;
    cc_command_result_free(&result);
  }

  return ok;
}

static bool test_json_info_is_versioned_machine_output(test_context_t *ctx) {
  const char *args[] = {"--json", "info"};
  command_result_t result = cc_run_cli(ctx, args, ARRAY_LEN(args), NULL, 0);
  const bool ok =
      cc_expect_exit(&result, 0) &&
      cc_expect_stdout_contains(&result, "\"format_version\":\"1.0\"") &&
      cc_expect_stdout_contains(&result, "\"features\"") &&
      cc_expect_stdout_contains(&result, "\"tui\"");
  cc_command_result_free(&result);
  return ok;
}

static bool test_quiet_json_commands_suppress_stdout(test_context_t *ctx) {
  bool ok = true;

  {
    const char *args[] = {"--quiet", "--json", "info"};
    command_result_t result = cc_run_cli(ctx, args, ARRAY_LEN(args), NULL, 0);
    ok = cc_expect_exit(&result, 0) && cc_expect_stdout_empty(&result) && ok;
    cc_command_result_free(&result);
  }

  {
    const char *args[] = {"--quiet", "--json", "doctor"};
    command_result_t result = cc_run_cli(ctx, args, ARRAY_LEN(args), NULL, 0);
    ok = cc_expect_exit(&result, 0) && cc_expect_stdout_empty(&result) && ok;
    cc_command_result_free(&result);
  }

  return ok;
}

static bool test_doctor_reports_binary_state(test_context_t *ctx) {
  const char *args[] = {"doctor"};
  command_result_t result = cc_run_cli(ctx, args, ARRAY_LEN(args), NULL, 0);
  const bool ok = cc_expect_exit(&result, 0) &&
                  cc_expect_stdout_contains(&result, "doctor") &&
                  cc_expect_stdout_contains(&result, "binary");
  cc_command_result_free(&result);
  return ok;
}

static bool test_plain_mode_disables_forced_color(test_context_t *ctx) {
  const char *args[] = {"--plain", "doctor"};
  const env_var_t env[] = {{"FORCE_COLOR", "1"}};
  command_result_t result =
      cc_run_cli(ctx, args, ARRAY_LEN(args), env, ARRAY_LEN(env));
  const bool ok =
      cc_expect_exit(&result, 0) &&
      cc_expect_stdout_contains(&result, "color_output") &&
      cc_expect_stdout_contains(&result, "disabled for this output");
  cc_command_result_free(&result);
  return ok;
}

static bool test_command_arguments_are_not_global_config_flags(
    test_context_t *ctx) {
  const char *args[] = {"echo", "-c", "/definitely/not/a/config.json"};
  command_result_t result = cc_run_cli(ctx, args, ARRAY_LEN(args), NULL, 0);
  const bool ok =
      cc_expect_exit(&result, 0) &&
      cc_expect_stdout_contains(&result, "-c /definitely/not/a/config.json");
  cc_command_result_free(&result);
  return ok;
}

static bool test_explicit_config_file_failures_are_visible(
    test_context_t *ctx) {
  const char *args[] = {"--config", "/definitely/not/a/config.json", "hello"};
  command_result_t result = cc_run_cli(ctx, args, ARRAY_LEN(args), NULL, 0);
  const bool ok =
      cc_expect_not_exit(&result, 0) &&
      cc_expect_stderr_contains(&result, "failed to load config") &&
      cc_expect_stderr_contains(&result, "/definitely/not/a/config.json");
  cc_command_result_free(&result);
  return ok;
}

static bool test_verbose_mode_emits_diagnostics_on_stderr(test_context_t *ctx) {
  const char *args[] = {"--verbose", "hello"};
  command_result_t result = cc_run_cli(ctx, args, ARRAY_LEN(args), NULL, 0);
  const bool ok = cc_expect_exit(&result, 0) &&
                  cc_expect_stdout_contains(&result, "Hello, World!") &&
                  cc_expect_stderr_contains(&result, "[INFO]");
  cc_command_result_free(&result);
  return ok;
}

static bool test_invalid_default_config_does_not_leak_partial_settings(
    test_context_t *ctx) {
  char *config_path = NULL;
  if (!cc_write_temp_config("{\"quiet\":true,\"ignored\":{\"nested\":true}}",
                            &config_path)) {
    fprintf(stderr, "failed to write temporary config\n");
    return false;
  }

  const char *args[] = {"hello"};
  const env_var_t env[] = {{"APP_CONFIG_PATH", config_path}};
  command_result_t result =
      cc_run_cli(ctx, args, ARRAY_LEN(args), env, ARRAY_LEN(env));
  const bool ok = cc_expect_exit(&result, 0) &&
                  cc_expect_stdout_contains(&result, "Hello, World!");
  cc_command_result_free(&result);
  (void)unlink(config_path);
  free(config_path);
  return ok;
}

static bool test_valid_flat_config_skips_unknown_scalar_keys(
    test_context_t *ctx) {
  char *config_path = NULL;
  if (!cc_write_temp_config("{\"ignored\":\"debug\",\"quiet\":true}",
                            &config_path)) {
    fprintf(stderr, "failed to write temporary config\n");
    return false;
  }

  const char *args[] = {"--config", config_path, "hello"};
  command_result_t result = cc_run_cli(ctx, args, ARRAY_LEN(args), NULL, 0);
  const bool ok = cc_expect_exit(&result, 0) && cc_expect_stdout_empty(&result);
  cc_command_result_free(&result);
  (void)unlink(config_path);
  free(config_path);
  return ok;
}

static bool test_unknown_command_reports_actionable_error(test_context_t *ctx) {
  const char *args[] = {"not-a-command"};
  command_result_t result = cc_run_cli(ctx, args, ARRAY_LEN(args), NULL, 0);
  const bool ok =
      cc_expect_not_exit(&result, 0) &&
      cc_expect_stderr_contains(&result, "Unknown command: not-a-command") &&
      cc_expect_stderr_contains(&result, "--help");
  cc_command_result_free(&result);
  return ok;
}

static bool test_terminal_command_requires_tty(test_context_t *ctx) {
  const char *args[] = {"menu"};
  command_result_t result = cc_run_cli(ctx, args, ARRAY_LEN(args), NULL, 0);
  const bool ok =
      cc_expect_exit(&result, APP_ERROR_IO) &&
      cc_expect_stderr_contains(&result, "requires an interactive terminal");
  cc_command_result_free(&result);
  return ok;
}

static const char *test_path_basename(const char *path) {
  const char *base = path;
  for (const char *p = path; p && *p; p++) {
    if (*p == '/' || *p == '\\') {
      base = p + 1;
    }
  }
  return base;
}

static char *test_binary_name(const char *path) {
  const char *base = test_path_basename(path);
  char *name = cc_copy_string(base);
  if (!name) {
    return NULL;
  }
  size_t len = strlen(name);
  if (len > 4 && strcmp(name + len - 4, ".exe") == 0) {
    name[len - 4] = '\0';
  }
  return name;
}

static char *test_replace_all(const char *input, const char *needle,
                              const char *replacement) {
  const size_t needle_len = strlen(needle);
  if (needle_len == 0) {
    return cc_copy_string(input);
  }
  const size_t replacement_len = strlen(replacement);
  size_t count = 0;

  for (const char *p = input; (p = strstr(p, needle)) != NULL;
       p += needle_len) {
    count++;
  }

  const size_t input_len = strlen(input);
  const size_t output_len =
      replacement_len >= needle_len
          ? input_len + count * (replacement_len - needle_len)
          : input_len - count * (needle_len - replacement_len);
  char *output = malloc(output_len + 1);
  if (!output) {
    return NULL;
  }

  char *dst = output;
  const char *src = input;
  const char *match = NULL;
  while ((match = strstr(src, needle)) != NULL) {
    const size_t prefix_len = (size_t)(match - src);
    memcpy(dst, src, prefix_len);
    dst += prefix_len;
    memcpy(dst, replacement, replacement_len);
    dst += replacement_len;
    src = match + needle_len;
  }
  strcpy(dst, src);
  return output;
}

static void test_strip_carriage_returns(char *text) {
  char *dst = text;
  for (const char *src = text; src && *src; src++) {
    if (*src != '\r') {
      *dst++ = *src;
    }
  }
  if (dst) {
    *dst = '\0';
  }
}

static bool test_opencli_contract_matches_checked_in_spec(test_context_t *ctx) {
  const char *args[] = {"opencli"};
  command_result_t result = cc_run_cli(ctx, args, ARRAY_LEN(args), NULL, 0);
  char *expected = cc_read_text_file("opencli.json");
  char *binary_name = test_binary_name(ctx->binary);
  char *normalized_expected = NULL;

  bool ok = cc_expect_exit(&result, 0) &&
            cc_expect_stdout_contains(&result, "\"opencli\": \"0.1\"") &&
            cc_expect_stdout_contains(&result, "\"name\": \"opencli\"");
  if (!expected) {
    fprintf(stderr, "failed to read opencli.json\n");
    ok = false;
  } else if (!binary_name) {
    fprintf(stderr, "failed to determine binary name\n");
    ok = false;
  } else {
    normalized_expected =
        strcmp(binary_name, "myapp") == 0
            ? cc_copy_string(expected)
            : test_replace_all(expected, "myapp", binary_name);
    if (!normalized_expected) {
      fprintf(stderr, "failed to normalize opencli.json\n");
      ok = false;
    }
  }

  if (ok) {
    test_strip_carriage_returns(result.out);
    test_strip_carriage_returns(normalized_expected);
  }
  if (ok && strcmp(result.out ? result.out : "", normalized_expected) != 0) {
    fprintf(stderr, "opencli command output does not match opencli.json\n");
    ok = false;
  }

  free(normalized_expected);
  free(binary_name);
  free(expected);
  cc_command_result_free(&result);
  return ok;
}

const test_case_t cli_contract_cases[] = {
    {"installed binary starts", test_installed_binary_starts},
    {"help is human readable", test_help_is_human_readable},
    {"builtins render expected output", test_builtins_render_expected_output},
    {"json info is versioned machine output",
     test_json_info_is_versioned_machine_output},
    {"quiet json commands suppress stdout",
     test_quiet_json_commands_suppress_stdout},
    {"doctor reports binary state", test_doctor_reports_binary_state},
    {"plain mode disables forced color", test_plain_mode_disables_forced_color},
    {"command arguments are not global config flags",
     test_command_arguments_are_not_global_config_flags},
    {"explicit config file failures are visible",
     test_explicit_config_file_failures_are_visible},
    {"verbose mode emits diagnostics on stderr",
     test_verbose_mode_emits_diagnostics_on_stderr},
    {"invalid default config does not leak partial settings",
     test_invalid_default_config_does_not_leak_partial_settings},
    {"valid flat config skips unknown scalar keys",
     test_valid_flat_config_skips_unknown_scalar_keys},
    {"unknown command reports actionable error",
     test_unknown_command_reports_actionable_error},
    {"terminal commands require a tty", test_terminal_command_requires_tty},
    {"opencli contract matches checked-in spec",
     test_opencli_contract_matches_checked_in_spec},
};

const size_t cli_contract_cases_count = ARRAY_LEN(cli_contract_cases);
