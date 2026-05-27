# Example: Adding a New Command

This example shows how to add a new `greet` command that greets multiple people.

## 1. Add the Command Handler

Create a small command translation unit, for example
`src/cli/commands_greet.c`:

```c
app_error app_cmd_greet(const app_config_t *config, int argc, char **argv) {
    (void)config;

    if (argc == 0) {
        fprintf(stderr, "Error: greet command requires at least one name\n");
        return APP_ERROR_MISSING_ARG;
    }

    printf("Greetings to:\n");
    for (int i = 0; i < argc; i++) {
        printf("  👋 %s\n", argv[i]);
    }

    printf("\nHave a great day!\n");
    return APP_SUCCESS;
}
```

## 2. Register Command Metadata

Add the handler forward declaration, arguments, examples, and command row in
`src/cli/commands.c`. The command table feeds dispatch, help, and the OpenCLI
contract:

```c
app_error app_cmd_greet(const app_config_t *config, int argc, char **argv);

static const app_command_arg_t greet_args[] = {
    {.name = "names",
     .required = true,
     .ordinal = 1,
     .arity_minimum = 1,
     .arity_maximum = APP_ARG_ARITY_UNBOUNDED,
     .description = "Names of people to greet"},
};

static const char *const greet_examples[] = {
    "myapp greet Alice",
    "myapp greet Alice Bob Charlie",
};

static const app_command_t g_app_commands[] = {
    /* existing commands... */
    {.name = "greet",
     .summary = "Greet multiple people.",
     .description = "Greet multiple people with a friendly message",
     .handler = app_cmd_greet,
     .arguments = greet_args,
     .argument_count = sizeof(greet_args) / sizeof(greet_args[0]),
     .examples = greet_examples,
     .example_count = sizeof(greet_examples) / sizeof(greet_examples[0]),
     .requires_terminal = false},
};
```

## 3. Update opencli.json

Run the live contract command and update `opencli.json` with the new command.
`zig build test` fails if the checked-in spec drifts from the binary output.

Your command should appear in the `commands` array like this:

```json
{
  "name": "greet",
  "description": "Greet multiple people with a friendly message",
  "options": [],
  "arguments": [
    {
      "name": "names",
      "required": true,
      "ordinal": 1,
      "arity": {
        "minimum": 1,
        "maximum": null
      },
      "description": "Names of people to greet"
    }
  ],
  "examples": [
    "myapp greet Alice",
    "myapp greet Alice Bob Charlie"
  ]
}
```

## 4. Add Tests

In `test/cli_contract_cases.c`, add a test for your command:

```c
static bool test_greet_command(test_context_t *ctx) {
  bool ok = true;

  {
    const char *args[] = {"greet", "Alice"};
    command_result_t result = cc_run_cli(ctx, args, ARRAY_LEN(args), NULL, 0);
    ok = cc_expect_exit(&result, 0) &&
         cc_expect_stdout_contains(&result, "Alice") && ok;
    cc_command_result_free(&result);
  }

  {
    const char *args[] = {"greet", "Alice", "Bob"};
    command_result_t result = cc_run_cli(ctx, args, ARRAY_LEN(args), NULL, 0);
    ok = cc_expect_exit(&result, 0) &&
         cc_expect_stdout_contains(&result, "Alice") &&
         cc_expect_stdout_contains(&result, "Bob") && ok;
    cc_command_result_free(&result);
  }

  {
    const char *args[] = {"greet"};
    command_result_t result = cc_run_cli(ctx, args, ARRAY_LEN(args), NULL, 0);
    ok = cc_expect_not_exit(&result, 0) &&
         cc_expect_stderr_contains(&result, "requires at least one name") && ok;
    cc_command_result_free(&result);
  }

  return ok;
}
```

Then register it in the `tests` array:

```c
{"greet command handles names", test_greet_command},
```

## 5. Build and Test

```bash
# Build
zig build

# Run tests
zig build test

# Confirm the live OpenCLI contract matches opencli.json
./zig-out/bin/myapp opencli

# Try your new command
./zig-out/bin/myapp greet Alice Bob Charlie
```

## Result

You've successfully added a new command that:

- ✅ Accepts multiple arguments
- ✅ Validates input
- ✅ Has help documentation
- ✅ Is tested
- ✅ Follows the project structure

This pattern can be used to add any command to your CLI application!
