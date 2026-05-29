#include "action_item.h"

#include "../cli/commands.h"

size_t app_actions_from_commands(app_action_item_t *out, size_t out_count) {
  size_t command_count = 0;
  const app_command_t *commands = app_commands(&command_count);
  if (!out || out_count == 0) {
    return command_count;
  }

  const size_t n = command_count < out_count ? command_count : out_count;
  for (size_t i = 0; i < n; i++) {
    const app_command_t *command = &commands[i];
    unsigned capabilities = APP_ACTION_CAP_NONE;
    if (command->requires_terminal) {
      capabilities |= APP_ACTION_CAP_INTERACTIVE_TERMINAL;
      capabilities |= APP_ACTION_CAP_TUI;
    }
    out[i] = (app_action_item_t){.id = (int)i + 1,
                                 .kind = APP_ACTION_COMMAND,
                                 .label = command->name,
                                 .description = command->summary,
                                 .disabled = false,
                                 .command_name = command->name,
                                 .capabilities = capabilities};
  }
  return command_count;
}
