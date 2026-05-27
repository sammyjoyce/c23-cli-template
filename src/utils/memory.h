/*
 * Optional helpers for handling sensitive data (passwords, tokens, keys).
 *
 * The rest of the codebase uses plain malloc/free/strdup; only reach for these
 * helpers when a buffer actually holds secret material that must not linger in
 * memory or be swapped to disk. They are intentionally narrow: zeroing,
 * allocate-and-lock, and free-with-zero. No realloc/strdup variants - secrets
 * should not grow, and copying them through strdup defeats the purpose.
 */

#pragma once

#include <stddef.h>

#include "../core/types.h"

// Zero memory in a way the compiler cannot optimise away. Safe to call with
// ptr == NULL or len == 0.
void app_secret_zero(void *ptr, size_t len);

// Allocate `size` bytes, zero them, and mlock() (on POSIX) so they cannot be
// swapped to disk. Returns NULL on allocation failure; mlock failures are
// logged but do not fail the call.
APP_NODISCARD void *app_secret_alloc(size_t size);

// Zero the buffer, unlock it, and free it. Safe to call with ptr == NULL.
void app_secret_free(void *ptr, size_t size);
