/*
 * Minimal JSON reader for app configuration files.
 */

#pragma once

#include "error.h"

typedef struct {
  bool quiet;
  bool debug;
  bool verbose;
  bool json_output;
  bool plain_output;
  bool no_color;
} app_config_json_state_t;

APP_NODISCARD app_error
app_config_parse_json_state(app_config_json_state_t *staged,
                            const char *content);
