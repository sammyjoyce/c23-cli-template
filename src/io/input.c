/*
 * Input handling implementation.
 */

#include "input.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../utils/logging.h"
#include "../utils/memory.h"

// Compile-time assertions
static_assert(INPUT_MAX_SIZE >= 512 * 1024, "Input max size too small");
static_assert(INPUT_BUFFER_INITIAL_SIZE >= 8192,
              "Initial input buffer too small");

char *app_read_input_from_stdin(void) {
  if (stdin == NULL) {
    LOG_ERROR("stdin is NULL");
    return nullptr;
  }

  // Optimize initial allocation by checking if stdin is a regular file
  size_t capacity = INPUT_BUFFER_INITIAL_SIZE;
  struct stat st;
  if (fstat(fileno(stdin), &st) == 0 && S_ISREG(st.st_mode) && st.st_size > 0) {
    capacity = st.st_size < INPUT_MAX_SIZE ? st.st_size + 1 : INPUT_MAX_SIZE;
  }

  if (capacity == 0 || capacity > INPUT_MAX_SIZE) {
    LOG_ERROR("Invalid initial buffer capacity");
    return nullptr;
  }

  char *buffer = app_secure_malloc(capacity);
  if (buffer == NULL) {
    errno = ENOMEM;
    return nullptr;
  }

  size_t total_size = 0;
  while (1) {
    if (total_size >= INPUT_MAX_SIZE) {
      LOG_ERROR("Input exceeds maximum size of %zu bytes",
                (size_t)INPUT_MAX_SIZE);
      app_secure_free(buffer, capacity);
      errno = E2BIG;
      return nullptr;
    }

    size_t remaining = capacity - total_size - 1;
    if (remaining < INPUT_BUFFER_READ_CHUNK_SIZE) {
      size_t new_capacity = capacity * 2;
      if (new_capacity > INPUT_MAX_SIZE) {
        new_capacity = INPUT_MAX_SIZE;
      }

      char *new_buffer = app_secure_realloc(buffer, capacity, new_capacity);
      if (new_buffer == NULL) {
        app_secure_free(buffer, capacity);
        errno = ENOMEM;
        return nullptr;
      }
      buffer = new_buffer;
      capacity = new_capacity;
      remaining = capacity - total_size - 1;
    }

    size_t bytes_to_read = remaining < INPUT_BUFFER_READ_CHUNK_SIZE
                               ? remaining
                               : INPUT_BUFFER_READ_CHUNK_SIZE;
    size_t bytes_read = fread(buffer + total_size, 1, bytes_to_read, stdin);

    if (bytes_read == 0) {
      if (feof(stdin)) {
        break;  // End of file reached
      }
      if (ferror(stdin)) {
        LOG_ERROR("Error reading from stdin: %s", strerror(errno));
        app_secure_free(buffer, capacity);
        return nullptr;
      }
    }

    total_size += bytes_read;
  }

  buffer[total_size] = '\0';

  // Shrink buffer to actual size to save memory
  if (total_size + 1 < capacity) {
    char *final_buffer = app_secure_realloc(buffer, capacity, total_size + 1);
    if (final_buffer != NULL) {
      buffer = final_buffer;
    }
  }

  LOG_DEBUG("Read %zu bytes from stdin", total_size);
  return buffer;
}

char *app_read_input_from_file(const char *filename) {
  if (filename == NULL) {
    LOG_ERROR("filename is NULL");
    return nullptr;
  }

  FILE *file = fopen(filename, "r");
  if (file == NULL) {
    LOG_ERROR("Failed to open file %s: %s", filename, strerror(errno));
    return nullptr;
  }

  // Get file size
  struct stat st;
  if (fstat(fileno(file), &st) != 0) {
    LOG_ERROR("Failed to stat file %s: %s", filename, strerror(errno));
    fclose(file);
    return nullptr;
  }

  if (st.st_size > INPUT_MAX_SIZE) {
    LOG_ERROR("File %s exceeds maximum size of %zu bytes", filename,
              (size_t)INPUT_MAX_SIZE);
    fclose(file);
    errno = E2BIG;
    return nullptr;
  }

  size_t file_size = st.st_size;
  char *buffer = app_secure_malloc(file_size + 1);
  if (buffer == NULL) {
    fclose(file);
    errno = ENOMEM;
    return nullptr;
  }

  size_t bytes_read = fread(buffer, 1, file_size, file);
  if (bytes_read != file_size) {
    LOG_ERROR("Failed to read entire file %s", filename);
    app_secure_free(buffer, file_size + 1);
    fclose(file);
    return nullptr;
  }

  buffer[file_size] = '\0';
  fclose(file);

  LOG_DEBUG("Read %zu bytes from file %s", file_size, filename);
  return buffer;
}

#if __STDC_VERSION__ >= 202311L
char *app_read_input_from_stdin_async(void) {
  // For now, just call the synchronous version
  // In a real implementation, this would use C23 thread features
  return app_read_input_from_stdin();
}
#endif
