#ifndef TEST_HARNESS_H
#define TEST_HARNESS_H

#include <stdbool.h>
#include <stddef.h>

#define ARRAY_LEN(items) (sizeof(items) / sizeof((items)[0]))

typedef struct {
  int exit_code;
  char *out;
  char *err;
} command_result_t;

typedef struct {
  const char *name;
  const char *value;
} env_var_t;

typedef struct {
  const char *binary;
  int passed;
  int failed;
} test_context_t;

command_result_t run_cli(test_context_t *ctx, const char *const *args,
                         size_t arg_count, const env_var_t *env,
                         size_t env_count);
void command_result_free(command_result_t *result);

void print_result_tail(const command_result_t *result);
bool expect_exit(const command_result_t *result, int expected);
bool expect_not_exit(const command_result_t *result, int unexpected);
bool expect_contains(const command_result_t *result, const char *label,
                     const char *haystack, const char *needle);
bool expect_stdout_contains(const command_result_t *result, const char *needle);
bool expect_stderr_contains(const command_result_t *result, const char *needle);
bool expect_stdout_empty(const command_result_t *result);

bool write_temp_config(const char *content, char **path_out);
bool file_exists(const char *path);

#endif
