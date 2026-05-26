#include "terminal_vt.h"

void buffer_free(buffer_t *buf) {
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

bool buffer_append(buffer_t *buf, const void *data, size_t len) {
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

const char *buffer_cstr(const buffer_t *buf) {
  return buf && buf->data ? buf->data : "";
}

int64_t monotonic_ms(void) {
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
    return 0;
  }
  return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

void print_tail(FILE *stream, const char *label, const char *text, size_t len,
                size_t max_len) {
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

void test_pass(test_stats_t *stats, const char *name) {
  stats->passed++;
  printf("ok %d - %s\n", stats->passed + stats->failed + stats->skipped, name);
}

void test_skip(test_stats_t *stats, const char *name, const char *why) {
  stats->skipped++;
  printf("ok %d - %s # SKIP %s\n",
         stats->passed + stats->failed + stats->skipped, name, why);
}

int test_fail(test_stats_t *stats, const char *name, const char *fmt, ...) {
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

bool contains_text(const char *haystack, const char *needle) {
  return strstr(haystack ? haystack : "", needle) != NULL;
}

bool set_nonblocking(int fd) {
  const int flags = fcntl(fd, F_GETFL, 0);
  if (flags < 0) {
    return false;
  }
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

void close_if_open(int *fd) {
  if (*fd >= 0) {
    close(*fd);
    *fd = -1;
  }
}

bool write_nonblocking_all(int fd, const void *data, size_t len,
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

char **make_argv(const char *binary, const char *const *args, size_t argc) {
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

void apply_child_env(const env_var_t *env, size_t env_len, bool interactive) {
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

command_result_t run_cli_command(const char *binary, const char *const *args,
                                 size_t argc, const env_var_t *env,
                                 size_t env_len, int timeout_ms) {
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

void command_result_free(command_result_t *result) {
  buffer_free(&result->stdout_buf);
  buffer_free(&result->stderr_buf);
}

int expect_success(test_stats_t *stats, const char *name,
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

int expect_stdout(test_stats_t *stats, const char *name,
                  command_result_t *result, const char *needle) {
  if (!contains_text(buffer_cstr(&result->stdout_buf), needle)) {
    print_tail(stderr, "stdout:\n", buffer_cstr(&result->stdout_buf),
               result->stdout_buf.len, 2000);
    return test_fail(stats, name, "stdout did not contain %s", needle);
  }
  return 0;
}

int expect_stderr(test_stats_t *stats, const char *name,
                  command_result_t *result, const char *needle) {
  if (!contains_text(buffer_cstr(&result->stderr_buf), needle)) {
    print_tail(stderr, "stderr:\n", buffer_cstr(&result->stderr_buf),
               result->stderr_buf.len, 2000);
    return test_fail(stats, name, "stderr did not contain %s", needle);
  }
  return 0;
}
