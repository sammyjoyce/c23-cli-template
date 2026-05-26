#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || \
    defined(__NetBSD__)
#include <util.h>
#else
#include <pty.h>
#endif

#include <ghostty/vt.h>

#define TEST_TIMEOUT_MS 5000
#define PTY_TIMEOUT_MS 8000
#define READ_CHUNK 4096

#ifndef APP_NAME
#define APP_NAME "myapp"
#endif

typedef struct {
  char *data;
  size_t len;
  size_t cap;
} buffer_t;

typedef struct {
  int passed;
  int failed;
  int skipped;
} test_stats_t;

typedef struct {
  const char *key;
  const char *value;
} env_var_t;

typedef struct {
  int exit_code;
  bool timed_out;
  buffer_t stdout_buf;
  buffer_t stderr_buf;
} command_result_t;

typedef struct {
  int master_fd;
  pid_t pid;
  GhosttyTerminal terminal;
  uint16_t cols;
  uint16_t rows;
  buffer_t transcript;
  bool child_exited;
  int exit_code;
} vt_session_t;

static void buffer_free(buffer_t *buf) {
  if (!buf) {
    return;
  }
  free(buf->data);
  buf->data = NULL;
  buf->len = 0;
  buf->cap = 0;
}

static bool buffer_reserve(buffer_t *buf, size_t needed) {
  if (needed <= buf->cap) {
    return true;
  }

  size_t new_cap = buf->cap == 0 ? 256 : buf->cap;
  while (new_cap < needed) {
    if (new_cap > SIZE_MAX / 2) {
      return false;
    }
    new_cap *= 2;
  }

  char *next = realloc(buf->data, new_cap);
  if (!next) {
    return false;
  }
  buf->data = next;
  buf->cap = new_cap;
  return true;
}

static bool buffer_append(buffer_t *buf, const void *data, size_t len) {
  if (len == 0) {
    return true;
  }
  if (buf->len > SIZE_MAX - len - 1) {
    return false;
  }
  const size_t needed = buf->len + len + 1;
  if (!buffer_reserve(buf, needed)) {
    return false;
  }
  memcpy(buf->data + buf->len, data, len);
  buf->len += len;
  buf->data[buf->len] = '\0';
  return true;
}

static const char *buffer_cstr(const buffer_t *buf) {
  return buf && buf->data ? buf->data : "";
}

static int64_t monotonic_ms(void) {
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
    return 0;
  }
  return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static void print_tail(FILE *stream, const char *label, const char *text,
                       size_t len, size_t max_len) {
  fprintf(stream, "%s", label);
  if (!text || len == 0) {
    fputs("<empty>\n", stream);
    return;
  }

  size_t offset = 0;
  if (len > max_len) {
    offset = len - max_len;
    fprintf(stream, "...<last %zu bytes>...\n", max_len);
  }
  fwrite(text + offset, 1, len - offset, stream);
  fputc('\n', stream);
}

static void test_pass(test_stats_t *stats, const char *name) {
  stats->passed++;
  printf("ok %d - %s\n", stats->passed + stats->failed + stats->skipped, name);
}

static void test_skip(test_stats_t *stats, const char *name, const char *why) {
  stats->skipped++;
  printf("ok %d - %s # SKIP %s\n",
         stats->passed + stats->failed + stats->skipped, name, why);
}

static int test_fail(test_stats_t *stats, const char *name, const char *fmt,
                     ...) {
  stats->failed++;
  printf("not ok %d - %s\n", stats->passed + stats->failed + stats->skipped,
         name);

  va_list args;
  va_start(args, fmt);
  fputs("  ", stderr);
  vfprintf(stderr, fmt, args);
  fputc('\n', stderr);
  va_end(args);
  return 1;
}

static bool contains_text(const char *haystack, const char *needle) {
  return strstr(haystack ? haystack : "", needle) != NULL;
}

static bool set_nonblocking(int fd) {
  const int flags = fcntl(fd, F_GETFL, 0);
  if (flags < 0) {
    return false;
  }
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

static void close_if_open(int *fd) {
  if (*fd >= 0) {
    close(*fd);
    *fd = -1;
  }
}

static bool write_nonblocking_all(int fd, const void *data, size_t len,
                                  int timeout_ms) {
  const uint8_t *bytes = data;
  size_t written = 0;
  const int64_t deadline = monotonic_ms() + timeout_ms;

  while (written < len) {
    const ssize_t n = write(fd, bytes + written, len - written);
    if (n > 0) {
      written += (size_t)n;
      continue;
    }
    if (n == 0) {
      errno = EIO;
      return false;
    }
    if (errno == EINTR) {
      continue;
    }
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      const int64_t now = monotonic_ms();
      if (now >= deadline) {
        return false;
      }

      int wait_ms = (int)(deadline - now);
      if (wait_ms > 50) {
        wait_ms = 50;
      }
      struct pollfd pfd = {.fd = fd, .events = POLLOUT};
      const int polled = poll(&pfd, 1, wait_ms);
      if (polled < 0) {
        if (errno == EINTR) {
          continue;
        }
        return false;
      }
      if (polled == 0) {
        continue;
      }
      if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
        return false;
      }
      continue;
    }
    return false;
  }
  return true;
}

static char **make_argv(const char *binary, const char *const *args,
                        size_t argc) {
  char **argv = calloc(argc + 2, sizeof(char *));
  if (!argv) {
    return NULL;
  }

  argv[0] = (char *)binary;
  for (size_t i = 0; i < argc; i++) {
    argv[i + 1] = (char *)args[i];
  }
  argv[argc + 1] = NULL;
  return argv;
}

static void apply_child_env(const env_var_t *env, size_t env_len,
                            bool interactive) {
  if (interactive) {
    setenv("TERM", "xterm-256color", 1);
    unsetenv("NO_COLOR");
  } else {
    setenv("TERM", "dumb", 1);
    setenv("NO_COLOR", "1", 1);
  }

  for (size_t i = 0; i < env_len; i++) {
    if (env[i].value) {
      setenv(env[i].key, env[i].value, 1);
    } else {
      unsetenv(env[i].key);
    }
  }
}

static command_result_t run_cli_command(const char *binary,
                                        const char *const *args, size_t argc,
                                        const env_var_t *env, size_t env_len,
                                        int timeout_ms) {
  command_result_t result = {.exit_code = -1};
  int out_pipe[2] = {-1, -1};
  int err_pipe[2] = {-1, -1};

  if (pipe(out_pipe) != 0 || pipe(err_pipe) != 0) {
    perror("pipe");
    goto cleanup;
  }

  const pid_t pid = fork();
  if (pid < 0) {
    perror("fork");
    goto cleanup;
  }

  if (pid == 0) {
    dup2(out_pipe[1], STDOUT_FILENO);
    dup2(err_pipe[1], STDERR_FILENO);
    close(out_pipe[0]);
    close(out_pipe[1]);
    close(err_pipe[0]);
    close(err_pipe[1]);

    apply_child_env(env, env_len, false);
    char **argv = make_argv(binary, args, argc);
    if (!argv) {
      _exit(127);
    }
    execv(binary, argv);
    perror("execv");
    _exit(127);
  }

  close_if_open(&out_pipe[1]);
  close_if_open(&err_pipe[1]);
  set_nonblocking(out_pipe[0]);
  set_nonblocking(err_pipe[0]);

  bool out_open = true;
  bool err_open = true;
  bool child_done = false;
  const int64_t deadline = monotonic_ms() + timeout_ms;

  while (out_open || err_open || !child_done) {
    if (monotonic_ms() > deadline) {
      result.timed_out = true;
      kill(pid, SIGKILL);
      break;
    }

    struct pollfd fds[2];
    nfds_t nfds = 0;
    if (out_open) {
      fds[nfds++] = (struct pollfd){.fd = out_pipe[0], .events = POLLIN};
    }
    if (err_open) {
      fds[nfds++] = (struct pollfd){.fd = err_pipe[0], .events = POLLIN};
    }

    const int poll_result = nfds > 0 ? poll(fds, nfds, 25) : 0;
    if (poll_result > 0) {
      for (nfds_t i = 0; i < nfds; i++) {
        if (!(fds[i].revents & (POLLIN | POLLHUP | POLLERR))) {
          continue;
        }

        char chunk[READ_CHUNK];
        while (true) {
          const ssize_t n = read(fds[i].fd, chunk, sizeof(chunk));
          if (n > 0) {
            buffer_t *target = fds[i].fd == out_pipe[0] ? &result.stdout_buf
                                                        : &result.stderr_buf;
            if (!buffer_append(target, chunk, (size_t)n)) {
              kill(pid, SIGKILL);
              result.timed_out = true;
              break;
            }
            continue;
          }
          if (n == 0) {
            if (fds[i].fd == out_pipe[0]) {
              out_open = false;
              close_if_open(&out_pipe[0]);
            } else {
              err_open = false;
              close_if_open(&err_pipe[0]);
            }
            break;
          }
          if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
            break;
          }
          if (fds[i].fd == out_pipe[0]) {
            out_open = false;
            close_if_open(&out_pipe[0]);
          } else {
            err_open = false;
            close_if_open(&err_pipe[0]);
          }
          break;
        }
      }
    }

    if (!child_done) {
      int status = 0;
      const pid_t waited = waitpid(pid, &status, WNOHANG);
      if (waited == pid) {
        child_done = true;
        if (WIFEXITED(status)) {
          result.exit_code = WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
          result.exit_code = 128 + WTERMSIG(status);
        }
      }
    }
  }

  if (!child_done) {
    int status = 0;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status)) {
      result.exit_code = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
      result.exit_code = 128 + WTERMSIG(status);
    }
  }

cleanup:
  close_if_open(&out_pipe[0]);
  close_if_open(&out_pipe[1]);
  close_if_open(&err_pipe[0]);
  close_if_open(&err_pipe[1]);
  return result;
}

static void command_result_free(command_result_t *result) {
  buffer_free(&result->stdout_buf);
  buffer_free(&result->stderr_buf);
}

static int expect_success(test_stats_t *stats, const char *name,
                          command_result_t *result) {
  if (result->timed_out) {
    return test_fail(stats, name, "command timed out");
  }
  if (result->exit_code != 0) {
    print_tail(stderr, "stdout:\n", buffer_cstr(&result->stdout_buf),
               result->stdout_buf.len, 2000);
    print_tail(stderr, "stderr:\n", buffer_cstr(&result->stderr_buf),
               result->stderr_buf.len, 2000);
    return test_fail(stats, name, "expected exit 0, got %d", result->exit_code);
  }
  return 0;
}

static int expect_stdout(test_stats_t *stats, const char *name,
                         command_result_t *result, const char *needle) {
  if (!contains_text(buffer_cstr(&result->stdout_buf), needle)) {
    print_tail(stderr, "stdout:\n", buffer_cstr(&result->stdout_buf),
               result->stdout_buf.len, 2000);
    return test_fail(stats, name, "stdout did not contain %s", needle);
  }
  return 0;
}

static int expect_stderr(test_stats_t *stats, const char *name,
                         command_result_t *result, const char *needle) {
  if (!contains_text(buffer_cstr(&result->stderr_buf), needle)) {
    print_tail(stderr, "stderr:\n", buffer_cstr(&result->stderr_buf),
               result->stderr_buf.len, 2000);
    return test_fail(stats, name, "stderr did not contain %s", needle);
  }
  return 0;
}

static void ghostty_write_pty(GhosttyTerminal terminal, void *userdata,
                              const uint8_t *data, size_t len) {
  (void)terminal;
  vt_session_t *session = userdata;
  if (!session || session->master_fd < 0) {
    return;
  }

  (void)write_nonblocking_all(session->master_fd, data, len, PTY_TIMEOUT_MS);
}

static bool vt_session_start(vt_session_t *session, const char *binary,
                             const char *const *args, size_t argc,
                             uint16_t cols, uint16_t rows) {
  memset(session, 0, sizeof(*session));
  session->master_fd = -1;
  session->cols = cols;
  session->rows = rows;

  GhosttyTerminalOptions opts = {
      .cols = cols,
      .rows = rows,
      .max_scrollback = 1000,
  };
  if (ghostty_terminal_new(NULL, &session->terminal, opts) != GHOSTTY_SUCCESS) {
    fputs("failed to create Ghostty VT terminal\n", stderr);
    return false;
  }

  ghostty_terminal_set(session->terminal, GHOSTTY_TERMINAL_OPT_USERDATA,
                       session);
  ghostty_terminal_set(session->terminal, GHOSTTY_TERMINAL_OPT_WRITE_PTY,
                       (const void *)ghostty_write_pty);

  struct winsize ws = {
      .ws_row = rows,
      .ws_col = cols,
      .ws_xpixel = (unsigned short)(cols * 8),
      .ws_ypixel = (unsigned short)(rows * 16),
  };

  char **argv = make_argv(binary, args, argc);
  if (!argv) {
    ghostty_terminal_free(session->terminal);
    session->terminal = NULL;
    return false;
  }

  const pid_t pid = forkpty(&session->master_fd, NULL, NULL, &ws);
  if (pid < 0) {
    perror("forkpty");
    free(argv);
    ghostty_terminal_free(session->terminal);
    session->terminal = NULL;
    return false;
  }

  if (pid == 0) {
    apply_child_env(NULL, 0, true);
    execv(binary, argv);
    perror("execv");
    _exit(127);
  }

  session->pid = pid;
  free(argv);
  set_nonblocking(session->master_fd);
  return true;
}

static void vt_session_close(vt_session_t *session) {
  if (!session) {
    return;
  }

  if (!session->child_exited && session->pid > 0) {
    kill(session->pid, SIGTERM);
    for (int i = 0; i < 20; i++) {
      int status = 0;
      const pid_t waited = waitpid(session->pid, &status, WNOHANG);
      if (waited == session->pid) {
        session->child_exited = true;
        break;
      }
      usleep(25000);
    }
    if (!session->child_exited) {
      kill(session->pid, SIGKILL);
      waitpid(session->pid, NULL, 0);
      session->child_exited = true;
    }
  }

  close_if_open(&session->master_fd);
  if (session->terminal) {
    ghostty_terminal_free(session->terminal);
    session->terminal = NULL;
  }
  buffer_free(&session->transcript);
}

static void vt_session_note_exit(vt_session_t *session, int status) {
  session->child_exited = true;
  if (WIFEXITED(status)) {
    session->exit_code = WEXITSTATUS(status);
  } else if (WIFSIGNALED(status)) {
    session->exit_code = 128 + WTERMSIG(status);
  } else {
    session->exit_code = -1;
  }
}

static void vt_drain(vt_session_t *session, int idle_ms) {
  if (!session || session->master_fd < 0) {
    return;
  }

  const int64_t deadline = monotonic_ms() + idle_ms;
  while (true) {
    const int64_t now = monotonic_ms();
    int wait_ms = 0;
    if (idle_ms > 0) {
      wait_ms = now >= deadline ? 0 : (int)(deadline - now);
      if (wait_ms > 50) {
        wait_ms = 50;
      }
    }

    struct pollfd fd = {.fd = session->master_fd, .events = POLLIN};
    const int polled = poll(&fd, 1, wait_ms);
    if (polled <= 0) {
      break;
    }

    if (!(fd.revents & (POLLIN | POLLHUP | POLLERR))) {
      break;
    }

    bool read_any = false;
    while (true) {
      uint8_t chunk[READ_CHUNK];
      const ssize_t n = read(session->master_fd, chunk, sizeof(chunk));
      if (n > 0) {
        read_any = true;
        buffer_append(&session->transcript, chunk, (size_t)n);
        ghostty_terminal_vt_write(session->terminal, chunk, (size_t)n);
        continue;
      }
      if (n == 0) {
        close_if_open(&session->master_fd);
        break;
      }
      if (errno == EIO) {
        close_if_open(&session->master_fd);
        break;
      }
      if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
        break;
      }
      close_if_open(&session->master_fd);
      break;
    }

    if (!read_any && idle_ms == 0) {
      break;
    }
  }

  if (!session->child_exited && session->pid > 0) {
    int status = 0;
    const pid_t waited = waitpid(session->pid, &status, WNOHANG);
    if (waited == session->pid) {
      vt_session_note_exit(session, status);
    }
  }
}

static char *vt_snapshot_text(vt_session_t *session) {
  GhosttyFormatterTerminalOptions opts =
      GHOSTTY_INIT_SIZED(GhosttyFormatterTerminalOptions);
  opts.emit = GHOSTTY_FORMATTER_FORMAT_PLAIN;
  opts.trim = true;

  GhosttyFormatter formatter = NULL;
  if (ghostty_formatter_terminal_new(NULL, &formatter, session->terminal,
                                     opts) != GHOSTTY_SUCCESS) {
    return NULL;
  }

  uint8_t *raw = NULL;
  size_t len = 0;
  if (ghostty_formatter_format_alloc(formatter, NULL, &raw, &len) !=
      GHOSTTY_SUCCESS) {
    ghostty_formatter_free(formatter);
    return NULL;
  }

  char *text = calloc(len + 1, 1);
  if (text) {
    memcpy(text, raw, len);
  }
  ghostty_free(NULL, raw, len);
  ghostty_formatter_free(formatter);
  return text;
}

static bool vt_expect_text(vt_session_t *session, const char *needle,
                           int timeout_ms, char **last_snapshot) {
  const int64_t deadline = monotonic_ms() + timeout_ms;
  while (monotonic_ms() <= deadline) {
    vt_drain(session, 50);
    char *snapshot = vt_snapshot_text(session);
    if (snapshot && contains_text(snapshot, needle)) {
      if (last_snapshot) {
        free(*last_snapshot);
        *last_snapshot = snapshot;
      } else {
        free(snapshot);
      }
      return true;
    }
    if (last_snapshot) {
      free(*last_snapshot);
      *last_snapshot = snapshot;
    } else {
      free(snapshot);
    }
    if (session->child_exited) {
      break;
    }
  }
  return false;
}

static bool vt_send(vt_session_t *session, const char *bytes) {
  const size_t len = strlen(bytes);
  if (!write_nonblocking_all(session->master_fd, bytes, len, PTY_TIMEOUT_MS)) {
    return false;
  }
  usleep(50000);
  vt_drain(session, 0);
  return true;
}

static bool vt_resize(vt_session_t *session, uint16_t cols, uint16_t rows) {
  struct winsize ws = {
      .ws_row = rows,
      .ws_col = cols,
      .ws_xpixel = (unsigned short)(cols * 8),
      .ws_ypixel = (unsigned short)(rows * 16),
  };
  if (ioctl(session->master_fd, TIOCSWINSZ, &ws) != 0) {
    return false;
  }
  kill(session->pid, SIGWINCH);
  session->cols = cols;
  session->rows = rows;
  return ghostty_terminal_resize(session->terminal, cols, rows, 8, 16) ==
         GHOSTTY_SUCCESS;
}

static int vt_wait_for_exit(vt_session_t *session, int timeout_ms) {
  const int64_t deadline = monotonic_ms() + timeout_ms;
  while (monotonic_ms() <= deadline) {
    vt_drain(session, 50);
    if (session->child_exited) {
      return session->exit_code;
    }
  }
  return -1;
}

static int run_help_test(test_stats_t *stats, const char *binary) {
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

static int run_version_test(test_stats_t *stats, const char *binary) {
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

static int run_builtin_test(test_stats_t *stats, const char *binary) {
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

static int run_json_info_test(test_stats_t *stats, const char *binary) {
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

static int run_quiet_json_test(test_stats_t *stats, const char *binary) {
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

static int run_error_tests(test_stats_t *stats, const char *binary) {
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

static int run_config_test(test_stats_t *stats, const char *binary) {
  const char *name = "cli valid config can suppress output";
  char template_path[] = "/tmp/myapp-config-XXXXXX";
  const int fd = mkstemp(template_path);
  if (fd < 0) {
    return test_fail(stats, name, "mkstemp failed: %s", strerror(errno));
  }
  const char *json = "{\"ignored\":\"debug\",\"quiet\":true}";
  write(fd, json, strlen(json));
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

static int run_cli_flag_precedence_test(test_stats_t *stats,
                                        const char *binary) {
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
    write(fd, json, strlen(json));
    close(fd);

    const char *hello_args[] = {"hello"};
    const env_var_t config_env[] = {{"APP_CONFIG_PATH", template_path}};
    command_result_t hello = run_cli_command(
        binary, hello_args, 1, config_env,
        sizeof(config_env) / sizeof(config_env[0]), TEST_TIMEOUT_MS);
    unlink(template_path);
    failed = failed || expect_success(stats, name, &hello) ||
             expect_stdout(stats, name, &hello, "Hello, World!");
    command_result_free(&hello);
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

static int run_tui_menu_test(test_stats_t *stats, const char *binary,
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

static int run_tui_fuzz_smoke(test_stats_t *stats, const char *binary,
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

static bool parse_bool_arg(const char *value) {
  return strcmp(value, "1") == 0 || strcmp(value, "true") == 0 ||
         strcmp(value, "yes") == 0;
}

int main(int argc, char **argv) {
  const char *binary = "./zig-out/bin/myapp";
  bool tui_enabled = false;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--binary") == 0 && i + 1 < argc) {
      binary = argv[++i];
    } else if (strcmp(argv[i], "--tui-enabled") == 0 && i + 1 < argc) {
      tui_enabled = parse_bool_arg(argv[++i]);
    } else {
      fprintf(
          stderr,
          "usage: %s [--binary PATH] [--tui-enabled 0|1]\nunknown arg: %s\n",
          argv[0], argv[i]);
      return 2;
    }
  }

  struct stat st;
  if (stat(binary, &st) != 0) {
    fprintf(stderr, "terminal-vt tests: binary does not exist: %s\n", binary);
    return 2;
  }

  test_stats_t stats = {0};
  printf("TAP version 13\n");
  run_help_test(&stats, binary);
  run_version_test(&stats, binary);
  run_builtin_test(&stats, binary);
  run_json_info_test(&stats, binary);
  run_quiet_json_test(&stats, binary);
  run_error_tests(&stats, binary);
  run_config_test(&stats, binary);
  run_cli_flag_precedence_test(&stats, binary);
  run_tui_menu_test(&stats, binary, tui_enabled);
  run_tui_fuzz_smoke(&stats, binary, tui_enabled);
  printf("1..%d\n", stats.passed + stats.failed + stats.skipped);
  fprintf(stderr, "%d passed, %d failed, %d skipped\n", stats.passed,
          stats.failed, stats.skipped);
  return stats.failed == 0 ? 0 : 1;
}
