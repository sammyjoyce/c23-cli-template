/*
 * Error handling implementation for the application.
 *
 * Provides human-readable error messages for all concrete error codes.
 */

#include "error.h"

static const app_error_info_t g_app_error_table[] = {
    {APP_SUCCESS, "Success"},
    {APP_ERROR_INVALID_ARG, "Invalid argument"},
    {APP_ERROR_INVALID_COMMAND, "Invalid or unknown command"},
    {APP_ERROR_CONFIG, "Configuration file error"},
    {APP_ERROR_CONFIG_PARSE, "Configuration file parse error"},
    {APP_ERROR_CONFIG_INVALID, "Configuration file has invalid values"},
    {APP_ERROR_MISSING_ARG, "Missing required argument"},
    {APP_ERROR_UNKNOWN_OPTION, "Unknown option"},
    {APP_ERROR_MEMORY, "Memory allocation error"},
    {APP_ERROR_IO, "I/O error"},
    {APP_ERROR_PERMISSION, "Permission denied"},
    {APP_ERROR_INTERNAL, "Internal error"},
    {APP_ERROR_THREADING, "Thread/mutex error"},
    {APP_ERROR_RESOURCE, "Resource exhaustion"},
    {APP_ERROR_SIGNAL, "Signal handling error"},
    {APP_ERROR_NOT_FOUND, "File or resource not found"},
    {APP_ERROR_INVALID_DATA, "Invalid data format"},
    {APP_ERROR_PARSE_ERROR, "Parse error"},
    {APP_ERROR_VALIDATION, "Validation failed"},
    {APP_ERROR_OVERFLOW, "Numeric overflow"},
    {APP_ERROR_UNDERFLOW, "Numeric underflow"},
    {APP_ERROR_OUT_OF_RANGE, "Value out of range"},
};

const app_error_info_t *app_error_table(size_t *count) {
  if (count) {
    *count = sizeof(g_app_error_table) / sizeof(g_app_error_table[0]);
  }
  return g_app_error_table;
}

const char *app_strerror(app_error error_code) {
  size_t count = 0;
  const app_error_info_t *errors = app_error_table(&count);
  for (size_t i = 0; i < count; i++) {
    if (errors[i].code == error_code) {
      return errors[i].description;
    }
  }
  return "Unknown error";
}
