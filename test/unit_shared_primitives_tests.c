/*
 * Unit tests for curses-free primitives shared by CLI and TUI code.
 */
#include <string.h>

#include "../src/cli/option_meta.h"
#include "../src/core/app_info.h"
#include "../src/core/config.h"
#include "../src/core/diagnostics.h"
#include "../src/io/terminal.h"
#include "../src/tui/tui_menu_adapter.h"
#include "../src/ui/text_layout.h"
#include "unit_support.h"

static bool test_app_info_feature_table(void) {
  const app_build_info_t *build = app_build_info();
  size_t count = 0;
  const app_feature_info_t *features = app_feature_table(&count);
  const app_feature_info_t *tui = app_feature_find(APP_FEATURE_TUI);
  return build && build->name && build->version && features && count >= 3 &&
         tui && strcmp(tui->key, "tui") == 0 && tui->build_option != NULL;
}

static bool test_option_meta_matches_and_formats(void) {
  char label[64];
  const app_command_arg_t arg = {.name = "path",
                                 .required = true,
                                 .arity_minimum = 1,
                                 .arity_maximum = 1,
                                 .description = "Path"};
  size_t len = app_option_format_label(label, sizeof(label), "--config", "-c",
                                       &arg, 1, APP_OPTION_LABEL_CLI);
  if (len == 0 || strcmp(label, "-c, --config path") != 0) {
    return false;
  }

  /* Long-only: no short flag, no dangling separator. */
  char long_only[64];
  app_option_format_label(long_only, sizeof(long_only), "--verbose", NULL, NULL,
                          0, APP_OPTION_LABEL_CLI);
  if (strcmp(long_only, "--verbose") != 0) {
    return false;
  }

  /* Short-only: must NOT emit a trailing ", --". */
  char short_only[64];
  app_option_format_label(short_only, sizeof(short_only), NULL, "-x", NULL, 0,
                          APP_OPTION_LABEL_CLI);
  if (strcmp(short_only, "-x") != 0) {
    return false;
  }

  /* Short-only with an empty long string behaves the same. */
  char short_empty_long[64];
  app_option_format_label(short_empty_long, sizeof(short_empty_long), "", "-x",
                          NULL, 0, APP_OPTION_LABEL_CLI);
  if (strcmp(short_empty_long, "-x") != 0) {
    return false;
  }

  return app_option_token_matches("--config", "--config", "-c") &&
         app_option_token_matches("-c", "config", "c") &&
         !app_option_token_matches("--conf", "config", "c");
}

static bool test_text_layout_width_and_truncate(void) {
  int cols = 0;
  size_t bytes = app_text_truncate_utf8_columns("hello", 3, &cols);
  return app_text_width_utf8("hello") == 5 && bytes == 3 && cols == 3 &&
         app_text_width_utf8("é") >= 1;
}

typedef struct {
  size_t count;
  char text[8][64];
  int columns[8];
} wrap_capture_t;

static bool wrap_capture_emit(void *user, const char *bytes, size_t byte_count,
                              int columns) {
  wrap_capture_t *cap = user;
  if (cap->count >= 8 || byte_count >= sizeof(cap->text[0])) {
    return false;
  }
  memcpy(cap->text[cap->count], bytes, byte_count);
  cap->text[cap->count][byte_count] = '\0';
  cap->columns[cap->count] = columns;
  cap->count++;
  return true;
}

static bool test_text_layout_wrap_multi_space(void) {
  /* Two words separated by a run of three spaces. With width 8, both fit on
   * one line ("aa" + 3 spaces + "bbb" = 8 columns). The reported `columns`
   * must equal the rendered width of the emitted byte span, not assume a
   * single separator space. */
  wrap_capture_t cap = {0};
  app_text_wrap_utf8("aa   bbb", 8, 0, 0, wrap_capture_emit, &cap);
  if (cap.count != 1 || strcmp(cap.text[0], "aa   bbb") != 0 ||
      cap.columns[0] != 8 ||
      cap.columns[0] != app_text_width_utf8(cap.text[0])) {
    return false;
  }

  /* With width 7 the second word no longer fits (2 + 3 + 3 = 8 > 7), so it
   * wraps. The first line keeps only "aa" with its true column width. */
  wrap_capture_t narrow = {0};
  app_text_wrap_utf8("aa   bbb", 7, 0, 0, wrap_capture_emit, &narrow);
  if (narrow.count != 2 || strcmp(narrow.text[0], "aa") != 0 ||
      narrow.columns[0] != 2 ||
      narrow.columns[0] != app_text_width_utf8(narrow.text[0]) ||
      strcmp(narrow.text[1], "bbb") != 0 || narrow.columns[1] != 3) {
    return false;
  }

  return true;
}

static bool test_text_layout_wrap_preserves_leading_indent(void) {
  /* Leading indentation at the start of a line (here two spaces before a
   * bullet word) must be preserved in the emitted bytes and counted in
   * `columns`, mirroring how app_show_overview() renders "  C23 modules". */
  wrap_capture_t cap = {0};
  app_text_wrap_utf8("  bullet text", 40, 0, 0, wrap_capture_emit, &cap);
  if (cap.count != 1 || strcmp(cap.text[0], "  bullet text") != 0 ||
      cap.columns[0] != 13 ||
      cap.columns[0] != app_text_width_utf8(cap.text[0])) {
    return false;
  }

  /* Indentation survives across an explicit newline: each line keeps its own
   * leading spaces, and inter-word spacing stays intact. */
  wrap_capture_t multi = {0};
  app_text_wrap_utf8("Intro\n  C23 modules\n  Zig build", 40, 0, 0,
                     wrap_capture_emit, &multi);
  if (multi.count != 3 || strcmp(multi.text[0], "Intro") != 0 ||
      strcmp(multi.text[1], "  C23 modules") != 0 || multi.columns[1] != 13 ||
      multi.columns[1] != app_text_width_utf8(multi.text[1]) ||
      strcmp(multi.text[2], "  Zig build") != 0 || multi.columns[2] != 11 ||
      multi.columns[2] != app_text_width_utf8(multi.text[2])) {
    return false;
  }

  return true;
}

static bool test_terminal_query_is_safe(void) {
  app_terminal_size_t size = app_terminal_query_size();
  return !size.known || (size.cols > 0 && size.rows > 0);
}

static bool test_tui_menu_adapter_maps_action_data(void) {
  const app_action_item_t action = {.id = 42,
                                    .kind = APP_ACTION_COMMAND,
                                    .label = "hello",
                                    .description = "Print a greeting",
                                    .disabled = true,
                                    .command_name = "hello"};
  tui_menu_item_t item = {0};
  if (!tui_menu_item_from_action(&action, &item)) {
    return false;
  }
  const app_action_item_t separator = {.kind = APP_ACTION_SEPARATOR};
  tui_menu_item_t sep = {0};
  return item.id == 42 && item.disabled && item.kind == TUI_MENU_ITEM_NORMAL &&
         strcmp(item.label, "hello") == 0 &&
         tui_menu_item_from_action(&separator, &sep) &&
         sep.kind == TUI_MENU_ITEM_SEPARATOR;
}

static bool test_diagnostics_collects_core_checks(void) {
  app_config_t *config = NULL;
  if (app_config_create(&config) != APP_SUCCESS) {
    return false;
  }
  app_diagnostic_check_t checks[8];
  size_t count = 0;
  app_error err = app_diagnostics_collect(
      config, checks, sizeof(checks) / sizeof(checks[0]), &count);
  bool ok = err == APP_SUCCESS && count == 8 &&
            strcmp(checks[0].name, "binary") == 0 &&
            strcmp(checks[2].name, "tui_compiled") == 0 &&
            checks[2].has_enabled;
  app_config_destroy(config);
  return ok;
}

void run_shared_primitives_unit_tests(unit_stats_t *stats) {
  unit_record(stats, test_app_info_feature_table(),
              "app_info exposes build and feature metadata");
  unit_record(stats, test_option_meta_matches_and_formats(),
              "option_meta matches and formats CLI labels");
  unit_record(stats, test_text_layout_width_and_truncate(),
              "text_layout measures and truncates utf8 text");
  unit_record(stats, test_text_layout_wrap_multi_space(),
              "text_layout wraps with multi-space column accounting");
  unit_record(stats, test_text_layout_wrap_preserves_leading_indent(),
              "text_layout preserves leading indentation when wrapping");
  unit_record(stats, test_terminal_query_is_safe(),
              "terminal query is safe off tty");
  unit_record(stats, test_tui_menu_adapter_maps_action_data(),
              "tui_menu_adapter maps action descriptors");
  unit_record(stats, test_diagnostics_collects_core_checks(),
              "diagnostics collector returns core checks");
}
