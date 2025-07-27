/*
 * Output handling for the application.
 *
 * Manages output formatting to support multiple output modes (JSON, plain text,
 * quiet). The module ensures proper formatting based on configuration while
 * maintaining compatibility with downstream tools expecting specific formats.
 * Output goes to stdout for pipeline integration, with errors going to stderr.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../core/types.h"

// Forward declaration prevents circular dependency with config.h.
// Allows output module to respect config settings without tight coupling.
typedef struct app_config app_config_t;

// Output text with appropriate formatting based on configuration.
// Handles JSON output mode, plain text, or colored output as configured.
// The output goes to stdout for normal output or stderr for errors.
void app_output(const char *text, const app_config_t *config, bool is_error);

// Output formatted text similar to printf.
// Provides convenience wrapper around app_output with printf-style formatting.
void app_output_format(const app_config_t *config, bool is_error,
                       const char *fmt, ...);

// Output JSON data with proper formatting.
// Ensures valid JSON output when in JSON mode, with pretty printing option.
void app_output_json(const char *json_string, const app_config_t *config,
                     bool pretty);
