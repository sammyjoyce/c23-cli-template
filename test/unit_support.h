/*
 * Shared helpers for in-process unit tests.
 *
 * Output is TAP so this runner slots into the same harness as the contract
 * and terminal tests.
 */
#pragma once

#include <stdbool.h>
#include <stdio.h>

typedef struct {
  int passed;
  int failed;
} unit_stats_t;

static inline void unit_record(unit_stats_t *stats, bool ok, const char *name) {
  if (ok) {
    stats->passed++;
    printf("ok %d - %s\n", stats->passed + stats->failed, name);
  } else {
    stats->failed++;
    printf("not ok %d - %s\n", stats->passed + stats->failed, name);
  }
}

void run_config_unit_tests(unit_stats_t *stats);
void run_input_unit_tests(unit_stats_t *stats);
void run_tui_menu_unit_tests(unit_stats_t *stats);
