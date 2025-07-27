/*
 * Error handling definitions for the application.
 *
 * Centralizes all error codes to ensure consistent error reporting across the
 * application. By using numeric codes with human-readable descriptions, we
 * enable both programmatic error handling and meaningful user feedback. The
 * error code ranges are designed to help quickly identify the error category
 * during debugging.
 */

#pragma once

#include "types.h"

// Error codes are grouped by category with reserved ranges to aid debugging.
// Each range represents a different layer of the application, making it easier
// to identify where failures occur without examining stack traces.
typedef enum {
  // Success (0): Indicates successful execution
  APP_SUCCESS = 0,

  // Input/configuration errors (1-9): User-correctable errors that typically
  // occur during startup or argument parsing. These errors indicate the user
  // needs to fix their input or configuration rather than a system failure.
  APP_ERROR_INVALID_ARG = 1,
  APP_ERROR_INVALID_COMMAND = 2,
  APP_ERROR_CONFIG = 3,
  APP_ERROR_CONFIG_PARSE = 4,
  APP_ERROR_CONFIG_INVALID = 5,
  APP_ERROR_MISSING_ARG = 6,
  APP_ERROR_UNKNOWN_OPTION = 7,

  // System errors (10-19): Critical failures that typically cannot be recovered
  // from without administrator intervention. These indicate resource
  // exhaustion,
  // permission issues, or internal bugs that require investigation.
  APP_ERROR_MEMORY = 10,
  APP_ERROR_IO = 11,
  APP_ERROR_PERMISSION = 12,
  APP_ERROR_INTERNAL = 13,
  APP_ERROR_THREADING = 14,
  APP_ERROR_RESOURCE = 15,
  APP_ERROR_SIGNAL = 16,
  APP_ERROR_NOT_FOUND = 17,

  // Data processing errors (20-29): Errors that occur during data validation
  // or processing. These might be recoverable depending on the context.
  APP_ERROR_INVALID_DATA = 20,
  APP_ERROR_PARSE_ERROR = 21,
  APP_ERROR_VALIDATION = 22,
  APP_ERROR_OVERFLOW = 23,
  APP_ERROR_UNDERFLOW = 24,
  APP_ERROR_OUT_OF_RANGE = 25,

  // Feature-specific errors (30+): Reserved for application-specific features
  // that may be added by users of this template.
  APP_ERROR_FEATURE_BASE = 30,
} app_error;

// Get human-readable error description for user-facing messages.
// This function ensures users receive meaningful feedback instead of cryptic
// error codes, improving the debugging experience and reducing support burden.
APP_NODISCARD const char *app_strerror(app_error error_code);