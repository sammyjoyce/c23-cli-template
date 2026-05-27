#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_harness.h"

#ifdef _WIN32
#include <io.h>
#define unlink _unlink
#else
#include <unistd.h>
#endif

typedef bool (*test_fn_t)(test_context_t *ctx);

typedef struct {
  const char *name;
  test_fn_t fn;
} test_case_t;

static bool test_installed_binary_starts(test_context_t *ctx) {
  if (!file_exists(ctx->binary)) {
    fprintf(stderr, "binary does not exist: %s\n", ctx->binary);
    return false;
  }

  const char *args[] = {"--version"};
  command_result_t result = run_cli(ctx, args, ARRAY_LEN(args), NULL, 0);
  const bool ok =
      expect_exit(&result, 0) && result.out && result.out[0] != '\0';
  if (!ok && result.out && result.out[0] == '\0') {
    fprintf(stderr, "expected version output\n");
    print_result_tail(&result);
  }
  command_result_free(&result);
  return ok;
}

static bool test_help_is_human_readable(test_context_t *ctx) {
  const char *args[] = {"--help"};
  command_result_t result = run_cli(ctx, args, ARRAY_LEN(args), NULL, 0);
  const bool ok = expect_exit(&result, 0) &&
                  expect_stdout_contains(&result, "USAGE") &&
                  expect_stdout_contains(&result, "COMMANDS") &&
                  expect_stdout_contains(&result, "doctor");
  command_result_free(&result);
  return ok;
}

static bool test_builtins_render_expected_output(test_context_t *ctx) {
  bool ok = true;

  {
    const char *args[] = {"hello"};
    command_result_t result = run_cli(ctx, args, ARRAY_LEN(args), NULL, 0);
    ok = expect_exit(&result, 0) &&
         expect_stdout_contains(&result, "Hello, World!") && ok;
    command_result_free(&result);
  }

  {
    const char *args[] = {"hello", "Alice"};
    command_result_t result = run_cli(ctx, args, ARRAY_LEN(args), NULL, 0);
    ok = expect_exit(&result, 0) &&
         expect_stdout_contains(&result, "Hello, Alice!") && ok;
    command_result_free(&result);
  }

  {
    const char *args[] = {"echo", "test", "message"};
    command_result_t result = run_cli(ctx, args, ARRAY_LEN(args), NULL, 0);
    ok = expect_exit(&result, 0) &&
         expect_stdout_contains(&result, "test message") && ok;
    command_result_free(&result);
  }

  {
    const char *args[] = {"info"};
    command_result_t result = run_cli(ctx, args, ARRAY_LEN(args), NULL, 0);
    ok = expect_exit(&result, 0) &&
         expect_stdout_contains(&result, "Application:") &&
         expect_stdout_contains(&result, "Version:") && ok;
    command_result_free(&result);
  }

  return ok;
}

static bool test_json_info_is_versioned_machine_output(test_context_t *ctx) {
  const char *args[] = {"--json", "info"};
  command_result_t result = run_cli(ctx, args, ARRAY_LEN(args), NULL, 0);
  const bool ok =
      expect_exit(&result, 0) &&
      expect_stdout_contains(&result, "\"format_version\":\"1.0\"") &&
      expect_stdout_contains(&result, "\"features\"") &&
      expect_stdout_contains(&result, "\"tui\"");
  command_result_free(&result);
  return ok;
}

static bool test_quiet_json_commands_suppress_stdout(test_context_t *ctx) {
  bool ok = true;

  {
    const char *args[] = {"--quiet", "--json", "info"};
    command_result_t result = run_cli(ctx, args, ARRAY_LEN(args), NULL, 0);
    ok = expect_exit(&result, 0) && expect_stdout_empty(&result) && ok;
    command_result_free(&result);
  }

  {
    const char *args[] = {"--quiet", "--json", "doctor"};
    command_result_t result = run_cli(ctx, args, ARRAY_LEN(args), NULL, 0);
    ok = expect_exit(&result, 0) && expect_stdout_empty(&result) && ok;
    command_result_free(&result);
  }

  return ok;
}

static bool test_doctor_reports_binary_state(test_context_t *ctx) {
  const char *args[] = {"doctor"};
  command_result_t result = run_cli(ctx, args, ARRAY_LEN(args), NULL, 0);
  const bool ok = expect_exit(&result, 0) &&
                  expect_stdout_contains(&result, "doctor") &&
                  expect_stdout_contains(&result, "binary");
  command_result_free(&result);
  return ok;
}

static bool test_plain_mode_disables_forced_color(test_context_t *ctx) {
  const char *args[] = {"--plain", "doctor"};
  const env_var_t env[] = {{"FORCE_COLOR", "1"}};
  command_result_t result =
      run_cli(ctx, args, ARRAY_LEN(args), env, ARRAY_LEN(env));
  const bool ok = expect_exit(&result, 0) &&
                  expect_stdout_contains(&result, "color_output") &&
                  expect_stdout_contains(&result, "disabled for this output");
  command_result_free(&result);
  return ok;
}

static bool test_command_arguments_are_not_global_config_flags(
    test_context_t *ctx) {
  const char *args[] = {"echo", "-c", "/definitely/not/a/config.json"};
  command_result_t result = run_cli(ctx, args, ARRAY_LEN(args), NULL, 0);
  const bool ok =
      expect_exit(&result, 0) &&
      expect_stdout_contains(&result, "-c /definitely/not/a/config.json");
  command_result_free(&result);
  return ok;
}

static bool test_explicit_config_file_failures_are_visible(
    test_context_t *ctx) {
  const char *args[] = {"--config", "/definitely/not/a/config.json", "hello"};
  command_result_t result = run_cli(ctx, args, ARRAY_LEN(args), NULL, 0);
  const bool ok =
      expect_not_exit(&result, 0) &&
      expect_stderr_contains(&result, "failed to load config") &&
      expect_stderr_contains(&result, "/definitely/not/a/config.json");
  command_result_free(&result);
  return ok;
}

static bool test_verbose_mode_emits_diagnostics_on_stderr(test_context_t *ctx) {
  const char *args[] = {"--verbose", "hello"};
  command_result_t result = run_cli(ctx, args, ARRAY_LEN(args), NULL, 0);
  const bool ok = expect_exit(&result, 0) &&
                  expect_stdout_contains(&result, "Hello, World!") &&
                  expect_stderr_contains(&result, "[INFO]");
  command_result_free(&result);
  return ok;
}

static bool test_invalid_default_config_does_not_leak_partial_settings(
    test_context_t *ctx) {
  char *config_path = NULL;
  if (!write_temp_config("{\"quiet\":true,\"ignored\":{\"nested\":true}}",
                         &config_path)) {
    fprintf(stderr, "failed to write temporary config\n");
    return false;
  }

  const char *args[] = {"hello"};
  const env_var_t env[] = {{"APP_CONFIG_PATH", config_path}};
  command_result_t result =
      run_cli(ctx, args, ARRAY_LEN(args), env, ARRAY_LEN(env));
  const bool ok = expect_exit(&result, 0) &&
                  expect_stdout_contains(&result, "Hello, World!");
  command_result_free(&result);
  (void)unlink(config_path);
  free(config_path);
  return ok;
}

static bool test_valid_flat_config_skips_unknown_scalar_keys(
    test_context_t *ctx) {
  char *config_path = NULL;
  if (!write_temp_config("{\"ignored\":\"debug\",\"quiet\":true}",
                         &config_path)) {
    fprintf(stderr, "failed to write temporary config\n");
    return false;
  }

  const char *args[] = {"--config", config_path, "hello"};
  command_result_t result = run_cli(ctx, args, ARRAY_LEN(args), NULL, 0);
  const bool ok = expect_exit(&result, 0) && expect_stdout_empty(&result);
  command_result_free(&result);
  (void)unlink(config_path);
  free(config_path);
  return ok;
}

static bool test_unknown_command_reports_actionable_error(test_context_t *ctx) {
  const char *args[] = {"not-a-command"};
  command_result_t result = run_cli(ctx, args, ARRAY_LEN(args), NULL, 0);
  const bool ok =
      expect_not_exit(&result, 0) &&
      expect_stderr_contains(&result, "Unknown command: not-a-command") &&
      expect_stderr_contains(&result, "--help");
  command_result_free(&result);
  return ok;
}

static void run_test(test_context_t *ctx, const test_case_t *test) {
  if (test->fn(ctx)) {
    ctx->passed++;
    printf("ok %d - %s\n", ctx->passed + ctx->failed, test->name);
  } else {
    ctx->failed++;
    printf("not ok %d - %s\n", ctx->passed + ctx->failed, test->name);
  }
}

static void usage(const char *argv0) {
  fprintf(stderr, "usage: %s --binary PATH\n", argv0);
}

int main(int argc, char **argv) {
  const char *binary = NULL;
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--binary") == 0 && i + 1 < argc) {
      binary = argv[++i];
    } else {
      usage(argv[0]);
      fprintf(stderr, "unknown arg: %s\n", argv[i]);
      return 2;
    }
  }
  if (!binary) {
    usage(argv[0]);
    return 2;
  }

  test_context_t ctx = {.binary = binary, .passed = 0, .failed = 0};
  const test_case_t tests[] = {
      {"installed binary starts", test_installed_binary_starts},
      {"help is human readable", test_help_is_human_readable},
      {"builtins render expected output", test_builtins_render_expected_output},
      {"json info is versioned machine output",
       test_json_info_is_versioned_machine_output},
      {"quiet json commands suppress stdout",
       test_quiet_json_commands_suppress_stdout},
      {"doctor reports binary state", test_doctor_reports_binary_state},
      {"plain mode disables forced color",
       test_plain_mode_disables_forced_color},
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
  };

  printf("TAP version 13\n");
  for (size_t i = 0; i < ARRAY_LEN(tests); i++) {
    run_test(&ctx, &tests[i]);
  }
  printf("1..%zu\n", ARRAY_LEN(tests));
  fprintf(stderr, "%d passed, %d failed\n", ctx.passed, ctx.failed);
  return ctx.failed == 0 ? 0 : 1;
}
