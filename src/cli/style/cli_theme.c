/*
 * CLI theme: the default scheme and scheme->styles compilation. See
 * cli_theme.h.
 */

#include "cli_theme.h"

#include <stdbool.h>
#include <stddef.h>

#include "../../style/design_tokens.h"

// RGB token with a semantic ANSI-16 fallback hint.
#define RGBH(rr, gg, bb, hint)                  \
  ((app_cli_color_t){.kind = APP_CLI_COLOR_RGB, \
                     .rgb = {(rr), (gg), (bb)}, \
                     .has_ansi16_hint = true,   \
                     .ansi16_hint = (hint)})

#define ADAPT(d, l) ((app_cli_adaptive_color_t){.dark = (d), .light = (l)})

// ANSI-16 indices used as hints: 1 red, 2 green, 3 yellow, 7 white,
// 8 bright-black (gray), 11 bright-yellow, 15 bright-white.

// Build an RGB token from a shared-palette triple plus an ANSI-16 hint.
static app_cli_color_t app_cli_rgbh(app_rgb_t rgb, uint8_t hint) {
  return (app_cli_color_t){.kind = APP_CLI_COLOR_RGB,
                           .rgb = rgb,
                           .has_ansi16_hint = true,
                           .ansi16_hint = hint};
}

// Default scheme: amber-on-near-black (dark) with a readable light variant.
// The dark tokens are derived at runtime from the shared design palette
// (APP_DESIGN_PALETTE in design_tokens.h) so the CLI and TUI stay one source of
// truth - no hand-maintained "keep in sync" literals. ERROR_HEADER_FG has no
// palette field and keeps its own literal. The light variants stay as literals.
static app_cli_color_scheme_t APP_CLI_DEFAULT_SCHEME;
static bool APP_CLI_DEFAULT_SCHEME_READY;

static void app_cli_theme_init_default_scheme(void) {
  if (APP_CLI_DEFAULT_SCHEME_READY) {
    return;
  }
  const app_design_palette_t *p = &APP_DESIGN_PALETTE;
  APP_CLI_DEFAULT_SCHEME = (app_cli_color_scheme_t){
      .tokens =
          {
              [APP_CLI_COLOR_TOKEN_BASE] =
                  ADAPT(app_cli_rgbh(p->fg, 7), RGBH(60, 56, 54, 0)),
              [APP_CLI_COLOR_TOKEN_TITLE] =
                  ADAPT(app_cli_rgbh(p->amber, 11), RGBH(135, 94, 20, 3)),
              [APP_CLI_COLOR_TOKEN_DESCRIPTION] =
                  ADAPT(app_cli_rgbh(p->fg, 7), RGBH(60, 56, 54, 0)),
              [APP_CLI_COLOR_TOKEN_CODEBLOCK] =
                  ADAPT(app_cli_rgbh(p->amber, 3), RGBH(110, 78, 20, 3)),
              [APP_CLI_COLOR_TOKEN_PROGRAM] =
                  ADAPT(app_cli_rgbh(p->amber, 11), RGBH(135, 94, 20, 3)),
              [APP_CLI_COLOR_TOKEN_DIMMED_ARGUMENT] =
                  ADAPT(app_cli_rgbh(p->muted, 8), RGBH(120, 112, 105, 8)),
              [APP_CLI_COLOR_TOKEN_COMMENT] =
                  ADAPT(app_cli_rgbh(p->muted, 8), RGBH(120, 112, 105, 8)),
              [APP_CLI_COLOR_TOKEN_FLAG] =
                  ADAPT(app_cli_rgbh(p->amber, 11), RGBH(135, 94, 20, 3)),
              [APP_CLI_COLOR_TOKEN_FLAG_DEFAULT] =
                  ADAPT(app_cli_rgbh(p->muted, 8), RGBH(120, 112, 105, 8)),
              [APP_CLI_COLOR_TOKEN_COMMAND] =
                  ADAPT(app_cli_rgbh(p->amber, 11), RGBH(135, 94, 20, 3)),
              [APP_CLI_COLOR_TOKEN_QUOTED_STRING] =
                  ADAPT(app_cli_rgbh(p->green, 2), RGBH(75, 115, 55, 2)),
              [APP_CLI_COLOR_TOKEN_ARGUMENT] =
                  ADAPT(app_cli_rgbh(p->fg, 7), RGBH(60, 56, 54, 0)),
              [APP_CLI_COLOR_TOKEN_HELP] =
                  ADAPT(app_cli_rgbh(p->amber, 11), RGBH(135, 94, 20, 3)),
              [APP_CLI_COLOR_TOKEN_DASH] =
                  ADAPT(app_cli_rgbh(p->muted, 8), RGBH(120, 112, 105, 8)),
              [APP_CLI_COLOR_TOKEN_ERROR_HEADER_FG] =
                  ADAPT(RGBH(255, 245, 235, 15), RGBH(255, 255, 255, 15)),
              [APP_CLI_COLOR_TOKEN_ERROR_HEADER_BG] =
                  ADAPT(app_cli_rgbh(p->red, 1), RGBH(180, 55, 50, 1)),
              [APP_CLI_COLOR_TOKEN_ERROR_DETAILS] =
                  ADAPT(app_cli_rgbh(p->red, 1), RGBH(145, 40, 35, 1)),
          },
  };
  APP_CLI_DEFAULT_SCHEME_READY = true;
}

const app_cli_color_scheme_t *app_cli_theme_default_scheme(void) {
  app_cli_theme_init_default_scheme();
  return &APP_CLI_DEFAULT_SCHEME;
}

// Default attributes per token.
static app_cli_attr_mask_t app_cli_token_attrs(app_cli_color_token_id token) {
  switch (token) {
  case APP_CLI_COLOR_TOKEN_TITLE:
  case APP_CLI_COLOR_TOKEN_PROGRAM:
  case APP_CLI_COLOR_TOKEN_HELP:
    return APP_CLI_ATTR_BOLD;
  case APP_CLI_COLOR_TOKEN_DIMMED_ARGUMENT:
  case APP_CLI_COLOR_TOKEN_COMMENT:
  case APP_CLI_COLOR_TOKEN_FLAG_DEFAULT:
  case APP_CLI_COLOR_TOKEN_DASH:
    return APP_CLI_ATTR_DIM;
  default:
    return APP_CLI_ATTR_NONE;
  }
}

static app_cli_resolved_color_t app_cli_color_resolve(
    app_cli_color_t color, app_cli_color_profile_id profile, int color_count) {
  switch (color.kind) {
  case APP_CLI_COLOR_UNSET:
    return (app_cli_resolved_color_t){.kind = APP_CLI_RESOLVED_COLOR_NONE};
  case APP_CLI_COLOR_DEFAULT:
    return (app_cli_resolved_color_t){.kind = APP_CLI_RESOLVED_COLOR_DEFAULT};
  default:
    break;
  }

  if (profile == APP_CLI_COLOR_PROFILE_TRUECOLOR &&
      color.kind == APP_CLI_COLOR_RGB) {
    return (app_cli_resolved_color_t){.kind = APP_CLI_RESOLVED_COLOR_RGB,
                                      .rgb = color.rgb};
  }

  if (profile == APP_CLI_COLOR_PROFILE_ANSI256) {
    uint8_t index;
    if (color.kind == APP_CLI_COLOR_ANSI256) {
      index = color.ansi256;
    } else if (color.kind == APP_CLI_COLOR_ANSI16) {
      index = color.ansi16;
    } else {
      index = app_color_rgb_to_xterm256(color.rgb);
    }
    return (app_cli_resolved_color_t){.kind = APP_CLI_RESOLVED_COLOR_INDEXED,
                                      .index = index};
  }

  if (profile == APP_CLI_COLOR_PROFILE_ANSI16) {
    uint8_t index;
    if (color.kind == APP_CLI_COLOR_ANSI16) {
      index = color.ansi16;
    } else if (color.has_ansi16_hint) {
      index = color.ansi16_hint;
    } else if (color.kind == APP_CLI_COLOR_ANSI256) {
      index = color.ansi256 < 16 ? color.ansi256 : 7;
    } else {
      index = app_color_rgb_to_ansi16(color.rgb);
    }
    // Fold bright colors onto the base 8 for true 8-color terminals.
    if (color_count > 0 && color_count < 16 && index >= 8) {
      index = (uint8_t)(index - 8);
    }
    return (app_cli_resolved_color_t){.kind = APP_CLI_RESOLVED_COLOR_INDEXED,
                                      .index = index};
  }

  return (app_cli_resolved_color_t){.kind = APP_CLI_RESOLVED_COLOR_NONE};
}

static app_cli_color_t app_cli_pick(const app_cli_adaptive_color_t *adaptive,
                                    app_cli_theme_mode_id mode) {
  return mode == APP_CLI_THEME_MODE_LIGHT ? adaptive->light : adaptive->dark;
}

void app_cli_styles_compile(app_cli_styles_t *out,
                            const app_cli_color_scheme_t *scheme,
                            app_cli_theme_mode_id mode,
                            app_cli_color_profile_id profile, int color_count) {
  if (!out) {
    return;
  }
  *out = (app_cli_styles_t){0};
  if (!scheme) {
    return;
  }

  for (int i = 0; i < APP_CLI_COLOR_TOKEN_COUNT; i++) {
    app_cli_color_t color = app_cli_pick(&scheme->tokens[i], mode);
    out->tokens[i].fg = app_cli_color_resolve(color, profile, color_count);
    out->tokens[i].attrs = app_cli_token_attrs((app_cli_color_token_id)i);
  }

  // Compose the error header: header fg on header bg, bold.
  out->error_header.fg = app_cli_color_resolve(
      app_cli_pick(&scheme->tokens[APP_CLI_COLOR_TOKEN_ERROR_HEADER_FG], mode),
      profile, color_count);
  out->error_header.bg = app_cli_color_resolve(
      app_cli_pick(&scheme->tokens[APP_CLI_COLOR_TOKEN_ERROR_HEADER_BG], mode),
      profile, color_count);
  out->error_header.attrs = APP_CLI_ATTR_BOLD;
}

const app_cli_style_t *app_cli_style(const app_cli_styles_t *styles,
                                     app_cli_color_token_id token) {
  static const app_cli_style_t empty = {0};
  if (!styles || token < 0 || token >= APP_CLI_COLOR_TOKEN_COUNT) {
    return &empty;
  }
  return &styles->tokens[token];
}
