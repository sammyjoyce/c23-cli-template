#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "terminal_vt_scenarios.h"
#include "test_harness.h"

static bool parse_bool_arg(const char *value) {
  return strcmp(value, "1") == 0 || strcmp(value, "true") == 0 ||
         strcmp(value, "yes") == 0;
}

int main(int argc, char **argv) {
  const char *binary = NULL;
  bool tui_enabled = false;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--binary") == 0 && i + 1 < argc) {
      binary = argv[++i];
    } else if (strcmp(argv[i], "--tui-enabled") == 0 && i + 1 < argc) {
      tui_enabled = parse_bool_arg(argv[++i]);
    } else {
      fprintf(stderr,
              "usage: %s --binary PATH [--tui-enabled 0|1]\nunknown arg: %s\n",
              argv[0], argv[i]);
      return 2;
    }
  }

  if (!binary) {
    fprintf(stderr, "usage: %s --binary PATH [--tui-enabled 0|1]\n", argv[0]);
    return 2;
  }

  struct stat st;
  if (stat(binary, &st) != 0) {
    fprintf(stderr, "terminal-vt tests: binary does not exist: %s\n", binary);
    return 2;
  }

  test_stats_t stats = {0};
  printf("TAP version 13\n");
  run_tui_menu_test(&stats, binary, tui_enabled);
  run_tui_fuzz_smoke(&stats, binary, tui_enabled);
  run_tui_menu_search(&stats, binary, tui_enabled);
  run_tui_menu_mnemonic(&stats, binary, tui_enabled);
  run_tui_menu_separator(&stats, binary, tui_enabled);
  run_tui_menu_resize(&stats, binary, tui_enabled);
  run_tui_menu_sigint(&stats, binary, tui_enabled);
  printf("1..%d\n", stats.passed + stats.failed + stats.skipped);
  fprintf(stderr, "%d passed, %d failed, %d skipped\n", stats.passed,
          stats.failed, stats.skipped);
  return stats.failed == 0 ? 0 : 1;
}
