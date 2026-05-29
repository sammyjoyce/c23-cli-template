#include "text_layout.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

static int app_text_wc_columns(wchar_t wc) {
#ifdef _WIN32
  return iswprint(wc) ? 1 : 0;
#else
  const int w = wcwidth(wc);
  return w > 0 ? w : 0;
#endif
}

static bool app_text_decode_one(const char *text, size_t remaining,
                                size_t *out_bytes, int *out_columns) {
  mbstate_t state = {0};
  wchar_t wc = 0;
  const size_t n = mbrtowc(&wc, text, remaining, &state);
  if (n == (size_t)-1 || n == (size_t)-2 || n == 0) {
    *out_bytes = remaining > 0 ? 1 : 0;
    *out_columns = remaining > 0 ? 1 : 0;
    return false;
  }
  *out_bytes = n;
  *out_columns = app_text_wc_columns(wc);
  return true;
}

int app_text_width_utf8_n(const char *text, size_t byte_count) {
  if (!text) {
    return 0;
  }
  int columns = 0;
  size_t offset = 0;
  while (offset < byte_count && text[offset] != '\0') {
    size_t bytes = 0;
    int cell_width = 0;
    (void)app_text_decode_one(text + offset, byte_count - offset, &bytes,
                              &cell_width);
    if (bytes == 0) {
      break;
    }
    columns += cell_width;
    offset += bytes;
  }
  return columns;
}

int app_text_width_utf8(const char *text) {
  return text ? app_text_width_utf8_n(text, strlen(text)) : 0;
}

size_t app_text_truncate_utf8_columns(const char *text, int max_columns,
                                      int *out_columns) {
  if (out_columns) {
    *out_columns = 0;
  }
  if (!text || max_columns <= 0) {
    return 0;
  }

  int columns = 0;
  size_t offset = 0;
  const size_t len = strlen(text);
  while (offset < len) {
    size_t bytes = 0;
    int cell_width = 0;
    (void)app_text_decode_one(text + offset, len - offset, &bytes, &cell_width);
    if (bytes == 0 || columns + cell_width > max_columns) {
      break;
    }
    columns += cell_width;
    offset += bytes;
  }
  if (out_columns) {
    *out_columns = columns;
  }
  return offset;
}

static bool emit_empty_line(app_text_line_emit_fn emit, void *user) {
  return emit ? emit(user, "", 0, 0) : false;
}

void app_text_wrap_utf8(const char *text, int width_columns,
                        int first_indent_columns, int next_indent_columns,
                        app_text_line_emit_fn emit, void *user) {
  if (!text || !emit) {
    return;
  }
  if (width_columns <= 0) {
    (void)emit_empty_line(emit, user);
    return;
  }

  int indent = first_indent_columns < 0 ? 0 : first_indent_columns;
  const int next_indent = next_indent_columns < 0 ? 0 : next_indent_columns;
  int line_cols = 0;
  bool line_started = false;
  const char *line_start = NULL;
  size_t line_bytes = 0;
  /* Start of the current line in the source. Leading whitespace between this
   * point and the first word belongs to the line (indentation/bullets) and is
   * preserved, while inter-word space runs are measured for wrap decisions. */
  const char *segment_start = text;
  const char *word = text;

  while (*word) {
    if (*word == '\n') {
      if (!line_started) {
        /* A whitespace-only line still preserves its indentation. */
        const size_t lead_bytes = (size_t)(word - segment_start);
        if (lead_bytes > 0) {
          if (!emit(user, segment_start, lead_bytes,
                    app_text_width_utf8_n(segment_start, lead_bytes))) {
            return;
          }
        } else if (!emit_empty_line(emit, user)) {
          return;
        }
      } else if (!emit(user, line_start, line_bytes, line_cols)) {
        return;
      }
      indent = next_indent;
      line_cols = 0;
      line_started = false;
      line_start = NULL;
      line_bytes = 0;
      word++;
      segment_start = word;
      continue;
    }
    if (*word == ' ') {
      word++;
      continue;
    }

    /* Measure the run of spaces that precede this word within the current
     * line so column accounting matches the bytes actually emitted. For the
     * first word these are LEADING spaces (indentation): include them in the
     * emitted span. For later words they are inter-word separators. In both
     * cases the run starts at `segment_start` (the line origin) for the first
     * word, or at `line_start` for subsequent words. */
    const char *run_origin = line_started ? line_start : segment_start;
    int separator_cols = 0;
    {
      const char *sep = word;
      while (sep > run_origin && sep[-1] == ' ') {
        sep--;
        separator_cols++;
      }
    }

    const char *end = word;
    while (*end && *end != ' ' && *end != '\n') {
      end++;
    }
    const size_t word_bytes = (size_t)(end - word);
    const int word_cols = app_text_width_utf8_n(word, word_bytes);
    const int available = width_columns > indent ? width_columns - indent : 1;

    if (line_started && line_cols + separator_cols + word_cols > available) {
      if (!emit(user, line_start, line_bytes, line_cols)) {
        return;
      }
      indent = next_indent;
      line_cols = 0;
      line_started = false;
      line_start = NULL;
      line_bytes = 0;
      /* The wrapped word begins a fresh line with no leading indentation. */
      run_origin = word;
      separator_cols = 0;
    }

    if (!line_started) {
      /* Begin the line at the leading-whitespace origin so indentation and
       * bullet spacing are preserved in the emitted bytes and columns. */
      line_start = run_origin;
      line_bytes = (size_t)(end - run_origin);
      line_cols = separator_cols + word_cols;
      line_started = true;
    } else {
      line_bytes = (size_t)(end - line_start);
      line_cols += separator_cols + word_cols;
    }
    word = end;
  }

  if (line_started) {
    (void)emit(user, line_start, line_bytes, line_cols);
  } else {
    /* Trailing whitespace-only content preserves its indentation too. */
    const size_t lead_bytes = (size_t)(word - segment_start);
    if (lead_bytes > 0) {
      (void)emit(user, segment_start, lead_bytes,
                 app_text_width_utf8_n(segment_start, lead_bytes));
    } else {
      (void)emit_empty_line(emit, user);
    }
  }
}
