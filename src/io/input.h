/*
 * Input handling for the application.
 *
 * Manages reading data from stdin and files with proper buffering and size
 * limits. We support both stdin and file input to accommodate different
 * integration patterns. The module handles both blocking and async reads for
 * flexibility.
 */

#pragma once

#include "../core/types.h"

// Read input from stdin with automatic buffer growth.
// Returns allocated string that must be freed by caller, or NULL on error.
// Enforces size limits to prevent memory exhaustion from malicious input.
// This blocking read is suitable for most pipeline use cases.
APP_NODISCARD char *app_read_input_from_stdin(void);

// Read input from file with automatic buffer growth.
// Returns allocated string that must be freed by caller, or NULL on error.
// Handles large files efficiently with chunked reading.
APP_NODISCARD char *app_read_input_from_file(const char *filename);

// Read input from stdin asynchronously using C23 thread features.
// Enables timeout support and graceful cancellation for interactive use.
// Falls back to blocking read on platforms without C23 support to maintain
// compatibility while leveraging modern features where available.
#if __STDC_VERSION__ >= 202311L
APP_NODISCARD char *app_read_input_from_stdin_async(void);
#endif