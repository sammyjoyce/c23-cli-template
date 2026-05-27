#ifndef TERMINAL_VT_H
#define TERMINAL_VT_H

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
  int master_fd;
  pid_t pid;
  GhosttyTerminal terminal;
  uint16_t cols;
  uint16_t rows;
  buffer_t transcript;
  bool child_exited;
  bool io_failed;
  int exit_code;
} vt_session_t;

void buffer_free(buffer_t *buf);
bool buffer_append(buffer_t *buf, const void *data, size_t len);
const char *buffer_cstr(const buffer_t *buf);

int64_t monotonic_ms(void);
void print_tail(FILE *stream, const char *label, const char *text, size_t len,
                size_t max_len);
void test_pass(test_stats_t *stats, const char *name);
void test_skip(test_stats_t *stats, const char *name, const char *why);
int test_fail(test_stats_t *stats, const char *name, const char *fmt, ...);
bool contains_text(const char *haystack, const char *needle);
bool set_nonblocking(int fd);
void close_if_open(int *fd);
bool write_nonblocking_all(int fd, const void *data, size_t len,
                           int timeout_ms);

char **make_argv(const char *binary, const char *const *args, size_t argc);
void apply_child_env(bool interactive);

bool vt_session_start(vt_session_t *session, const char *binary,
                      const char *const *args, size_t argc, uint16_t cols,
                      uint16_t rows);
void vt_session_close(vt_session_t *session);
void vt_drain(vt_session_t *session, int idle_ms);
bool vt_expect_text(vt_session_t *session, const char *needle, int timeout_ms,
                    char **last_snapshot);
bool vt_send(vt_session_t *session, const char *bytes);
bool vt_resize(vt_session_t *session, uint16_t cols, uint16_t rows);
int vt_wait_for_exit(vt_session_t *session, int timeout_ms);

int run_tui_menu_test(test_stats_t *stats, const char *binary,
                      bool tui_enabled);
int run_tui_fuzz_smoke(test_stats_t *stats, const char *binary,
                       bool tui_enabled);
int run_tui_menu_search(test_stats_t *stats, const char *binary,
                        bool tui_enabled);
int run_tui_menu_mnemonic(test_stats_t *stats, const char *binary,
                          bool tui_enabled);
int run_tui_menu_separator(test_stats_t *stats, const char *binary,
                           bool tui_enabled);
int run_tui_menu_resize(test_stats_t *stats, const char *binary,
                        bool tui_enabled);

#endif
