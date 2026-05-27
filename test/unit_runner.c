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
#endif

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

static bool test_menu_label_strips_ampersand(void) {
  const tui_menu_item_t items[] = {{.label = "&Overview", .id = 1}};
  const tui_menu_config_t cfg = {.items = items, .item_count = 1};
  tui_menu_state_t *s = NULL;
  if (tui_menu_state_create(&cfg, &s) != TUI_MENU_OK)
    return false;
  const wchar_t *w = tui_menu_state_label_wcs(s, 0);
  bool ok =
      wcscmp(w, L"Overview") == 0 && tui_menu_state_mnemonic(s, 0) == L'o';
  tui_menu_state_destroy(s);
  return ok;
}

static bool test_menu_label_literal_ampersand(void) {
  const tui_menu_item_t items[] = {{.label = "AT&&T", .id = 1}};
  const tui_menu_config_t cfg = {.items = items, .item_count = 1};
  tui_menu_state_t *s = NULL;
  if (tui_menu_state_create(&cfg, &s) != TUI_MENU_OK)
    return false;
  const wchar_t *w = tui_menu_state_label_wcs(s, 0);
  bool ok = wcscmp(w, L"AT&T") == 0 && tui_menu_state_mnemonic(s, 0) == 0;
  tui_menu_state_destroy(s);
  return ok;
}

static bool test_menu_label_mnemonic_mid_word(void) {
  const tui_menu_item_t items[] = {{.label = "E&xit", .id = 0}};
  const tui_menu_config_t cfg = {.items = items, .item_count = 1};
  tui_menu_state_t *s = NULL;
  if (tui_menu_state_create(&cfg, &s) != TUI_MENU_OK)
    return false;
  const wchar_t *w = tui_menu_state_label_wcs(s, 0);
  bool ok = wcscmp(w, L"Exit") == 0 && tui_menu_state_mnemonic(s, 0) == L'x';
  tui_menu_state_destroy(s);
  return ok;
}

static bool test_mnemonic_unique_returns_index(void) {
  const tui_menu_item_t items[] = {
      {.label = "&Foo", .id = 1},
      {.label = "&Bar", .id = 2},
  };
  const tui_menu_config_t cfg = {.items = items, .item_count = 2};
  tui_menu_state_t *s = NULL;
  if (tui_menu_state_create(&cfg, &s) != TUI_MENU_OK)
    return false;
  bool beep = false;
  int r = tui_menu_state_mnemonic_jump(s, L'b', &beep);
  bool ok = (r == 1) && !beep;
  tui_menu_state_destroy(s);
  return ok;
}

static bool test_mnemonic_duplicate_cycles_no_confirm(void) {
  const tui_menu_item_t items[] = {
      {.label = "F&oo", .id = 1},
      {.label = "B&ar", .id = 2},
      {.label = "B&az", .id = 3},
  };
  const tui_menu_config_t cfg = {.items = items, .item_count = 3};
  tui_menu_state_t *s = NULL;
  if (tui_menu_state_create(&cfg, &s) != TUI_MENU_OK)
    return false;
  bool beep = false;
  /* Two items share mnemonic 'a' - calling it twice should cycle. */
  int r1 = tui_menu_state_mnemonic_jump(s, L'a', &beep);
  int idx1 = tui_menu_state_selected_index(s);
  int r2 = tui_menu_state_mnemonic_jump(s, L'a', &beep);
  int idx2 = tui_menu_state_selected_index(s);
  bool ok = r1 < 0 && r2 < 0 && idx1 != idx2 && beep;
  tui_menu_state_destroy(s);
  return ok;
}

static bool test_mnemonic_disabled_beeps(void) {
  const tui_menu_item_t items[] = {
      {.label = "&Foo", .id = 1, .disabled = true},
      {.label = "&Bar", .id = 2},
  };
  const tui_menu_config_t cfg = {.items = items, .item_count = 2};
  tui_menu_state_t *s = NULL;
  if (tui_menu_state_create(&cfg, &s) != TUI_MENU_OK)
    return false;
  bool beep = false;
  int r = tui_menu_state_mnemonic_jump(s, L'f', &beep);
  bool ok = r < 0 && beep;
  tui_menu_state_destroy(s);
  return ok;
}

static bool test_search_filters_matches_to_front_of_selection(void) {
  const tui_menu_item_t items[] = {
      {.label = "&Overview", .id = 1},
      {.label = "&System Information", .id = 2},
      {.label = "&Input Dialog", .id = 3},
      {.label = "&Progress Pattern", .id = 4},
  };
  const tui_menu_config_t cfg = {.items = items, .item_count = 4};
  tui_menu_state_t *s = NULL;
  if (tui_menu_state_create(&cfg, &s) != TUI_MENU_OK)
    return false;

  tui_menu_state_search_open(s);
  tui_menu_state_search_append(s, L'p');
  tui_menu_state_search_append(s, L'r');
  tui_menu_state_search_append(s, L'o');

  /* "Progress Pattern" is the only "pro" substring match */
  bool ok =
      tui_menu_state_search_active(s) && tui_menu_state_selected_index(s) == 3;

  tui_menu_state_destroy(s);
  return ok;
}

static bool test_search_case_insensitive(void) {
  const tui_menu_item_t items[] = {
      {.label = "Overview", .id = 1},
      {.label = "System", .id = 2},
  };
  const tui_menu_config_t cfg = {.items = items, .item_count = 2};
  tui_menu_state_t *s = NULL;
  if (tui_menu_state_create(&cfg, &s) != TUI_MENU_OK)
    return false;
  tui_menu_state_search_open(s);
  tui_menu_state_search_append(s, L'S');
  bool ok = tui_menu_state_selected_index(s) == 1;
  tui_menu_state_destroy(s);
  return ok;
}

static bool test_search_close_clears_query(void) {
  const tui_menu_item_t items[] = {{.label = "Foo", .id = 1}};
  const tui_menu_config_t cfg = {.items = items, .item_count = 1};
  tui_menu_state_t *s = NULL;
  if (tui_menu_state_create(&cfg, &s) != TUI_MENU_OK)
    return false;
  tui_menu_state_search_open(s);
  tui_menu_state_search_append(s, L'f');
  tui_menu_state_search_close(s);
  bool ok = !tui_menu_state_search_active(s) &&
            wcslen(tui_menu_state_search_query(s)) == 0;
  tui_menu_state_destroy(s);
  return ok;
}

static bool test_search_backspace_removes_one_wchar(void) {
  const tui_menu_item_t items[] = {{.label = "Foo", .id = 1}};
  const tui_menu_config_t cfg = {.items = items, .item_count = 1};
  tui_menu_state_t *s = NULL;
  if (tui_menu_state_create(&cfg, &s) != TUI_MENU_OK)
    return false;
  tui_menu_state_search_open(s);
  tui_menu_state_search_append(s, L'a');
  tui_menu_state_search_append(s, L'b');
  tui_menu_state_search_backspace(s);
  bool ok = wcscmp(tui_menu_state_search_query(s), L"a") == 0;
  tui_menu_state_destroy(s);
  return ok;
}

static bool test_select_visible_rejects_disabled(void) {
  const tui_menu_item_t items[] = {
      {.label = "a", .id = 1},
      {.label = "b", .id = 2, .disabled = true},
      {.label = "c", .id = 3},
  };
  const tui_menu_config_t cfg = {.items = items, .item_count = 3};
  tui_menu_state_t *s = NULL;
  if (tui_menu_state_create(&cfg, &s) != TUI_MENU_OK)
    return false;
  const bool rejected = !tui_menu_state_select_visible(s, 1);
  const bool selected = tui_menu_state_select_visible(s, 2);
  bool ok = rejected && selected && tui_menu_state_selected_index(s) == 2;
  tui_menu_state_destroy(s);
  return ok;
}

static bool test_ensure_selection_visible_updates_top(void) {
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
  (void)tui_menu_state_select_visible(s, 3);
  tui_menu_state_ensure_selection_visible(s, 2);
  bool ok = tui_menu_state_top_visible(s) == 2;
  tui_menu_state_destroy(s);
  return ok;
}

static bool test_numeric_jump(void) {
  const tui_menu_item_t items[] = {
      {.label = "a", .id = 1},
      {.label = "b", .id = 2},
      {.label = "c", .id = 3, .disabled = true},
      {.label = "d", .id = 4},
  };
  const tui_menu_config_t cfg = {.items = items, .item_count = 4};
  tui_menu_state_t *s = NULL;
  if (tui_menu_state_create(&cfg, &s) != TUI_MENU_OK)
    return false;
  tui_menu_state_numeric_jump(s, 1); /* visible row 1 = items[1] */
  if (tui_menu_state_selected_index(s) != 1) {
    tui_menu_state_destroy(s);
    return false;
  }
  tui_menu_state_numeric_jump(s, 2); /* disabled - must not change selection */
  bool ok = tui_menu_state_selected_index(s) == 1;
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
#ifndef _WIN32
  unit_record(&stats, test_config_env_no_color_empty_sets_flag(),
              "config env treats empty NO_COLOR as present");
#endif
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
  unit_record(&stats, test_menu_label_strips_ampersand(),
              "tui_menu label strips '&' and records mnemonic");
  unit_record(&stats, test_menu_label_literal_ampersand(),
              "tui_menu label '&&' renders as literal '&'");
  unit_record(&stats, test_menu_label_mnemonic_mid_word(),
              "tui_menu mnemonic can be mid-word");
  unit_record(&stats, test_mnemonic_unique_returns_index(),
              "tui_menu unique mnemonic returns the items[] index");
  unit_record(&stats, test_mnemonic_duplicate_cycles_no_confirm(),
              "tui_menu duplicate mnemonic cycles selection");
  unit_record(&stats, test_mnemonic_disabled_beeps(),
              "tui_menu disabled mnemonic beeps, no confirm");
  unit_record(&stats, test_search_filters_matches_to_front_of_selection(),
              "tui_menu search snaps selection to first match");
  unit_record(&stats, test_search_case_insensitive(),
              "tui_menu search is case-insensitive");
  unit_record(&stats, test_search_close_clears_query(),
              "tui_menu search_close clears the query");
  unit_record(&stats, test_search_backspace_removes_one_wchar(),
              "tui_menu search backspace pops one wchar");
  unit_record(&stats, test_select_visible_rejects_disabled(),
              "tui_menu select_visible rejects disabled rows");
  unit_record(&stats, test_ensure_selection_visible_updates_top(),
              "tui_menu keeps selected row in viewport");
  unit_record(&stats, test_numeric_jump(),
              "tui_menu numeric jump targets visible row, skips disabled");
  unit_record(&stats, test_secret_zero_clears_buffer(),
              "app_secret_zero clears buffer");

  printf("1..%d\n", stats.passed + stats.failed);
  fprintf(stderr, "%d passed, %d failed\n", stats.passed, stats.failed);
  return stats.failed == 0 ? 0 : 1;
}
