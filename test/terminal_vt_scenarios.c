#include "terminal_vt.h"

typedef enum {
  TUI_STEP_EXPECT,
  TUI_STEP_SEND,
  TUI_STEP_RESIZE,
  TUI_STEP_WAIT_EXIT,
} tui_step_kind_t;

typedef struct {
  tui_step_kind_t kind;
  const char *value;
  int menu_index;
  uint16_t cols;
  uint16_t rows;
  int timeout_ms;
  const char *failure;
} tui_step_t;

static int run_tui_step(test_stats_t *stats, const char *test_name,
                        vt_session_t *session, const tui_step_t *step,
                        char **snapshot) {
  switch (step->kind) {
  case TUI_STEP_EXPECT:
    if (vt_expect_text(session, step->value, step->timeout_ms, snapshot)) {
      return 0;
    }
    print_tail(stderr, "screen:\n", *snapshot ? *snapshot : "",
               *snapshot ? strlen(*snapshot) : 0, 4000);
    print_tail(stderr, "transcript:\n", buffer_cstr(&session->transcript),
               session->transcript.len, 4000);
    return test_fail(stats, test_name, "%s", step->failure);
  case TUI_STEP_SEND:
    return vt_send(session, step->value)
               ? 0
               : test_fail(stats, test_name, "%s", step->failure);
  case TUI_STEP_RESIZE:
    return vt_resize(session, step->cols, step->rows)
               ? 0
               : test_fail(stats, test_name, "%s", step->failure);
  case TUI_STEP_WAIT_EXIT: {
    const int exit_code = vt_wait_for_exit(session, step->timeout_ms);
    return exit_code == 0 ? 0
                          : test_fail(stats, test_name,
                                      "expected exit 0, got %d", exit_code);
  }
  }
  return test_fail(stats, test_name, "unknown TUI step");
}

int run_tui_menu_test(test_stats_t *stats, const char *binary,
                      bool tui_enabled) {
  const char *name = "all TUI screens render through Ghostty VT";
  if (!tui_enabled) {
    test_skip(stats, name, "rebuild with -Denable-tui=true");
    return 0;
  }

  const char *args[] = {"menu"};
  vt_session_t session;
  if (!vt_session_start(&session, binary, args, 1, 80, 24)) {
    return test_fail(stats, name, "failed to start PTY session");
  }

  const tui_step_t steps[] = {
      {TUI_STEP_EXPECT, "Starter Showcase", 0, 0, 0, PTY_TIMEOUT_MS,
       "Starter Showcase did not appear"},
      {TUI_STEP_EXPECT, "Overview", 0, 0, 0, 1000,
       "menu label did not appear: Overview"},
      {TUI_STEP_EXPECT, "System Information", 0, 0, 0, 1000,
       "menu label did not appear: System Information"},
      {TUI_STEP_EXPECT, "Input Dialog", 0, 0, 0, 1000,
       "menu label did not appear: Input Dialog"},
      {TUI_STEP_EXPECT, "Progress Pattern", 0, 0, 0, 1000,
       "menu label did not appear: Progress Pattern"},
      {TUI_STEP_EXPECT, "Layout Pattern", 0, 0, 0, 1000,
       "menu label did not appear: Layout Pattern"},
      {TUI_STEP_EXPECT, "Exit", 0, 0, 0, 1000,
       "menu label did not appear: Exit"},

      {TUI_STEP_SEND, "o", 0, 0, 0, 0, "failed to select Overview"},
      {TUI_STEP_EXPECT, "Starter Overview", 0, 0, 0, PTY_TIMEOUT_MS,
       "Starter Overview did not appear"},
      {TUI_STEP_EXPECT, "C23 modules", 0, 0, 0, 1000,
       "overview body did not appear"},
      {TUI_STEP_SEND, "x", 0, 0, 0, 0, "failed to dismiss overview"},
      {TUI_STEP_EXPECT, "Starter Showcase", 0, 0, 0, PTY_TIMEOUT_MS,
       "menu did not reappear after overview"},

      {TUI_STEP_SEND, "s", 0, 0, 0, 0, "failed to select System Information"},
      {TUI_STEP_EXPECT, "System Information", 0, 0, 0, PTY_TIMEOUT_MS,
       "System Information did not appear"},
      {TUI_STEP_EXPECT, "Application:", 0, 0, 0, 1000,
       "system information body was incomplete: Application"},
      {TUI_STEP_EXPECT, "Terminal Size:", 0, 0, 0, 1000,
       "system information body was incomplete: Terminal Size"},
      {TUI_STEP_EXPECT, "Colors Supported:", 0, 0, 0, 1000,
       "system information body was incomplete: Colors Supported"},
      {TUI_STEP_SEND, "x", 0, 0, 0, 0, "failed to dismiss system information"},
      {TUI_STEP_EXPECT, "Starter Showcase", 0, 0, 0, PTY_TIMEOUT_MS,
       "menu did not reappear after system info"},

      {TUI_STEP_SEND, "i", 0, 0, 0, 0, "failed to select Input Dialog"},
      {TUI_STEP_EXPECT, "Input Dialog", 0, 0, 0, PTY_TIMEOUT_MS,
       "Input Dialog did not appear"},
      {TUI_STEP_EXPECT, "Enter a display name:", 0, 0, 0, 1000,
       "input prompt did not appear"},
      {TUI_STEP_SEND, "Ada Lovelace\r", 0, 0, 0, 0,
       "failed to submit input dialog text"},
      {TUI_STEP_EXPECT, "Input Captured", 0, 0, 0, PTY_TIMEOUT_MS,
       "Input Captured did not appear"},
      {TUI_STEP_EXPECT, "Hello, Ada Lovelace.", 0, 0, 0, 1000,
       "captured input message did not appear"},
      {TUI_STEP_SEND, "x", 0, 0, 0, 0,
       "failed to dismiss input captured message"},
      {TUI_STEP_EXPECT, "Starter Showcase", 0, 0, 0, PTY_TIMEOUT_MS,
       "menu did not reappear after input flow"},

      {TUI_STEP_SEND, "p", 0, 0, 0, 0, "failed to select Progress Pattern"},
      {TUI_STEP_EXPECT, "Progress Complete", 0, 0, 0, PTY_TIMEOUT_MS,
       "Progress Complete did not appear"},
      {TUI_STEP_EXPECT, "window lifecycle", 0, 0, 0, 1000,
       "progress completion body did not appear"},
      {TUI_STEP_SEND, "x", 0, 0, 0, 0, "failed to dismiss progress completion"},
      {TUI_STEP_EXPECT, "Starter Showcase", 0, 0, 0, PTY_TIMEOUT_MS,
       "menu did not reappear after progress"},

      {TUI_STEP_SEND, "l", 0, 0, 0, 0, "failed to select Layout Pattern"},
      {TUI_STEP_EXPECT, "Layout Pattern", 0, 0, 0, PTY_TIMEOUT_MS,
       "Layout Pattern did not appear"},
      {TUI_STEP_EXPECT, "Composable terminal UI", 0, 0, 0, 1000,
       "layout screen body was incomplete: Composable terminal UI"},
      {TUI_STEP_EXPECT, "Enter/Esc closes this panel", 0, 0, 0, 1000,
       "layout screen body was incomplete: close hint"},
      {TUI_STEP_SEND, "\x1b", 0, 0, 0, 0, "failed to close layout screen"},
      {TUI_STEP_EXPECT, "Starter Showcase", 0, 0, 0, PTY_TIMEOUT_MS,
       "menu did not reappear after layout"},

      {TUI_STEP_RESIZE, NULL, 0, 100, 28, 0,
       "failed to resize PTY/Ghostty terminal"},
      {TUI_STEP_EXPECT, "Starter Showcase", 0, 0, 0, PTY_TIMEOUT_MS,
       "Starter Showcase disappeared after resize"},
      {TUI_STEP_SEND, "q", 0, 0, 0, 0, "failed to send q"},
      {TUI_STEP_EXPECT, "Return to the shell?", 0, 0, 0, PTY_TIMEOUT_MS,
       "exit confirmation did not appear"},
      {TUI_STEP_SEND, "n", 0, 0, 0, 0, "failed to cancel exit confirmation"},
      {TUI_STEP_EXPECT, "Starter Showcase", 0, 0, 0, PTY_TIMEOUT_MS,
       "menu did not reappear after exit cancel"},
      {TUI_STEP_SEND, "x", 0, 0, 0, 0, "failed to select Exit"},
      {TUI_STEP_EXPECT, "Return to the shell?", 0, 0, 0, PTY_TIMEOUT_MS,
       "exit menu confirmation did not appear"},
      {TUI_STEP_SEND, "y", 0, 0, 0, 0, "failed to confirm exit"},
      {TUI_STEP_WAIT_EXIT, NULL, 0, 0, 0, PTY_TIMEOUT_MS, NULL},
  };

  char *snapshot = NULL;
  int failed = 0;
  for (size_t i = 0; !failed && i < sizeof(steps) / sizeof(steps[0]); i++) {
    failed = run_tui_step(stats, name, &session, &steps[i], &snapshot);
  }

  if (!failed) {
    test_pass(stats, name);
  }
  free(snapshot);
  vt_session_close(&session);
  return failed;
}

int run_tui_fuzz_smoke(test_stats_t *stats, const char *binary,
                       bool tui_enabled) {
  const char *name = "tui deterministic input and resize smoke";
  if (!tui_enabled) {
    test_skip(stats, name, "rebuild with -Denable-tui=true");
    return 0;
  }

  const char *args[] = {"menu"};
  vt_session_t session;
  if (!vt_session_start(&session, binary, args, 1, 80, 24)) {
    return test_fail(stats, name, "failed to start PTY session");
  }

  char *snapshot = NULL;
  int failed = 0;
  if (!vt_expect_text(&session, "Starter Showcase", PTY_TIMEOUT_MS,
                      &snapshot)) {
    failed = test_fail(stats, name, "initial menu did not render");
  }

  const char *safe_inputs[] = {
      "\x1b[B", "\x1b[A", "\t", "\x1b[B", "\x1b[A", "\t",
  };
  const uint16_t sizes[][2] = {{72, 20}, {100, 28}, {80, 24}};
  for (size_t i = 0;
       !failed && i < sizeof(safe_inputs) / sizeof(safe_inputs[0]); i++) {
    if (!vt_send(&session, safe_inputs[i])) {
      failed = test_fail(stats, name, "failed to send generated input %zu", i);
      break;
    }
    if (i < sizeof(sizes) / sizeof(sizes[0])) {
      if (!vt_resize(&session, sizes[i][0], sizes[i][1])) {
        failed = test_fail(stats, name, "failed to apply resize %zu", i);
        break;
      }
    }
    if (!vt_expect_text(&session, "Starter Showcase", PTY_TIMEOUT_MS,
                        &snapshot)) {
      failed = test_fail(stats, name,
                         "menu invariant failed after generated action %zu", i);
      break;
    }
  }

  if (!failed && (!vt_send(&session, "q") ||
                  !vt_expect_text(&session, "Return to the shell?",
                                  PTY_TIMEOUT_MS, &snapshot) ||
                  !vt_send(&session, "y"))) {
    failed = test_fail(stats, name, "failed to drive clean exit");
  }
  if (!failed) {
    const int exit_code = vt_wait_for_exit(&session, PTY_TIMEOUT_MS);
    if (exit_code != 0) {
      failed = test_fail(stats, name, "expected exit 0, got %d", exit_code);
    }
  }

  if (!failed) {
    test_pass(stats, name);
  }
  free(snapshot);
  vt_session_close(&session);
  return failed;
}

int run_tui_menu_separator(test_stats_t *stats, const char *binary,
                           bool tui_enabled) {
  const char *name = "tui menu navigation skips separator";
  if (!tui_enabled) {
    test_skip(stats, name, "rebuild with -Denable-tui=true");
    return 0;
  }
  const char *args[] = {"menu"};
  vt_session_t session;
  if (!vt_session_start(&session, binary, args, 1, 80, 24)) {
    return test_fail(stats, name, "failed to start PTY session");
  }
  char *snapshot = NULL;
  int failed = 0;
  if (!vt_expect_text(&session, "Starter Showcase", PTY_TIMEOUT_MS, &snapshot))
    failed = test_fail(stats, name, "initial menu did not render");
  /* Selection starts on Overview. j j j should advance past the separator
   * to Progress Pattern (item 4). */
  if (!failed && !vt_send(&session, "jjj"))
    failed = test_fail(stats, name, "failed to send navigation");
  if (!failed && !vt_send(&session, "\r"))
    failed = test_fail(stats, name, "failed to confirm");
  if (!failed &&
      !vt_expect_text(&session, "Progress Complete", PTY_TIMEOUT_MS, &snapshot))
    failed = test_fail(stats, name, "expected to land on Progress Pattern");
  if (!failed && !vt_send(&session, "x"))
    failed = test_fail(stats, name, "failed to dismiss dialog");
  if (!failed && !vt_send(&session, "q"))
    failed = test_fail(stats, name, "failed to start exit");
  if (!failed && !vt_send(&session, "y"))
    failed = test_fail(stats, name, "failed to confirm exit");
  if (!failed && vt_wait_for_exit(&session, PTY_TIMEOUT_MS) != 0)
    failed = test_fail(stats, name, "process did not exit cleanly");
  if (!failed)
    test_pass(stats, name);
  free(snapshot);
  vt_session_close(&session);
  return failed;
}

int run_tui_menu_resize(test_stats_t *stats, const char *binary,
                        bool tui_enabled) {
  const char *name = "tui menu survives shrink-then-grow resize";
  if (!tui_enabled) {
    test_skip(stats, name, "rebuild with -Denable-tui=true");
    return 0;
  }
  const char *args[] = {"menu"};
  vt_session_t session;
  if (!vt_session_start(&session, binary, args, 1, 100, 30)) {
    return test_fail(stats, name, "failed to start PTY session");
  }
  char *snapshot = NULL;
  int failed = 0;
  if (!vt_expect_text(&session, "Starter Showcase", PTY_TIMEOUT_MS, &snapshot))
    failed = test_fail(stats, name, "initial menu did not render");
  /* Shrink: above minimum but smaller than initial frame_width=72. */
  if (!failed && !vt_resize(&session, 60, 16))
    failed = test_fail(stats, name, "failed to shrink");
  if (!failed &&
      !vt_expect_text(&session, "Starter Showcase", PTY_TIMEOUT_MS, &snapshot))
    failed = test_fail(stats, name, "title vanished after shrink");
  /* Grow back. */
  if (!failed && !vt_resize(&session, 100, 30))
    failed = test_fail(stats, name, "failed to grow");
  if (!failed &&
      !vt_expect_text(&session, "Starter Showcase", PTY_TIMEOUT_MS, &snapshot))
    failed = test_fail(stats, name, "title vanished after grow");
  if (!failed && !vt_send(&session, "q"))
    failed = test_fail(stats, name, "failed to start exit");
  if (!failed && !vt_send(&session, "y"))
    failed = test_fail(stats, name, "failed to confirm exit");
  if (!failed && vt_wait_for_exit(&session, PTY_TIMEOUT_MS) != 0)
    failed = test_fail(stats, name, "process did not exit cleanly");
  if (!failed)
    test_pass(stats, name);
  free(snapshot);
  vt_session_close(&session);
  return failed;
}

int run_tui_menu_mnemonic(test_stats_t *stats, const char *binary,
                          bool tui_enabled) {
  const char *name = "tui menu mnemonic auto-confirms";
  if (!tui_enabled) {
    test_skip(stats, name, "rebuild with -Denable-tui=true");
    return 0;
  }
  const char *args[] = {"menu"};
  vt_session_t session;
  if (!vt_session_start(&session, binary, args, 1, 80, 24)) {
    return test_fail(stats, name, "failed to start PTY session");
  }
  char *snapshot = NULL;
  int failed = 0;
  if (!vt_expect_text(&session, "Starter Showcase", PTY_TIMEOUT_MS, &snapshot))
    failed = test_fail(stats, name, "initial menu did not render");
  /* "&System Information" has unique mnemonic 's' - auto-confirms. */
  if (!failed && !vt_send(&session, "s"))
    failed = test_fail(stats, name, "failed to send 's'");
  if (!failed && !vt_expect_text(&session, "System Information", PTY_TIMEOUT_MS,
                                 &snapshot))
    failed = test_fail(stats, name, "System Information dialog did not appear");
  if (!failed && !vt_expect_text(&session, "Application:", 1000, &snapshot))
    failed = test_fail(stats, name, "Application metadata missing");
  if (!failed && !vt_send(&session, "x"))
    failed = test_fail(stats, name, "failed to dismiss dialog");
  if (!failed && !vt_send(&session, "q"))
    failed = test_fail(stats, name, "failed to start exit");
  if (!failed && !vt_send(&session, "y"))
    failed = test_fail(stats, name, "failed to confirm exit");
  if (!failed && vt_wait_for_exit(&session, PTY_TIMEOUT_MS) != 0)
    failed = test_fail(stats, name, "process did not exit cleanly");
  if (!failed)
    test_pass(stats, name);
  free(snapshot);
  vt_session_close(&session);
  return failed;
}

int run_tui_menu_search(test_stats_t *stats, const char *binary,
                        bool tui_enabled) {
  const char *name = "tui menu search filter narrows and confirms";
  if (!tui_enabled) {
    test_skip(stats, name, "rebuild with -Denable-tui=true");
    return 0;
  }
  const char *args[] = {"menu"};
  vt_session_t session;
  if (!vt_session_start(&session, binary, args, 1, 80, 24)) {
    return test_fail(stats, name, "failed to start PTY session");
  }
  char *snapshot = NULL;
  int failed = 0;
  if (!vt_expect_text(&session, "Starter Showcase", PTY_TIMEOUT_MS, &snapshot))
    failed = test_fail(stats, name, "initial menu did not render");
  if (!failed && !vt_send(&session, "/"))
    failed = test_fail(stats, name, "failed to enter search mode");
  if (!failed &&
      !vt_expect_text(&session, "Search:", PTY_TIMEOUT_MS, &snapshot))
    failed = test_fail(stats, name, "search prompt did not appear");
  if (!failed && !vt_send(&session, "prog"))
    failed = test_fail(stats, name, "failed to type 'prog'");
  if (!failed &&
      !vt_expect_text(&session, "Progress Pattern", PTY_TIMEOUT_MS, &snapshot))
    failed = test_fail(stats, name, "Progress Pattern not filtered in");
  if (!failed && !vt_send(&session, "\r"))
    failed = test_fail(stats, name, "failed to confirm");
  if (!failed &&
      !vt_expect_text(&session, "Progress Complete", PTY_TIMEOUT_MS, &snapshot))
    failed = test_fail(stats, name, "Progress dialog did not appear");
  if (!failed && !vt_send(&session, "x"))
    failed = test_fail(stats, name, "failed to dismiss progress");
  if (!failed && !vt_send(&session, "q"))
    failed = test_fail(stats, name, "failed to start exit");
  if (!failed && !vt_expect_text(&session, "Return to the shell?",
                                 PTY_TIMEOUT_MS, &snapshot))
    failed = test_fail(stats, name, "exit confirm did not appear");
  if (!failed && !vt_send(&session, "y"))
    failed = test_fail(stats, name, "failed to confirm exit");
  if (!failed && vt_wait_for_exit(&session, PTY_TIMEOUT_MS) != 0)
    failed = test_fail(stats, name, "process did not exit cleanly");
  if (!failed)
    test_pass(stats, name);
  free(snapshot);
  vt_session_close(&session);
  return failed;
}
