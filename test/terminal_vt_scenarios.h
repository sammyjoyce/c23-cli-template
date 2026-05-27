#ifndef TERMINAL_VT_SCENARIOS_H
#define TERMINAL_VT_SCENARIOS_H

#include "terminal_vt_support.h"

int run_tui_menu_test(test_stats_t *stats, const char *binary,
                      bool tui_enabled);
int run_tui_fuzz_smoke(test_stats_t *stats, const char *binary,
                       bool tui_enabled);

#endif
