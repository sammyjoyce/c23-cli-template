/*
 * Unit tests for core config, error, color, and memory helpers.
 */

#include <stdlib.h>
#include <string.h>

#include "../src/core/config_json.h"
#include "../src/core/error.h"
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
  const int codes[] = {
      APP_SUCCESS,
      APP_ERROR_INVALID_ARG,
      APP_ERROR_INVALID_COMMAND,
      APP_ERROR_CONFIG,
      APP_ERROR_CONFIG_PARSE,
      APP_ERROR_CONFIG_INVALID,
      APP_ERROR_MISSING_ARG,
      APP_ERROR_UNKNOWN_OPTION,
      APP_ERROR_MEMORY,
      APP_ERROR_IO,
      APP_ERROR_PERMISSION,
      APP_ERROR_INTERNAL,
      APP_ERROR_THREADING,
      APP_ERROR_RESOURCE,
      APP_ERROR_SIGNAL,
      APP_ERROR_NOT_FOUND,
      APP_ERROR_INVALID_DATA,
      APP_ERROR_PARSE_ERROR,
      APP_ERROR_VALIDATION,
      APP_ERROR_OVERFLOW,
      APP_ERROR_UNDERFLOW,
      APP_ERROR_OUT_OF_RANGE,
  };

  for (size_t i = 0; i < sizeof(codes) / sizeof(codes[0]); i++) {
    const char *msg = app_strerror((app_error)codes[i]);
    if (!msg || msg[0] == '\0') {
      fprintf(stderr, "app_strerror returned empty for code %d\n", codes[i]);
      return false;
    }
    if (strcmp(msg, "Unknown error") == 0 &&
        codes[i] != APP_ERROR_FEATURE_BASE) {
      fprintf(stderr, "code %d hit the default branch\n", codes[i]);
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

  app_config_set_debug(config, true);
  bool ok = app_config_is_debug(config) && !app_config_is_quiet(config) &&
            !app_config_is_verbose(config);
  app_config_set_quiet(config, true);
  ok = ok && !app_config_is_debug(config) && app_config_is_quiet(config) &&
       !app_config_is_verbose(config);
  app_config_set_verbose(config, true);
  ok = ok && !app_config_is_debug(config) && !app_config_is_quiet(config) &&
       app_config_is_verbose(config);

  app_config_destroy(config);
  return ok;
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
  unit_record(stats, test_config_json_output_exclusivity(),
              "config_json enforces output flag exclusivity");
  unit_record(stats, test_config_json_log_level_exclusivity(),
              "config_json enforces log-level exclusivity");
  unit_record(stats, test_config_setter_log_level_exclusivity(),
              "config setters enforce log-level exclusivity");
#ifndef _WIN32
  unit_record(stats, test_config_env_no_color_empty_sets_flag(),
              "config env treats empty NO_COLOR as present");
  unit_record(stats, test_use_colors_honors_no_color_without_env_load(),
              "colors honor NO_COLOR even when config skipped env load");
#endif
  unit_record(stats, test_secret_zero_clears_buffer(),
              "app_secret_zero clears buffer");
}
