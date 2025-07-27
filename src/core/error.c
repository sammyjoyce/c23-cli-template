/*
 * Error handling implementation for the application.
 *
 * Provides human-readable error messages for all error codes. We use a simple
 * switch statement rather than a lookup table to ensure compile-time
 * verification that all error codes have corresponding messages, preventing
 * oversight when adding new error codes.
 */

#include "error.h"

const char *app_strerror(app_error error_code) {
  switch (error_code) {
  // Success
  case APP_SUCCESS:
    return "Success";

  // Input/configuration errors: Messages guide users to fix their input.
  // We provide specific details about what went wrong to reduce debugging time.
  case APP_ERROR_INVALID_ARG:
    return "Invalid argument";
  case APP_ERROR_INVALID_COMMAND:
    return "Invalid or unknown command";
  case APP_ERROR_CONFIG:
    return "Configuration file error";
  case APP_ERROR_CONFIG_PARSE:
    return "Configuration file parse error";
  case APP_ERROR_CONFIG_INVALID:
    return "Configuration file has invalid values";
  case APP_ERROR_MISSING_ARG:
    return "Missing required argument";
  case APP_ERROR_UNKNOWN_OPTION:
    return "Unknown option";

  // System errors: Messages indicate serious problems requiring system-level
  // fixes. These are kept concise as they often appear in logs that
  // administrators review.
  case APP_ERROR_MEMORY:
    return "Memory allocation error";
  case APP_ERROR_IO:
    return "I/O error";
  case APP_ERROR_PERMISSION:
    return "Permission denied";
  case APP_ERROR_INTERNAL:
    return "Internal error";
  case APP_ERROR_THREADING:
    return "Thread/mutex error";
  case APP_ERROR_RESOURCE:
    return "Resource exhaustion";
  case APP_ERROR_SIGNAL:
    return "Signal handling error";
  case APP_ERROR_NOT_FOUND:
    return "File or resource not found";

  // Data processing errors: Messages help debug data validation issues.
  case APP_ERROR_INVALID_DATA:
    return "Invalid data format";
  case APP_ERROR_PARSE_ERROR:
    return "Parse error";
  case APP_ERROR_VALIDATION:
    return "Validation failed";
  case APP_ERROR_OVERFLOW:
    return "Numeric overflow";
  case APP_ERROR_UNDERFLOW:
    return "Numeric underflow";
  case APP_ERROR_OUT_OF_RANGE:
    return "Value out of range";

  default:
    return "Unknown error";
  }
}