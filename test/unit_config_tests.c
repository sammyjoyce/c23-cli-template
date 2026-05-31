/*
 * Unit tests for core config, error, color, and memory helpers.
 */

#include <stdlib.h>
#include <string.h>

#include "../src/core/config_json.h"
#include "../src/core/error.h"
#include "../src/core/request_json.h"
#include "../src/utils/colors.h"
#include "../src/utils/memory.h"
#include "unit_support.h"

#ifndef _WIN32
static bool test_config_env_no_color_empty_sets_flag(void) {
  const char *previous = getenv("NO_COLOR");
  char *previous_copy = previous ? strdup(previous) : NULL;
  if (previous && !previous_copy) {
    return false;
  }

  bool ok = setenv("NO_COLOR", "", 1) == 0;
  app_config_t *config = NULL;
  if (ok) {
    ok = app_config_create(&config) == APP_SUCCESS;
  }
  if (ok) {
    ok = app_config_load_env(config) == APP_SUCCESS &&
         app_config_is_no_color(config);
  }

  app_config_destroy(config);
  if (previous_copy) {
    (void)setenv("NO_COLOR", previous_copy, 1);
  } else {
    (void)unsetenv("NO_COLOR");
  }
  free(previous_copy);
  return ok;
}

static bool test_use_colors_honors_no_color_without_env_load(void) {
  const char *previous = getenv("NO_COLOR");
  char *previous_copy = previous ? strdup(previous) : NULL;
  if (previous && !previous_copy) {
    return false;
  }

  bool ok = setenv("NO_COLOR", "", 1) == 0;
  app_config_t *config = NULL;
  if (ok) {
    ok = app_config_create(&config) == APP_SUCCESS;
  }
  if (ok) {
    ok = !app_use_colors(config);
  }

  app_config_destroy(config);
  if (previous_copy) {
    (void)setenv("NO_COLOR", previous_copy, 1);
  } else {
    (void)unsetenv("NO_COLOR");
  }
  free(previous_copy);
  return ok;
}
#endif

static bool test_strerror_covers_every_code(void) {
  size_t count = 0;
  const app_error_info_t *errors = app_error_table(&count);
  if (!errors || count == 0) {
    return false;
  }

  for (size_t i = 0; i < count; i++) {
    const char *msg = app_strerror(errors[i].code);
    if (!msg || msg[0] == '\0') {
      fprintf(stderr, "app_strerror returned empty for code %d\n",
              errors[i].code);
      return false;
    }
    if (strcmp(msg, "Unknown error") == 0) {
      fprintf(stderr, "code %d hit the default branch\n", errors[i].code);
      return false;
    }
  }
  return true;
}

static bool test_config_json_parses_valid_input(void) {
  app_config_json_state_t state = {0};
  const char *input = "{\"debug\":true,\"quiet\":false}";
  if (app_config_parse_json_state(&state, input) != APP_SUCCESS) {
    return false;
  }
  return state.values[APP_FLAG_DEBUG] && !state.values[APP_FLAG_QUIET];
}

static bool test_config_json_rejects_unicode_escape(void) {
  app_config_json_state_t state = {0};
  const char *input = "{\"debug\":\"\\u00e9\"}";
  return app_config_parse_json_state(&state, input) != APP_SUCCESS;
}

static bool test_config_json_rejects_trailing_garbage(void) {
  app_config_json_state_t state = {0};
  const char *input = "{}garbage";
  return app_config_parse_json_state(&state, input) != APP_SUCCESS;
}

static bool test_config_json_rejects_long_keys(void) {
  char input[96];
  memset(input, 'a', sizeof(input));
  input[0] = '{';
  input[1] = '"';
  input[sizeof(input) - 8] = '"';
  input[sizeof(input) - 7] = ':';
  input[sizeof(input) - 6] = 't';
  input[sizeof(input) - 5] = 'r';
  input[sizeof(input) - 4] = 'u';
  input[sizeof(input) - 3] = 'e';
  input[sizeof(input) - 2] = '}';
  input[sizeof(input) - 1] = '\0';

  app_config_json_state_t state = {0};
  return app_config_parse_json_state(&state, input) == APP_ERROR_OUT_OF_RANGE;
}

static bool test_config_json_output_exclusivity(void) {
  app_config_json_state_t state = {0};
  const char *input = "{\"plain_output\":true,\"json_output\":true}";
  if (app_config_parse_json_state(&state, input) != APP_SUCCESS) {
    return false;
  }
  return state.values[APP_FLAG_JSON_OUTPUT] &&
         !state.values[APP_FLAG_PLAIN_OUTPUT];
}

static bool test_config_json_log_level_exclusivity(void) {
  app_config_json_state_t state = {0};
  const char *input = "{\"debug\":true,\"quiet\":true,\"verbose\":true}";
  if (app_config_parse_json_state(&state, input) != APP_SUCCESS) {
    return false;
  }
  return !state.values[APP_FLAG_DEBUG] && !state.values[APP_FLAG_QUIET] &&
         state.values[APP_FLAG_VERBOSE];
}

static bool test_config_setter_log_level_exclusivity(void) {
  app_config_t *config = NULL;
  if (app_config_create(&config) != APP_SUCCESS) {
    return false;
  }

  bool ok = app_config_set_debug(config, true) == APP_SUCCESS &&
            app_config_is_debug(config) && !app_config_is_quiet(config) &&
            !app_config_is_verbose(config);
  ok = ok && app_config_set_quiet(config, true) == APP_SUCCESS &&
       !app_config_is_debug(config) && app_config_is_quiet(config) &&
       !app_config_is_verbose(config);
  ok = ok && app_config_set_verbose(config, true) == APP_SUCCESS &&
       !app_config_is_debug(config) && !app_config_is_quiet(config) &&
       app_config_is_verbose(config);

  app_config_destroy(config);
  return ok;
}

static bool test_request_json_parses_command_args_and_flags(void) {
  app_request_t request;
  app_request_init(&request);
  const char *input =
      "{\"command\":\"hello\",\"args\":[\"Alice\"],"
      "\"flags\":{\"debug\":true}}";
  bool ok = app_request_parse_json(&request, input) == APP_SUCCESS &&
            request.command && strcmp(request.command, "hello") == 0 &&
            request.arg_count == 1 && strcmp(request.args[0], "Alice") == 0 &&
            request.flag_seen[APP_FLAG_DEBUG] &&
            request.flag_values[APP_FLAG_DEBUG];
  app_request_destroy(&request);
  return ok;
}

static bool test_request_json_applies_to_config(void) {
  app_request_t request;
  app_request_init(&request);
  app_config_t *config = NULL;
  bool ok = app_request_parse_json(&request,
                                   "{\"command\":\"echo\",\"args\":[\"hi\"],"
                                   "\"flags\":{\"plain_output\":true}}") ==
                APP_SUCCESS &&
            app_config_create(&config) == APP_SUCCESS;
  if (ok) {
    ok = app_request_apply_to_config(&request, config) == APP_SUCCESS &&
         strcmp(app_config_get_command(config), "echo") == 0 &&
         app_config_is_plain_output(config);
  }

  int count = 0;
  char *const *args =
      config ? app_config_get_command_args(config, &count) : NULL;
  ok = ok && count == 1 && args && strcmp(args[0], "hi") == 0;

  app_config_destroy(config);
  app_request_destroy(&request);
  return ok;
}

static bool test_request_json_rejects_unknown_flag(void) {
  app_request_t request;
  app_request_init(&request);
  const app_error err = app_request_parse_json(
      &request, "{\"command\":\"hello\",\"flags\":{\"bogus\":true}}");
  app_request_destroy(&request);
  return err == APP_ERROR_UNKNOWN_OPTION;
}

static bool test_secret_zero_clears_buffer(void) {
  unsigned char buf[16];
  for (size_t i = 0; i < sizeof(buf); i++) {
    buf[i] = (unsigned char)(i + 1);
  }
  app_secret_zero(buf, sizeof(buf));
  for (size_t i = 0; i < sizeof(buf); i++) {
    if (buf[i] != 0) {
      return false;
    }
  }
  app_secret_zero(NULL, 0);
  app_secret_zero(buf, 0);
  return true;
}

void run_config_unit_tests(unit_stats_t *stats) {
  unit_record(stats, test_strerror_covers_every_code(),
              "app_strerror covers every code");
  unit_record(stats, test_config_json_parses_valid_input(),
              "config_json parses valid input");
  unit_record(stats, test_config_json_rejects_unicode_escape(),
              "config_json rejects \\uXXXX escapes");
  unit_record(stats, test_config_json_rejects_trailing_garbage(),
              "config_json rejects trailing garbage");
  unit_record(stats, test_config_json_rejects_long_keys(),
              "config_json rejects truncated keys");
  unit_record(stats, test_config_json_output_exclusivity(),
              "config_json enforces output flag exclusivity");
  unit_record(stats, test_config_json_log_level_exclusivity(),
              "config_json enforces log-level exclusivity");
  unit_record(stats, test_config_setter_log_level_exclusivity(),
              "config setters enforce log-level exclusivity");
  unit_record(stats, test_request_json_parses_command_args_and_flags(),
              "request_json parses command, args, and flags");
  unit_record(stats, test_request_json_applies_to_config(),
              "request_json applies parsed values to config");
  unit_record(stats, test_request_json_rejects_unknown_flag(),
              "request_json rejects unknown flags");
#ifndef _WIN32
  unit_record(stats, test_config_env_no_color_empty_sets_flag(),
              "config env treats empty NO_COLOR as present");
  unit_record(stats, test_use_colors_honors_no_color_without_env_load(),
              "colors honor NO_COLOR even when config skipped env load");
#endif
  unit_record(stats, test_secret_zero_clears_buffer(),
              "app_secret_zero clears buffer");
}
