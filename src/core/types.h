/*
 * Core type definitions for the application.
 * This header centralizes all fundamental type definitions to ensure
 * consistency across the codebase and prevent circular dependencies. By
 * defining types here, we establish a single source of truth for data
 * structures that multiple modules depend on.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Forward declaration to avoid circular dependency with config.h.
// This allows other headers to use app_config_t* without including the full
// definition.
typedef struct app_config app_config_t;

// Generic buffer type for dynamic data.
// Uses separate size/capacity tracking to enable efficient reallocation
// without constantly calling strlen(). This pattern is borrowed from
// std::vector and similar growable containers.
typedef struct {
  char *data;
  size_t size;
  size_t capacity;
} app_buffer_t;

// C23 compatibility macros ensure code can compile on both C23 and pre-C23
// compilers. These allow us to use modern C23 features while maintaining
// backward compatibility with C11/C17 toolchains that users might have
// installed.
#if __STDC_VERSION__ < 202311L
#define nullptr ((void *)0)
#define typeof_unqual(x) typeof(x)
#define _BitInt(N) __int##N##_t
#else
#define auto __auto_type
#endif

// C23 attribute marks functions whose return values must be checked.
// This catches common errors where callers forget to handle allocation failures
// or error conditions, improving reliability by making such oversights
// compile-time errors.
#if __STDC_VERSION__ >= 202311L
#define APP_NODISCARD [[nodiscard("Return value should be checked")]]
#else
#define APP_NODISCARD
#endif

// Application-wide constants define default values and limits.
// These are centralized here to make configuration changes easier and ensure
// consistent behavior across all modules.

// Version is defined by the build system via -DAPP_VERSION compiler flag.
// This fallback is only used if the build system doesn't provide it.
#ifndef APP_VERSION
#define APP_VERSION "1.0.0"
#endif

// Application name
#ifndef APP_NAME
#define APP_NAME "myapp"
#endif

// Build date - should be provided by build system
#ifndef APP_BUILD_DATE
#define APP_BUILD_DATE "unknown"
#endif

// Buffer and limit constants
#define INPUT_BUFFER_INITIAL_SIZE (128 * 1024)
#define INPUT_BUFFER_READ_CHUNK_SIZE 8192
#define INPUT_MAX_SIZE (512 * 1024)
#define BUFFER_INITIAL_SIZE (64 * 1024)
#define TIMESTAMP_BUFFER_SIZE 32
#define ID_BUFFER_SIZE 64
#define PATH_BUFFER_SIZE 4096
