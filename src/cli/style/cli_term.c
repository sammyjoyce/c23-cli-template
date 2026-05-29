/*
 * Backend-agnostic terminal policy: detection, profile resolution, width, and
 * the public emit_* entry points. See cli_term.h.
 */

#include "cli_term.h"

#include <stdlib.h>
#include <string.h>

#include "cli_term_internal.h"

#ifdef _WIN32
#include <io.h>
#else
#include <sys/ioctl.h>
#include <unistd.h>
#endif

static int app_cli_stream_fd(FILE *stream) {
#ifdef _WIN32
  return _fileno(stream);
#else
  return fileno(stream);
#endif
}

static bool app_cli_fd_is_tty(int fd) {
#ifdef _WIN32
  return _isatty(fd) != 0;
#else
  return isatty(fd) != 0;
#endif
}

static size_t app_cli_detect_width(int fd) {
#ifndef _WIN32
  struct winsize ws;
  if (ioctl(fd, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) {
    return (size_t)ws.ws_col;
  }
#else
  (void)fd;
#endif
  const char *cols = getenv("COLUMNS");
  if (cols && *cols) {
    long n = strtol(cols, nullptr, 10);
    if (n > 0) {
      return (size_t)n;
    }
  }
  return 0;
}

// Does the environment advertise 24-bit color?
static bool app_cli_env_truecolor(void) {
  const char *ct = getenv("COLORTERM");
  if (ct && (strstr(ct, "truecolor") || strstr(ct, "24bit"))) {
    return true;
  }
  const char *term = getenv("TERM");
  if (term && (strstr(term, "truecolor") || strstr(term, "direct"))) {
    return true;
  }
  return false;
}

static app_cli_color_profile_id app_cli_parse_profile(const char *s) {
  if (!s) {
    return APP_CLI_COLOR_PROFILE_NONE;
  }
  if (strcmp(s, "truecolor") == 0) {
    return APP_CLI_COLOR_PROFILE_TRUECOLOR;
  }
  if (strcmp(s, "256") == 0) {
    return APP_CLI_COLOR_PROFILE_ANSI256;
  }
  if (strcmp(s, "16") == 0) {
    return APP_CLI_COLOR_PROFILE_ANSI16;
  }
  return APP_CLI_COLOR_PROFILE_NONE;
}

// Hard policy that disables styling regardless of any forced test profile:
// explicit config flags, NO_COLOR, or a dumb terminal. These always win so a
// user who sets NO_COLOR never sees escapes.
static bool app_cli_hard_disabled(const app_config_t *config) {
  if (config &&
      (app_config_is_plain_output(config) || app_config_is_no_color(config) ||
       app_config_is_json_output(config))) {
    return true;
  }
  if (app_flag_env_enabled(APP_FLAG_NO_COLOR)) {
    return true;
  }
  const char *term = getenv("TERM");
  if (term && strcmp(term, "dumb") == 0) {
    return true;
  }
  return false;
}

bool app_cli_term_init(app_cli_term_t *term, FILE *stream,
                       const app_config_t *config,
                       const app_cli_term_opts_t *opts) {
  if (!term) {
    return false;
  }
  *term = (app_cli_term_t){0};
  term->stream = stream ? stream : stdout;
  term->fd = app_cli_stream_fd(term->stream);
  term->is_tty = app_cli_fd_is_tty(term->fd);
  term->profile = APP_CLI_COLOR_PROFILE_NONE;

  const char *force_profile = opts && opts->force_profile
                                  ? opts->force_profile
                                  : getenv("APP_CLI_TEST_PROFILE");

  // Width: explicit override, then env test hooks, then live detection.
  size_t width = opts ? opts->force_width : 0;
  if (width == 0) {
    const char *env_w = getenv("APP_CLI_TEST_WIDTH");
    if (!env_w) {
      env_w = getenv("__FANG_TEST_WIDTH");
    }
    if (env_w && *env_w) {
      long n = strtol(env_w, nullptr, 10);
      if (n > 0) {
        width = (size_t)n;
      }
    }
  }
  if (width == 0) {
    width = app_cli_detect_width(term->fd);
  }
  term->width = width;

  bool style;
  if (app_cli_hard_disabled(config)) {
    // NO_COLOR / --plain / --no-color / --json / TERM=dumb always win.
    style = false;
  } else if (force_profile) {
    // A forced profile of "none" disables styling; any other forced profile
    // enables emission even off a TTY so golden tests can exercise it.
    style = app_cli_parse_profile(force_profile) != APP_CLI_COLOR_PROFILE_NONE;
  } else {
    style = getenv("FORCE_COLOR") != nullptr || app_cli_fd_is_tty(term->fd);
  }

  if (!style) {
    term->style_enabled = false;
    return false;
  }

  app_cli_backend_probe(term);

  // Resolve the color profile.
  app_cli_color_profile_id profile;
  if (force_profile) {
    profile = app_cli_parse_profile(force_profile);
  } else if (app_cli_env_truecolor()) {
    profile = APP_CLI_COLOR_PROFILE_TRUECOLOR;
  } else if (term->color_count >= 256) {
    profile = APP_CLI_COLOR_PROFILE_ANSI256;
  } else if (term->color_count >= 8) {
    profile = APP_CLI_COLOR_PROFILE_ANSI16;
  } else {
    profile = APP_CLI_COLOR_PROFILE_NONE;
  }

  term->profile = profile;
  term->style_enabled = profile != APP_CLI_COLOR_PROFILE_NONE;
  if (!term->style_enabled) {
    app_cli_backend_release(term);
  }
  return term->style_enabled;
}

void app_cli_term_deinit(app_cli_term_t *term) {
  if (!term) {
    return;
  }
  app_cli_backend_release(term);
  *term = (app_cli_term_t){0};
}

void app_cli_term_raw(app_cli_term_t *term, const char *s, size_t n) {
  if (!term || !term->stream || !s || n == 0) {
    return;
  }
  fwrite(s, 1, n, term->stream);
}

void app_cli_term_write(app_cli_term_t *term, const char *s, size_t n) {
  app_cli_term_raw(term, s, n);
}

void app_cli_term_puts(app_cli_term_t *term, const char *s) {
  if (s) {
    app_cli_term_raw(term, s, strlen(s));
  }
}

void app_cli_term_emit_attr(app_cli_term_t *term, app_cli_attr_bit attr) {
  if (!term || !term->style_enabled) {
    return;
  }
  app_cli_backend_emit_attr(term, attr);
}

void app_cli_term_emit_indexed(app_cli_term_t *term, bool background,
                               uint16_t index) {
  if (!term || !term->style_enabled) {
    return;
  }
  app_cli_backend_emit_indexed(term, background, index);
}

void app_cli_term_emit_truecolor(app_cli_term_t *term, bool background,
                                 app_rgb_t rgb) {
  if (!term || !term->style_enabled) {
    return;
  }
  // 24-bit SGR is backend-independent: no portable terminfo cap expresses it.
  char buf[32];
  int n = snprintf(buf, sizeof(buf), "\033[%d;2;%u;%u;%um",
                   background ? 48 : 38, rgb.r, rgb.g, rgb.b);
  if (n > 0) {
    app_cli_term_raw(term, buf, (size_t)n);
  }
}

void app_cli_term_emit_reset(app_cli_term_t *term) {
  if (!term || !term->style_enabled) {
    return;
  }
  app_cli_backend_emit_reset(term);
}
