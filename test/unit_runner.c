/*
 * Unit tests for pieces of the codebase that don't need a subprocess:
 *   - app_strerror coverage (one entry per error code)
 *   - JSON config parser error paths
 *   - app_secret_zero behaviour
 *
 * Output is TAP so it slots into the same harness as the contract tests.
 */

#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "../src/core/config_json.h"
#include "../src/core/error.h"
#include "../src/tui/tui_menu.h"
#include "../src/tui/tui_menu_internal.h"
#include "../src/utils/memory.h"

typedef struct {
  int passed;
  int failed;
} unit_stats_t;

static void unit_record(unit_stats_t *stats, bool ok, const char *name) {
  if (ok) {
    stats->passed++;
    printf("ok %d - %s\n", stats->passed + stats->failed, name);
  } else {
    stats->failed++;
    printf("not ok %d - %s\n", stats->passed + stats->failed, name);
  }
}

static bool test_strerror_covers_every_code(void) {
  // Iterate every defined enum value. APP_ERROR_FEATURE_BASE marks the start
  // of the reserved range for user-defined codes, so we stop just before it.
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
  // \uXXXX escapes used to be silently replaced with '?'. They should now
  // cleanly fail parsing.
  app_config_json_state_t state = {0};
  const char *input = "{\"debug\":\"\\u00e9\"}";
  return app_config_parse_json_state(&state, input) != APP_SUCCESS;
}

static bool test_config_json_rejects_trailing_garbage(void) {
  app_config_json_state_t state = {0};
  const char *input = "{}garbage";
  return app_config_parse_json_state(&state, input) != APP_SUCCESS;
}

static bool test_config_json_exclusivity(void) {
  // Setting json_output should turn off plain_output, even if plain_output
  // was set earlier in the same object.
  app_config_json_state_t state = {0};
  const char *input = "{\"plain_output\":true,\"json_output\":true}";
  if (app_config_parse_json_state(&state, input) != APP_SUCCESS) {
    return false;
  }
  return state.values[APP_FLAG_JSON_OUTPUT] &&
         !state.values[APP_FLAG_PLAIN_OUTPUT];
}

static bool test_menu_state_rejects_zero_items(void) {
  const tui_menu_config_t cfg = {.items = NULL, .item_count = 0};
  tui_menu_state_t *s = NULL;
  return tui_menu_state_create(&cfg, &s) == TUI_MENU_INVALID_ARG && s == NULL;
}

static bool test_menu_state_picks_first_enabled_when_default_negative(void) {
  const tui_menu_item_t items[] = {
      {.label = "first", .id = 1, .disabled = true},
      {.label = "second", .id = 2},
      {.label = "third", .id = 3},
  };
  const tui_menu_config_t cfg = {
      .items = items, .item_count = 3, .default_index = -1};
  tui_menu_state_t *s = NULL;
  if (tui_menu_state_create(&cfg, &s) != TUI_MENU_OK)
    return false;
  bool ok = tui_menu_state_selected_index(s) == 1;
  tui_menu_state_destroy(s);
  return ok;
}

static bool test_menu_state_honors_default_index_when_enabled(void) {
  const tui_menu_item_t items[] = {
      {.label = "a", .id = 1},
      {.label = "b", .id = 2},
      {.label = "c", .id = 3},
  };
  const tui_menu_config_t cfg = {
      .items = items, .item_count = 3, .default_index = 2};
  tui_menu_state_t *s = NULL;
  if (tui_menu_state_create(&cfg, &s) != TUI_MENU_OK)
    return false;
  bool ok = tui_menu_state_selected_index(s) == 2;
  tui_menu_state_destroy(s);
  return ok;
}

static bool test_menu_step_skips_separator(void) {
  const tui_menu_item_t items[] = {
      {.label = "a", .id = 1},
      {.kind = TUI_MENU_ITEM_SEPARATOR},
      {.label = "b", .id = 2},
  };
  const tui_menu_config_t cfg = {.items = items, .item_count = 3};
  tui_menu_state_t *s = NULL;
  if (tui_menu_state_create(&cfg, &s) != TUI_MENU_OK)
    return false;
  if (tui_menu_state_selected_index(s) != 0) {
    tui_menu_state_destroy(s);
    return false;
  }
  tui_menu_state_step(s, 1);
  bool ok = tui_menu_state_selected_index(s) == 2;
  tui_menu_state_destroy(s);
  return ok;
}

static bool test_menu_step_skips_disabled(void) {
  const tui_menu_item_t items[] = {
      {.label = "a", .id = 1},
      {.label = "b", .id = 2, .disabled = true},
      {.label = "c", .id = 3},
  };
  const tui_menu_config_t cfg = {.items = items, .item_count = 3};
  tui_menu_state_t *s = NULL;
  if (tui_menu_state_create(&cfg, &s) != TUI_MENU_OK)
    return false;
  tui_menu_state_step(s, 1);
  bool ok = tui_menu_state_selected_index(s) == 2;
  tui_menu_state_destroy(s);
  return ok;
}

static bool test_menu_step_wraps(void) {
  const tui_menu_item_t items[] = {
      {.label = "a", .id = 1},
      {.label = "b", .id = 2},
  };
  const tui_menu_config_t cfg = {.items = items, .item_count = 2};
  tui_menu_state_t *s = NULL;
  if (tui_menu_state_create(&cfg, &s) != TUI_MENU_OK)
    return false;
  tui_menu_state_step(s, -1); /* wrap from 0 to 1 */
  bool ok = tui_menu_state_selected_index(s) == 1;
  tui_menu_state_destroy(s);
  return ok;
}

static bool test_menu_home_end(void) {
  const tui_menu_item_t items[] = {
      {.label = "a", .id = 1},
      {.label = "b", .id = 2},
      {.label = "c", .id = 3},
      {.label = "d", .id = 4},
  };
  const tui_menu_config_t cfg = {.items = items, .item_count = 4};
  tui_menu_state_t *s = NULL;
  if (tui_menu_state_create(&cfg, &s) != TUI_MENU_OK)
    return false;
  tui_menu_state_end(s);
  if (tui_menu_state_selected_index(s) != 3) {
    tui_menu_state_destroy(s);
    return false;
  }
  tui_menu_state_home(s);
  bool ok = tui_menu_state_selected_index(s) == 0;
  tui_menu_state_destroy(s);
  return ok;
}

static bool test_menu_page_clamps(void) {
  const tui_menu_item_t items[] = {
      {.label = "a", .id = 1}, {.label = "b", .id = 2}, {.label = "c", .id = 3},
      {.label = "d", .id = 4}, {.label = "e", .id = 5},
  };
  const tui_menu_config_t cfg = {.items = items, .item_count = 5};
  tui_menu_state_t *s = NULL;
  if (tui_menu_state_create(&cfg, &s) != TUI_MENU_OK)
    return false;
  tui_menu_state_page(s, 1, 3); /* expect selection moves to index 3 */
  if (tui_menu_state_selected_index(s) != 3) {
    tui_menu_state_destroy(s);
    return false;
  }
  tui_menu_state_page(s, 1, 3); /* clamps to last selectable */
  bool ok = tui_menu_state_selected_index(s) == 4;
  tui_menu_state_destroy(s);
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
  // Null/zero-length calls must be no-ops, not segfaults.
  app_secret_zero(NULL, 0);
  app_secret_zero(buf, 0);
  return true;
}

int main(void) {
  unit_stats_t stats = {0};
  setlocale(LC_ALL, ""); /* required for wchar_t tests */
  printf("TAP version 13\n");

  {
    const tui_menu_item_t item = {
        .label = "&Foo",
        .description = "demo",
        .id = 42,
    };
    /* Default-init must yield enabled, NORMAL kind. */
    bool ok =
        !item.disabled && item.kind == TUI_MENU_ITEM_NORMAL && item.id == 42;
    unit_record(&stats, ok,
                "tui_menu_item_t zero-init defaults are enabled+normal");
  }
  {
    const tui_menu_result_t r = {.status = TUI_MENU_OK, .selected_id = 7};
    unit_record(&stats, r.status == TUI_MENU_OK && r.selected_id == 7,
                "tui_menu_result_t designated-init works");
  }
  {
    /* Forward-declared opaque pointer is usable as a value type. */
    tui_menu_state_t *s = NULL;
    unit_record(&stats, s == NULL,
                "tui_menu_state_t is forward-declared opaque");
  }

  unit_record(&stats, test_strerror_covers_every_code(),
              "app_strerror covers every code");
  unit_record(&stats, test_config_json_parses_valid_input(),
              "config_json parses valid input");
  unit_record(&stats, test_config_json_rejects_unicode_escape(),
              "config_json rejects \\uXXXX escapes");
  unit_record(&stats, test_config_json_rejects_trailing_garbage(),
              "config_json rejects trailing garbage");
  unit_record(&stats, test_config_json_exclusivity(),
              "config_json enforces flag exclusivity");
  unit_record(&stats, test_menu_state_rejects_zero_items(),
              "tui_menu_state_create rejects zero items");
  unit_record(&stats,
              test_menu_state_picks_first_enabled_when_default_negative(),
              "tui_menu_state_create skips disabled default");
  unit_record(&stats, test_menu_state_honors_default_index_when_enabled(),
              "tui_menu_state_create honors enabled default_index");
  unit_record(&stats, test_menu_step_skips_separator(),
              "tui_menu_state_step skips separators");
  unit_record(&stats, test_menu_step_skips_disabled(),
              "tui_menu_state_step skips disabled");
  unit_record(&stats, test_menu_step_wraps(),
              "tui_menu_state_step wraps at ends");
  unit_record(&stats, test_menu_home_end(),
              "tui_menu home/end land on first/last");
  unit_record(&stats, test_menu_page_clamps(), "tui_menu page clamps at ends");
  unit_record(&stats, test_secret_zero_clears_buffer(),
              "app_secret_zero clears buffer");

  printf("1..%d\n", stats.passed + stats.failed);
  fprintf(stderr, "%d passed, %d failed\n", stats.passed, stats.failed);
  return stats.failed == 0 ? 0 : 1;
}
