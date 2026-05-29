/*
 * Small curses-free terminal facts shared by CLI and optional TUI code.
 */
#pragma once

#include <stdbool.h>

typedef enum {
  APP_TERMINAL_STDIN = 0,
  APP_TERMINAL_STDOUT,
  APP_TERMINAL_STDERR,
} app_terminal_stream_t;

typedef struct {
  int cols;
  int rows;
  bool known;
} app_terminal_size_t;

bool app_terminal_stream_is_tty(app_terminal_stream_t stream);
bool app_terminal_is_interactive(void);
app_terminal_size_t app_terminal_query_size(void);
