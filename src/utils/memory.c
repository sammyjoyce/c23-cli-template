/*
 * Optional helpers for handling sensitive data. See memory.h for the policy:
 * normal allocations belong in plain malloc/free; these helpers are reserved
 * for buffers that genuinely hold secret material.
 */

#include "memory.h"

#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <sys/mman.h>
#endif

#include "logging.h"

void app_secret_zero(void *ptr, size_t len) {
  if (ptr == nullptr || len == 0) {
    return;
  }

#if defined(__STDC_LIB_EXT1__)
  memset_s(ptr, len, 0, len);
#elif defined(__OpenBSD__) || defined(__FreeBSD__) || defined(__NetBSD__)
  explicit_bzero(ptr, len);
#else
  volatile unsigned char *p = ptr;
  while (len--) {
    *p++ = 0;
  }
  __asm__ __volatile__("" : : "r"(ptr) : "memory");
#endif
}

void *app_secret_alloc(size_t size) {
  if (size == 0) {
    return nullptr;
  }

  void *ptr = malloc(size);
  if (ptr == nullptr) {
    return nullptr;
  }

  memset(ptr, 0, size);

#ifndef _WIN32
  if (mlock(ptr, size) != 0) {
    LOG_WARNING("Failed to mlock %zu bytes for secret buffer; check ulimit -l.",
                size);
  }
#endif

  return ptr;
}

void app_secret_free(void *ptr, size_t size) {
  if (ptr == nullptr) {
    return;
  }

  app_secret_zero(ptr, size);
#ifndef _WIN32
  munlock(ptr, size);
#endif
  free(ptr);
}
