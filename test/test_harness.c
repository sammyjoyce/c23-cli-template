#include "test_harness.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <process.h>
#include <sys/stat.h>
#define close _close
#define dup _dup
#define dup2 _dup2
#define fdopen _fdopen
#define fileno _fileno
#define getpid _getpid
#define open _open
#define unlink _unlink
#else
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

typedef struct {
  const char *name;
  char *previous;
  bool had_previous;
} env_restore_t;

typedef struct {
  char *path;
  int fd;
} temp_file_t;

static char *format_string(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  va_list copy;
  va_copy(copy, args);
  const int needed = vsnprintf(NULL, 0, fmt, copy);
  va_end(copy);
  if (needed < 0) {
    va_end(args);
    return NULL;
  }

  char *buf = malloc((size_t)needed + 1);
  if (!buf) {
    va_end(args);
    return NULL;
  }

  if (vsnprintf(buf, (size_t)needed + 1, fmt, args) < 0) {
    free(buf);
    va_end(args);
    return NULL;
  }
  va_end(args);
  return buf;
}

static char *copy_string(const char *value) {
  if (!value) {
    value = "";
  }
  const size_t len = strlen(value);
  char *copy = malloc(len + 1);
  if (!copy) {
    return NULL;
  }
  memcpy(copy, value, len + 1);
  return copy;
}

static bool set_env_value(const char *name, const char *value) {
#ifdef _WIN32
  return _putenv_s(name, value ? value : "") == 0;
#else
  if (value) {
    return setenv(name, value, 1) == 0;
  }
  return unsetenv(name) == 0;
#endif
}

static bool clear_env_value(const char *name) {
#ifdef _WIN32
  return _putenv_s(name, "") == 0;
#else
  return unsetenv(name) == 0;
#endif
}

static bool apply_env(const env_var_t *env, size_t env_count,
                      env_restore_t *restore) {
  for (size_t i = 0; i < env_count; i++) {
    restore[i].name = env[i].name;
    restore[i].previous = NULL;
    restore[i].had_previous = false;

    const char *previous = getenv(env[i].name);
    if (previous) {
      restore[i].previous = copy_string(previous);
      if (!restore[i].previous) {
        return false;
      }
      restore[i].had_previous = true;
    }

    if (!set_env_value(env[i].name, env[i].value)) {
      return false;
    }
  }
  return true;
}

static void restore_env(env_restore_t *restore, size_t env_count) {
  for (size_t i = env_count; i > 0; i--) {
    env_restore_t *item = &restore[i - 1];
    if (!item->name) {
      continue;
    }
    if (item->had_previous) {
      (void)set_env_value(item->name, item->previous);
    } else {
      (void)clear_env_value(item->name);
    }
    free(item->previous);
    item->previous = NULL;
  }
}

static const char *temp_dir(void) {
#ifdef _WIN32
  const char *dir = getenv("TEMP");
  if (!dir || dir[0] == '\0') {
    dir = getenv("TMP");
  }
  return dir && dir[0] != '\0' ? dir : ".";
#else
  const char *dir = getenv("TMPDIR");
  return dir && dir[0] != '\0' ? dir : "/tmp";
#endif
}

static const char *path_separator(void) {
#ifdef _WIN32
  return "\\";
#else
  return "/";
#endif
}

static char *temp_template(const char *label) {
  const char *dir = temp_dir();
  const size_t len = strlen(dir);
  const bool has_sep = len > 0 && (dir[len - 1] == '/' || dir[len - 1] == '\\');
  return format_string("%s%sc23-cli-%s-%ld-XXXXXX", dir,
                       has_sep ? "" : path_separator(), label, (long)getpid());
}

static bool make_temp_file(temp_file_t *file, const char *label) {
  *file = (temp_file_t){.path = temp_template(label), .fd = -1};
  if (!file->path) {
    return false;
  }
#ifdef _WIN32
  if (_mktemp_s(file->path, strlen(file->path) + 1) != 0) {
    free(file->path);
    *file = (temp_file_t){.fd = -1};
    return false;
  }
  file->fd = open(file->path, _O_CREAT | _O_EXCL | _O_RDWR | _O_BINARY,
                  _S_IREAD | _S_IWRITE);
#else
  file->fd = mkstemp(file->path);
#endif

  if (file->fd < 0) {
    free(file->path);
    *file = (temp_file_t){.fd = -1};
    return false;
  }
  return true;
}

static void close_temp_fd(temp_file_t *file) {
  if (file->fd >= 0) {
    close(file->fd);
    file->fd = -1;
  }
}

static void cleanup_temp_file(temp_file_t *file) {
  close_temp_fd(file);
  if (file->path) {
    (void)unlink(file->path);
    free(file->path);
  }
  *file = (temp_file_t){.fd = -1};
}

static char *read_entire_file(const char *path) {
  FILE *stream = fopen(path, "rb");
  if (!stream) {
    return format_string("failed to open captured output %s: %s", path,
                         strerror(errno));
  }
  if (fseek(stream, 0, SEEK_END) != 0) {
    fclose(stream);
    return copy_string("failed to seek captured output");
  }
  const long size = ftell(stream);
  if (size < 0) {
    fclose(stream);
    return copy_string("failed to size captured output");
  }
  if (fseek(stream, 0, SEEK_SET) != 0) {
    fclose(stream);
    return copy_string("failed to rewind captured output");
  }

  char *buf = malloc((size_t)size + 1);
  if (!buf) {
    fclose(stream);
    return copy_string("out of memory reading captured output");
  }

  const size_t read_len = fread(buf, 1, (size_t)size, stream);
  fclose(stream);
  buf[read_len] = '\0';
  return buf;
}

static command_result_t make_error_result(const char *message) {
  return (command_result_t){
      .exit_code = -1,
      .out = copy_string(""),
      .err = copy_string(message),
  };
}

void command_result_free(command_result_t *result) {
  free(result->out);
  free(result->err);
  *result = (command_result_t){0};
}

static int run_child(char *const *argv, int stdout_fd, int stderr_fd) {
#ifdef _WIN32
  fflush(NULL);
  const int saved_stdout = dup(fileno(stdout));
  const int saved_stderr = dup(fileno(stderr));
  if (saved_stdout < 0 || saved_stderr < 0) {
    if (saved_stdout >= 0) {
      close(saved_stdout);
    }
    if (saved_stderr >= 0) {
      close(saved_stderr);
    }
    return -1;
  }
  if (dup2(stdout_fd, fileno(stdout)) != 0 ||
      dup2(stderr_fd, fileno(stderr)) != 0) {
    const int saved_errno = errno;
    (void)dup2(saved_stdout, fileno(stdout));
    (void)dup2(saved_stderr, fileno(stderr));
    close(saved_stdout);
    close(saved_stderr);
    errno = saved_errno;
    return -1;
  }

  const int status = _spawnv(_P_WAIT, argv[0], (const char *const *)argv);
  const int saved_errno = errno;
  fflush(NULL);
  (void)dup2(saved_stdout, fileno(stdout));
  (void)dup2(saved_stderr, fileno(stderr));
  close(saved_stdout);
  close(saved_stderr);
  errno = saved_errno;
  return status;
#else
  const pid_t pid = fork();
  if (pid < 0) {
    return -1;
  }
  if (pid == 0) {
    if (dup2(stdout_fd, STDOUT_FILENO) < 0 ||
        dup2(stderr_fd, STDERR_FILENO) < 0) {
      _exit(126);
    }
    execv(argv[0], argv);
    fprintf(stderr, "exec failed for %s: %s\n", argv[0], strerror(errno));
    _exit(127);
  }

  int status = 0;
  while (waitpid(pid, &status, 0) < 0) {
    if (errno != EINTR) {
      return -1;
    }
  }
  if (WIFEXITED(status)) {
    return WEXITSTATUS(status);
  }
  if (WIFSIGNALED(status)) {
    return 128 + WTERMSIG(status);
  }
  return -1;
#endif
}

static command_result_t run_process(char *const *argv, const env_var_t *env,
                                    size_t env_count) {
  temp_file_t stdout_file;
  temp_file_t stderr_file;
  if (!make_temp_file(&stdout_file, "stdout")) {
    return make_error_result("failed to create stdout capture file");
  }
  if (!make_temp_file(&stderr_file, "stderr")) {
    cleanup_temp_file(&stdout_file);
    return make_error_result("failed to create stderr capture file");
  }

  env_restore_t *restore = NULL;
  if (env_count > 0) {
    restore = calloc(env_count, sizeof(*restore));
    if (!restore || !apply_env(env, env_count, restore)) {
      if (restore) {
        restore_env(restore, env_count);
      }
      free(restore);
      cleanup_temp_file(&stdout_file);
      cleanup_temp_file(&stderr_file);
      return make_error_result("failed to apply child environment");
    }
  }

  const int status = run_child(argv, stdout_file.fd, stderr_file.fd);
  const int child_errno = errno;
  if (restore) {
    restore_env(restore, env_count);
    free(restore);
  }
  close_temp_fd(&stdout_file);
  close_temp_fd(&stderr_file);

  char *out = read_entire_file(stdout_file.path);
  char *err = read_entire_file(stderr_file.path);
  cleanup_temp_file(&stdout_file);
  cleanup_temp_file(&stderr_file);

  if (status < 0) {
    char *spawn_err = format_string("spawn failed: %s\n%s",
                                    strerror(child_errno), err ? err : "");
    free(err);
    err = spawn_err ? spawn_err : copy_string("spawn failed");
  }

  return (command_result_t){
      .exit_code = status,
      .out = out ? out : copy_string(""),
      .err = err ? err : copy_string(""),
  };
}

command_result_t run_cli(test_context_t *ctx, const char *const *args,
                         size_t arg_count, const env_var_t *env,
                         size_t env_count) {
  char **argv = calloc(arg_count + 2, sizeof(*argv));
  if (!argv) {
    return make_error_result("out of memory building argv");
  }
  argv[0] = (char *)ctx->binary;
  for (size_t i = 0; i < arg_count; i++) {
    argv[i + 1] = (char *)args[i];
  }
  argv[arg_count + 1] = NULL;

  command_result_t result = run_process(argv, env, env_count);
  free(argv);
  return result;
}

void print_result_tail(const command_result_t *result) {
  fprintf(stderr, "exit: %d\nstdout:\n%s\nstderr:\n%s\n", result->exit_code,
          result->out ? result->out : "", result->err ? result->err : "");
}

bool expect_exit(const command_result_t *result, int expected) {
  if (result->exit_code == expected) {
    return true;
  }
  fprintf(stderr, "expected exit %d\n", expected);
  print_result_tail(result);
  return false;
}

bool expect_not_exit(const command_result_t *result, int unexpected) {
  if (result->exit_code != unexpected) {
    return true;
  }
  fprintf(stderr, "expected exit other than %d\n", unexpected);
  print_result_tail(result);
  return false;
}

bool expect_contains(const command_result_t *result, const char *label,
                     const char *haystack, const char *needle) {
  if (strstr(haystack ? haystack : "", needle)) {
    return true;
  }
  fprintf(stderr, "expected %s to contain: %s\n", label, needle);
  print_result_tail(result);
  return false;
}

bool expect_stdout_contains(const command_result_t *result,
                            const char *needle) {
  return expect_contains(result, "stdout", result->out, needle);
}

bool expect_stderr_contains(const command_result_t *result,
                            const char *needle) {
  return expect_contains(result, "stderr", result->err, needle);
}

bool expect_stdout_empty(const command_result_t *result) {
  if (result->out && result->out[0] == '\0') {
    return true;
  }
  fprintf(stderr, "expected empty stdout\n");
  print_result_tail(result);
  return false;
}

bool write_temp_config(const char *content, char **path_out) {
  temp_file_t file;
  if (!make_temp_file(&file, "config")) {
    return false;
  }

  FILE *stream = fdopen(file.fd, "wb");
  file.fd = -1;
  if (!stream) {
    cleanup_temp_file(&file);
    return false;
  }
  const size_t len = strlen(content);
  const bool ok = fwrite(content, 1, len, stream) == len;
  if (fclose(stream) != 0) {
    cleanup_temp_file(&file);
    return false;
  }
  if (!ok) {
    cleanup_temp_file(&file);
    return false;
  }

  *path_out = file.path;
  file.path = NULL;
  cleanup_temp_file(&file);
  return true;
}

bool file_exists(const char *path) {
  FILE *stream = fopen(path, "rb");
  if (!stream) {
    return false;
  }
  fclose(stream);
  return true;
}
