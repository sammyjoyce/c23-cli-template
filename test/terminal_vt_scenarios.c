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

  const char *named_args[] = {"hello", "Alice"};
  command_result_t named =
      run_cli_command(binary, named_args, 2, NULL, 0, TEST_TIMEOUT_MS);
  failed = failed || expect_success(stats, name, &named) ||
           expect_stdout(stats, name, &named, "Hello, Alice!");
  command_result_free(&named);

  const char *echo_args[] = {"echo", "test", "message"};
  command_result_t echo =
      run_cli_command(binary, echo_args, 3, NULL, 0, TEST_TIMEOUT_MS);
  failed = failed || expect_success(stats, name, &echo) ||
           expect_stdout(stats, name, &echo, "test message");
  command_result_free(&echo);

  const char *info_args[] = {"info"};
  command_result_t info =
      run_cli_command(binary, info_args, 1, NULL, 0, TEST_TIMEOUT_MS);
  failed = failed || expect_success(stats, name, &info) ||
           expect_stdout(stats, name, &info, "Application:") ||
           expect_stdout(stats, name, &info, "Version:");
  command_result_free(&info);

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

  const char *doctor_args[] = {"--quiet", "--json", "doctor"};
  command_result_t doctor =
      run_cli_command(binary, doctor_args, 3, NULL, 0, TEST_TIMEOUT_MS);
  failed = failed || expect_success(stats, name, &doctor);
  if (!failed && doctor.stdout_buf.len != 0) {
    failed = test_fail(stats, name, "quiet doctor wrote stdout: %s",
                       buffer_cstr(&doctor.stdout_buf));
  }
  command_result_free(&doctor);

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

  const char *unknown_args[] = {"not-a-command"};
  command_result_t unknown =
      run_cli_command(binary, unknown_args, 1, NULL, 0, TEST_TIMEOUT_MS);
  if (!failed && unknown.exit_code == 0) {
    failed = test_fail(stats, name, "unknown command unexpectedly succeeded");
  }
  failed =
      failed ||
      expect_stderr(stats, name, &unknown, "Unknown command: not-a-command") ||
      expect_stderr(stats, name, &unknown, "--help");
  command_result_free(&unknown);

  if (!failed) {
    test_pass(stats, name);
  }
  return failed;
}

static bool write_config_json(int fd, const char *json) {
  return write_nonblocking_all(fd, json, strlen(json), TEST_TIMEOUT_MS);
}

int run_config_test(test_stats_t *stats, const char *binary) {
  const char *name = "cli valid config can suppress output";
  char template_path[] = "/tmp/myapp-config-XXXXXX";
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

  const char *echo_args[] = {"echo", "-c", "/definitely/not/a/config.json"};
  command_result_t echo =
      run_cli_command(binary, echo_args, 3, NULL, 0, TEST_TIMEOUT_MS);
  failed =
      failed || expect_success(stats, name, &echo) ||
      expect_stdout(stats, name, &echo, "-c /definitely/not/a/config.json");
  command_result_free(&echo);

  const char *verbose_args[] = {"--verbose", "hello"};
  command_result_t verbose =
      run_cli_command(binary, verbose_args, 2, NULL, 0, TEST_TIMEOUT_MS);
  failed = failed || expect_success(stats, name, &verbose) ||
           expect_stdout(stats, name, &verbose, "Hello, World!") ||
           expect_stderr(stats, name, &verbose, "[INFO]");
  command_result_free(&verbose);

  char template_path[] = "/tmp/myapp-default-config-XXXXXX";
  const int fd = mkstemp(template_path);
  if (fd < 0) {
    failed =
        failed || test_fail(stats, name, "mkstemp failed: %s", strerror(errno));
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

  char *snapshot = NULL;
  int failed = 0;
  if (!vt_expect_text(&session, "Starter Showcase", PTY_TIMEOUT_MS,
                      &snapshot)) {
    print_tail(stderr, "screen:\n", snapshot ? snapshot : "",
               snapshot ? strlen(snapshot) : 0, 4000);
    print_tail(stderr, "transcript:\n", buffer_cstr(&session.transcript),
               session.transcript.len, 4000);
    failed = test_fail(stats, name, "Starter Showcase did not appear");
  }

  const char *menu_labels[] = {"Overview",       "System Information",
                               "Input Dialog",   "Progress Pattern",
                               "Layout Pattern", "Exit"};
  for (size_t i = 0;
       !failed && i < sizeof(menu_labels) / sizeof(menu_labels[0]); i++) {
    if (!vt_expect_text(&session, menu_labels[i], 1000, &snapshot)) {
      failed = test_fail(stats, name, "menu label did not appear: %s",
                         menu_labels[i]);
    }
  }

  if (!failed && !vt_select_menu_index(&session, 0)) {
    failed = test_fail(stats, name, "failed to select Overview");
  }
  if (!failed && !vt_expect_text(&session, "Starter Overview", PTY_TIMEOUT_MS,
                                 &snapshot)) {
    failed = test_fail(stats, name, "Starter Overview did not appear");
  }
  if (!failed && !vt_expect_text(&session, "C23 modules", 1000, &snapshot)) {
    failed = test_fail(stats, name, "overview body did not appear");
  }
  if (!failed && !vt_send(&session, "x")) {
    failed = test_fail(stats, name, "failed to dismiss overview");
  }
  if (!failed && !vt_expect_text(&session, "Starter Showcase", PTY_TIMEOUT_MS,
                                 &snapshot)) {
    failed = test_fail(stats, name, "menu did not reappear after overview");
  }

  if (!failed && !vt_select_menu_index(&session, 1)) {
    failed = test_fail(stats, name, "failed to select System Information");
  }
  if (!failed && !vt_expect_text(&session, "System Information", PTY_TIMEOUT_MS,
                                 &snapshot)) {
    failed = test_fail(stats, name, "System Information did not appear");
  }
  if (!failed &&
      (!vt_expect_text(&session, "Application:", 1000, &snapshot) ||
       !vt_expect_text(&session, "Terminal Size:", 1000, &snapshot) ||
       !vt_expect_text(&session, "Colors Supported:", 1000, &snapshot))) {
    failed = test_fail(stats, name, "system information body was incomplete");
  }
  if (!failed && !vt_send(&session, "x")) {
    failed = test_fail(stats, name, "failed to dismiss system information");
  }
  if (!failed && !vt_expect_text(&session, "Starter Showcase", PTY_TIMEOUT_MS,
                                 &snapshot)) {
    failed = test_fail(stats, name, "menu did not reappear after system info");
  }

  if (!failed && !vt_select_menu_index(&session, 2)) {
    failed = test_fail(stats, name, "failed to select Input Dialog");
  }
  if (!failed &&
      !vt_expect_text(&session, "Input Dialog", PTY_TIMEOUT_MS, &snapshot)) {
    failed = test_fail(stats, name, "Input Dialog did not appear");
  }
  if (!failed &&
      !vt_expect_text(&session, "Enter a display name:", 1000, &snapshot)) {
    failed = test_fail(stats, name, "input prompt did not appear");
  }
  if (!failed && !vt_send(&session, "Ada Lovelace\r")) {
    failed = test_fail(stats, name, "failed to submit input dialog text");
  }
  if (!failed &&
      !vt_expect_text(&session, "Input Captured", PTY_TIMEOUT_MS, &snapshot)) {
    failed = test_fail(stats, name, "Input Captured did not appear");
  }
  if (!failed &&
      !vt_expect_text(&session, "Hello, Ada Lovelace.", 1000, &snapshot)) {
    failed = test_fail(stats, name, "captured input message did not appear");
  }
  if (!failed && !vt_send(&session, "x")) {
    failed = test_fail(stats, name, "failed to dismiss input captured message");
  }
  if (!failed && !vt_expect_text(&session, "Starter Showcase", PTY_TIMEOUT_MS,
                                 &snapshot)) {
    failed = test_fail(stats, name, "menu did not reappear after input flow");
  }

  if (!failed && !vt_select_menu_index(&session, 3)) {
    failed = test_fail(stats, name, "failed to select Progress Pattern");
  }
  if (!failed && !vt_expect_text(&session, "Progress Complete", PTY_TIMEOUT_MS,
                                 &snapshot)) {
    failed = test_fail(stats, name, "Progress Complete did not appear");
  }
  if (!failed &&
      !vt_expect_text(&session, "window lifecycle", 1000, &snapshot)) {
    failed = test_fail(stats, name, "progress completion body did not appear");
  }
  if (!failed && !vt_send(&session, "x")) {
    failed = test_fail(stats, name, "failed to dismiss progress completion");
  }
  if (!failed && !vt_expect_text(&session, "Starter Showcase", PTY_TIMEOUT_MS,
                                 &snapshot)) {
    failed = test_fail(stats, name, "menu did not reappear after progress");
  }

  if (!failed && !vt_select_menu_index(&session, 4)) {
    failed = test_fail(stats, name, "failed to select Layout Pattern");
  }
  if (!failed &&
      !vt_expect_text(&session, "Layout Pattern", PTY_TIMEOUT_MS, &snapshot)) {
    failed = test_fail(stats, name, "Layout Pattern did not appear");
  }
  if (!failed &&
      (!vt_expect_text(&session, "Composable terminal UI", 1000, &snapshot) ||
       !vt_expect_text(&session, "Enter/Esc closes this panel", 1000,
                       &snapshot))) {
    failed = test_fail(stats, name, "layout screen body was incomplete");
  }
  if (!failed && !vt_send(&session, "\x1b")) {
    failed = test_fail(stats, name, "failed to close layout screen");
  }
  if (!failed && !vt_expect_text(&session, "Starter Showcase", PTY_TIMEOUT_MS,
                                 &snapshot)) {
    failed = test_fail(stats, name, "menu did not reappear after layout");
  }

  if (!failed && !vt_resize(&session, 100, 28)) {
    failed = test_fail(stats, name, "failed to resize PTY/Ghostty terminal");
  }
  if (!failed && !vt_expect_text(&session, "Starter Showcase", PTY_TIMEOUT_MS,
                                 &snapshot)) {
    failed =
        test_fail(stats, name, "Starter Showcase disappeared after resize");
  }

  if (!failed && !vt_send(&session, "q")) {
    failed = test_fail(stats, name, "failed to send q");
  }
  if (!failed && !vt_expect_text(&session, "Return to the shell?",
                                 PTY_TIMEOUT_MS, &snapshot)) {
    failed = test_fail(stats, name, "exit confirmation did not appear");
  }
  if (!failed && !vt_send(&session, "n")) {
    failed = test_fail(stats, name, "failed to cancel exit confirmation");
  }
  if (!failed && !vt_expect_text(&session, "Starter Showcase", PTY_TIMEOUT_MS,
                                 &snapshot)) {
    failed = test_fail(stats, name, "menu did not reappear after exit cancel");
  }

  if (!failed && !vt_select_menu_index(&session, 5)) {
    failed = test_fail(stats, name, "failed to select Exit");
  }
  if (!failed && !vt_expect_text(&session, "Return to the shell?",
                                 PTY_TIMEOUT_MS, &snapshot)) {
    failed = test_fail(stats, name, "exit menu confirmation did not appear");
  }
  if (!failed && !vt_send(&session, "y")) {
    failed = test_fail(stats, name, "failed to confirm exit");
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
