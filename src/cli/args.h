/*
 * Command-line argument parsing for the application.
 *
 * Handles parsing of all command-line options and arguments with proper
 * validation. Supports both short and long option formats for user convenience.
 * The parser integrates with the config system to apply command-line overrides
 * as the highest priority configuration source.
 */

#pragma once

#include "../core/error.h"
#include "../core/types.h"

// Forward declaration avoids circular dependency with config.h.
// The parser updates config based on parsed arguments.
typedef struct app_config app_config_t;

// Parse command line arguments and update configuration accordingly.
// Returns APP_SUCCESS on success, or appropriate error code for invalid
// arguments. Special handling: exits with code 0 for --help/--version (not an
// error). This allows scripts to check app capabilities without error handling.
APP_NODISCARD app_error app_parse_args(int argc, char *argv[],
                                       app_config_t *config);
