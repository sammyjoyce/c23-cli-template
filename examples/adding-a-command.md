# Example: Adding a New Command

This example shows how to add a new `greet` command that greets multiple people.

## 1. Add the Command Handler

In `src/main.c`, add your command to the `handle_command` function:

```c
if (strcmp(command, "greet") == 0) {
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

## 2. Update Help Text

In `src/cli/help.c`, add your command to both help functions:

### In `app_print_concise_help()`

```c
printf("  greet <names...> Greet multiple people\n");
```

### In `app_print_verbose_usage()`

```c
printf("  greet <names...>   Greet multiple people\n");
printf("                     Displays a greeting for each name provided\n\n");
```

## 3. Add to Examples Section

Still in `app_print_verbose_usage()`:

```c
printf("  Greet multiple people:\n");
printf("    $ %s greet Alice Bob Charlie\n", program_name);
printf("    Greetings to:\n");
printf("      👋 Alice\n");
printf("      👋 Bob\n");
printf("      👋 Charlie\n");
printf("    \n");
printf("    Have a great day!\n\n");
```

## 4. Update opencli.json

Add your command to the `commands` array:

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

## 5. Add Tests

In `test/cli_contract_runner.c`, add a test for your command:

```c
static bool test_greet_command(test_context_t *ctx) {
  bool ok = true;

  {
    const char *args[] = {"greet", "Alice"};
    command_result_t result = run_cli(ctx, args, ARRAY_LEN(args), NULL, 0);
    ok = expect_exit(&result, 0) &&
         expect_stdout_contains(&result, "Hello, Alice!") && ok;
    command_result_free(&result);
  }

  {
    const char *args[] = {"greet", "Alice", "Bob"};
    command_result_t result = run_cli(ctx, args, ARRAY_LEN(args), NULL, 0);
    ok = expect_exit(&result, 0) &&
         expect_stdout_contains(&result, "Hello, Alice!") &&
         expect_stdout_contains(&result, "Hello, Bob!") && ok;
    command_result_free(&result);
  }

  {
    const char *args[] = {"greet"};
    command_result_t result = run_cli(ctx, args, ARRAY_LEN(args), NULL, 0);
    ok = expect_not_exit(&result, 0) &&
         expect_stderr_contains(&result, "requires at least one name") && ok;
    command_result_free(&result);
  }

  return ok;
}
```

Then register it in the `tests` array:

```c
{"greet command handles names", test_greet_command},
```

## 6. Build and Test

```bash
# Build
zig build

# Run tests
zig build test

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
