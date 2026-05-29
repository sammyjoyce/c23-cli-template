/*
 * Curses-free selectable action descriptors. These let CLI command metadata and
 * TUI menu rows share labels/capabilities without sharing renderers or
 * dispatch.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>

typedef enum {
  APP_ACTION_COMMAND = 0,
  APP_ACTION_CALLBACK,
  APP_ACTION_SEPARATOR,
} app_action_kind_t;

typedef enum {
  APP_ACTION_CAP_NONE = 0,
  APP_ACTION_CAP_INTERACTIVE_TERMINAL = 1u << 0,
  APP_ACTION_CAP_TUI = 1u << 1,
} app_action_capability_t;

typedef struct {
  int id;
  app_action_kind_t kind;
  const char *label;
  const char *description;
  bool disabled;
  const char *command_name;
  unsigned capabilities;
} app_action_item_t;

size_t app_actions_from_commands(app_action_item_t *out, size_t out_count);
