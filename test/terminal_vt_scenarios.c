#include "terminal_vt.h"

int run_help_test(test_stats_t *stats, const char *binary) {
  const char *name = "cli help is human readable";
  const char *args[] = {"--help"};
  command_result_t r =
      run_cli_command(binary, args, 1, NULL, 0, TEST_TIMEOUT_MS);
  int failed = expect_success(stats, name, &r) ||
               expect_stdout(stats, name, &r, "USAGE") ||
               expect_stdout(stats, name, &r, "COMMANDS") ||
               expect_stdout(stats, name, &r, "doctor");
  if (!failed) {
    test_pass(stats, name);
  }
  command_result_free(&r);
  return failed;
}

int run_version_test(test_stats_t *stats, const char *binary) {
  const char *name = "cli version starts successfully";
  const char *args[] = {"--version"};
  command_result_t r =
      run_cli_command(binary, args, 1, NULL, 0, TEST_TIMEOUT_MS);
  int failed = expect_success(stats, name, &r);
  if (!failed && r.stdout_buf.len == 0) {
    failed = test_fail(stats, name, "version stdout was empty");
  }
  if (!failed) {
    test_pass(stats, name);
  }
  command_result_free(&r);
  return failed;
}

int run_builtin_test(test_stats_t *stats, const char *binary) {
  const char *name = "cli builtins render expected output";
  int failed = 0;

  const char *hello_args[] = {"hello"};
  command_result_t hello =
      run_cli_command(binary, hello_args, 1, NULL, 0, TEST_TIMEOUT_MS);
  failed = expect_success(stats, name, &hello) ||
           expect_stdout(stats, name, &hello, "Hello, World!");
  command_result_free(&hello);

  if (!failed) {
    const char *named_args[] = {"hello", "Alice"};
    command_result_t named =
        run_cli_command(binary, named_args, 2, NULL, 0, TEST_TIMEOUT_MS);
    failed = expect_success(stats, name, &named) ||
             expect_stdout(stats, name, &named, "Hello, Alice!");
    command_result_free(&named);
  }

  if (!failed) {
    const char *echo_args[] = {"echo", "test", "message"};
    command_result_t echo =
        run_cli_command(binary, echo_args, 3, NULL, 0, TEST_TIMEOUT_MS);
    failed = expect_success(stats, name, &echo) ||
             expect_stdout(stats, name, &echo, "test message");
    command_result_free(&echo);
  }

  if (!failed) {
    const char *info_args[] = {"info"};
    command_result_t info =
        run_cli_command(binary, info_args, 1, NULL, 0, TEST_TIMEOUT_MS);
    failed = expect_success(stats, name, &info) ||
             expect_stdout(stats, name, &info, "Application:") ||
             expect_stdout(stats, name, &info, "Version:");
    command_result_free(&info);
  }

  if (!failed) {
    test_pass(stats, name);
  }
  return failed;
}

int run_json_info_test(test_stats_t *stats, const char *binary) {
  const char *name = "cli json info is versioned machine output";
  const char *args[] = {"--json", "info"};
  command_result_t r =
      run_cli_command(binary, args, 2, NULL, 0, TEST_TIMEOUT_MS);
  int failed = expect_success(stats, name, &r) ||
               expect_stdout(stats, name, &r, "\"format_version\":\"1.0\"") ||
               expect_stdout(stats, name, &r, "\"features\"") ||
               expect_stdout(stats, name, &r, "\"tui\"");
  if (!failed) {
    test_pass(stats, name);
  }
  command_result_free(&r);
  return failed;
}

int run_quiet_json_test(test_stats_t *stats, const char *binary) {
  const char *name = "cli quiet json commands suppress stdout";
  int failed = 0;
  const char *info_args[] = {"--quiet", "--json", "info"};
  command_result_t info =
      run_cli_command(binary, info_args, 3, NULL, 0, TEST_TIMEOUT_MS);
  failed = expect_success(stats, name, &info);
  if (!failed && info.stdout_buf.len != 0) {
    failed = test_fail(stats, name, "quiet info wrote stdout: %s",
                       buffer_cstr(&info.stdout_buf));
  }
  command_result_free(&info);

  if (!failed) {
    const char *doctor_args[] = {"--quiet", "--json", "doctor"};
    command_result_t doctor =
        run_cli_command(binary, doctor_args, 3, NULL, 0, TEST_TIMEOUT_MS);
    failed = expect_success(stats, name, &doctor);
    if (!failed && doctor.stdout_buf.len != 0) {
      failed = test_fail(stats, name, "quiet doctor wrote stdout: %s",
                         buffer_cstr(&doctor.stdout_buf));
    }
    command_result_free(&doctor);
  }

  if (!failed) {
    test_pass(stats, name);
  }
  return failed;
}

int run_error_tests(test_stats_t *stats, const char *binary) {
  const char *name = "cli error paths are actionable";
  int failed = 0;

  const char *config_args[] = {"--config", "/definitely/not/a/config.json",
                               "hello"};
  command_result_t config =
      run_cli_command(binary, config_args, 3, NULL, 0, TEST_TIMEOUT_MS);
  if (config.exit_code == 0) {
    failed = test_fail(stats, name, "missing config unexpectedly succeeded");
  }
  failed = failed ||
           expect_stderr(stats, name, &config, "failed to load config") ||
           expect_stderr(stats, name, &config, "/definitely/not/a/config.json");
  command_result_free(&config);

  if (!failed) {
    const char *unknown_args[] = {"not-a-command"};
    command_result_t unknown =
        run_cli_command(binary, unknown_args, 1, NULL, 0, TEST_TIMEOUT_MS);
    if (unknown.exit_code == 0) {
      failed = test_fail(stats, name, "unknown command unexpectedly succeeded");
    }
    failed = failed ||
             expect_stderr(stats, name, &unknown,
                           "Unknown command: not-a-command") ||
             expect_stderr(stats, name, &unknown, "--help");
    command_result_free(&unknown);
  }

  if (!failed) {
    test_pass(stats, name);
  }
  return failed;
}

static bool make_temp_config_path(char *buf, size_t buf_len, const char *name) {
  const int n = snprintf(buf, buf_len, "/tmp/%s-%s-XXXXXX", APP_NAME, name);
  return n > 0 && (size_t)n < buf_len;
}

static bool write_config_json(int fd, const char *json) {
  return write_nonblocking_all(fd, json, strlen(json), TEST_TIMEOUT_MS);
}

int run_config_test(test_stats_t *stats, const char *binary) {
  const char *name = "cli valid config can suppress output";
  char template_path[256];
  if (!make_temp_config_path(template_path, sizeof(template_path), "config")) {
    return test_fail(stats, name, "config temp path is too long");
  }
  const int fd = mkstemp(template_path);
  if (fd < 0) {
    return test_fail(stats, name, "mkstemp failed: %s", strerror(errno));
  }
  const char *json = "{\"ignored\":\"debug\",\"quiet\":true}";
  if (!write_config_json(fd, json)) {
    const int saved_errno = errno;
    close(fd);
    unlink(template_path);
    return test_fail(stats, name, "failed to write config: %s",
                     strerror(saved_errno));
  }
  close(fd);

  const char *args[] = {"--config", template_path, "hello"};
  command_result_t r =
      run_cli_command(binary, args, 3, NULL, 0, TEST_TIMEOUT_MS);
  unlink(template_path);

  int failed = expect_success(stats, name, &r);
  if (!failed && r.stdout_buf.len != 0) {
    failed = test_fail(stats, name, "quiet config wrote stdout: %s",
                       buffer_cstr(&r.stdout_buf));
  }
  if (!failed) {
    test_pass(stats, name);
  }
  command_result_free(&r);
  return failed;
}

int run_cli_flag_precedence_test(test_stats_t *stats, const char *binary) {
  const char *name = "cli flags and config precedence stay stable";
  int failed = 0;

  const char *plain_args[] = {"--plain", "doctor"};
  const env_var_t force_color[] = {{"FORCE_COLOR", "1"}};
  command_result_t plain = run_cli_command(
      binary, plain_args, 2, force_color,
      sizeof(force_color) / sizeof(force_color[0]), TEST_TIMEOUT_MS);
  failed = expect_success(stats, name, &plain) ||
           expect_stdout(stats, name, &plain, "color_output") ||
           expect_stdout(stats, name, &plain, "disabled for this output");
  command_result_free(&plain);

  if (!failed) {
    const char *echo_args[] = {"echo", "-c", "/definitely/not/a/config.json"};
    command_result_t echo =
        run_cli_command(binary, echo_args, 3, NULL, 0, TEST_TIMEOUT_MS);
    failed =
        expect_success(stats, name, &echo) ||
        expect_stdout(stats, name, &echo, "-c /definitely/not/a/config.json");
    command_result_free(&echo);
  }

  if (!failed) {
    const char *verbose_args[] = {"--verbose", "hello"};
    command_result_t verbose =
        run_cli_command(binary, verbose_args, 2, NULL, 0, TEST_TIMEOUT_MS);
    failed = expect_success(stats, name, &verbose) ||
             expect_stdout(stats, name, &verbose, "Hello, World!") ||
             expect_stderr(stats, name, &verbose, "[INFO]");
    command_result_free(&verbose);
  }

  if (!failed) {
    char template_path[256];
    if (!make_temp_config_path(template_path, sizeof(template_path),
                               "default-config")) {
      failed = test_fail(stats, name, "config temp path is too long");
    }
    const int fd = failed ? -1 : mkstemp(template_path);
    if (fd < 0) {
      failed = failed ||
               test_fail(stats, name, "mkstemp failed: %s", strerror(errno));
    } else {
      const char *json = "{\"quiet\":true,\"ignored\":{\"nested\":true}}";
      if (!write_config_json(fd, json)) {
        const int saved_errno = errno;
        close(fd);
        unlink(template_path);
        failed = failed || test_fail(stats, name, "failed to write config: %s",
                                     strerror(saved_errno));
      } else {
        close(fd);
      }

      if (!failed) {
        const char *hello_args[] = {"hello"};
        const env_var_t config_env[] = {{"APP_CONFIG_PATH", template_path}};
        command_result_t hello = run_cli_command(
            binary, hello_args, 1, config_env,
            sizeof(config_env) / sizeof(config_env[0]), TEST_TIMEOUT_MS);
        failed = expect_success(stats, name, &hello) ||
                 expect_stdout(stats, name, &hello, "Hello, World!");
        command_result_free(&hello);
      }
      unlink(template_path);
    }
  }

  if (!failed) {
    test_pass(stats, name);
  }
  return failed;
}

static bool vt_select_menu_index(vt_session_t *session, int index) {
  for (int i = 0; i < index; i++) {
    if (!vt_send(session, "j")) {
      return false;
    }
  }
  return vt_send(session, "\r");
}

typedef enum {
  TUI_STEP_EXPECT,
  TUI_STEP_SEND,
  TUI_STEP_SELECT,
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
  case TUI_STEP_SELECT:
    return vt_select_menu_index(session, step->menu_index)
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

      {TUI_STEP_SELECT, NULL, 0, 0, 0, 0, "failed to select Overview"},
      {TUI_STEP_EXPECT, "Starter Overview", 0, 0, 0, PTY_TIMEOUT_MS,
       "Starter Overview did not appear"},
      {TUI_STEP_EXPECT, "C23 modules", 0, 0, 0, 1000,
       "overview body did not appear"},
      {TUI_STEP_SEND, "x", 0, 0, 0, 0, "failed to dismiss overview"},
      {TUI_STEP_EXPECT, "Starter Showcase", 0, 0, 0, PTY_TIMEOUT_MS,
       "menu did not reappear after overview"},

      {TUI_STEP_SELECT, NULL, 1, 0, 0, 0,
       "failed to select System Information"},
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

      {TUI_STEP_SELECT, NULL, 2, 0, 0, 0, "failed to select Input Dialog"},
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

      {TUI_STEP_SELECT, NULL, 3, 0, 0, 0, "failed to select Progress Pattern"},
      {TUI_STEP_EXPECT, "Progress Complete", 0, 0, 0, PTY_TIMEOUT_MS,
       "Progress Complete did not appear"},
      {TUI_STEP_EXPECT, "window lifecycle", 0, 0, 0, 1000,
       "progress completion body did not appear"},
      {TUI_STEP_SEND, "x", 0, 0, 0, 0, "failed to dismiss progress completion"},
      {TUI_STEP_EXPECT, "Starter Showcase", 0, 0, 0, PTY_TIMEOUT_MS,
       "menu did not reappear after progress"},

      {TUI_STEP_SELECT, NULL, 4, 0, 0, 0, "failed to select Layout Pattern"},
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
      {TUI_STEP_SELECT, NULL, 5, 0, 0, 0, "failed to select Exit"},
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
