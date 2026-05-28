# Advanced Usage

How to script and integrate the CLI. Every example here runs against the template's
real commands (`hello`, `echo`, `info`, `doctor`, and `opencli`) plus the global flags.
As you add your own commands ([adding a command](adding-a-command.md)), the same
patterns apply.

- [Output formats and streams](#output-formats-and-streams)
- [Parsing output with jq](#parsing-output-with-jq)
- [Exit codes in scripts](#exit-codes-in-scripts)
- [Health checks and smoke tests](#health-checks-and-smoke-tests)
- [Pipelines](#pipelines)
- [Configuration and environment](#configuration-and-environment)
- [Integration patterns](#integration-patterns)

## Output formats and streams

The CLI keeps a stable I/O contract: results go to **stdout**, errors and diagnostics
go to **stderr**, and `--json` produces machine-readable output (always including a
`format_version`) where a command supports it.

```bash
myapp info                       # human-readable
myapp --json info                # machine-readable
myapp --plain info               # no colors, good for logs
NO_COLOR=1 myapp hello Alice     # same, via environment

myapp info > info.txt            # capture stdout
myapp doctor 2> diagnostics.log  # capture stderr only
myapp info > out.txt 2>&1        # capture both
```

Use `--quiet`, `--verbose`, or `--debug` to dial verbosity up or down.

## Parsing output with jq

`myapp opencli` prints the whole CLI as JSON with a stable schema, which makes it ideal for discovery and for asserting behavior in scripts.

```bash
# Every command name
myapp opencli | jq -r '.commands[].name'

# Public exit codes as a table
myapp opencli | jq -r '.exitCodes[] | "\(.code)\t\(.description)"'

# All global options
myapp opencli | jq -r '.options[].name'

# Version straight from the binary
myapp opencli | jq -r '.info.version'
```

Command output works the same way:

```bash
myapp --json info | jq .                  # pretty-print
myapp --json info | jq -r '.format_version'
```

## Exit codes in scripts

Commands return specific exit codes: `0` for success, non-zero for a categorized failure (for example `2` unknown command, `6` missing argument, `7` unknown option). The full list is in the contract:

```bash
myapp opencli | jq -r '.exitCodes[] | "\(.code) \(.description)"'
```

Branch on them like any Unix tool:

```bash
if myapp doctor > /dev/null 2>&1; then
  echo "healthy"
else
  echo "doctor reported problems (exit $?)" >&2
fi

# Fail fast in a pipeline
set -euo pipefail
myapp --json info > build-info.json

# Inspect a specific failure
myapp frobnicate || echo "exit code: $?"   # unknown command -> 2
```

## Health checks and smoke tests

`doctor` exists for exactly this: it reports environment status and exits non-zero on failure. `--deep` also exercises the TUI runtime when a terminal is available (and the TUI build is in use).

```bash
myapp doctor            # quick diagnostics
myapp --json doctor     # machine-readable, for monitoring
myapp doctor --deep     # also smoke-test the TUI
```

It makes a natural CI smoke test or container health check.

## Pipelines

Real commands compose with standard tools.

```bash
# Greet a list of names, one invocation each
printf 'Alice\nBob\nCharlie\n' | xargs -n1 myapp hello

# Same, in parallel
printf 'Alice\nBob\nCharlie\n' | parallel myapp hello

# Feed command output into another stage
myapp echo "build complete" | tr '[:lower:]' '[:upper:]'

# Count the commands the binary exposes
myapp opencli | jq '.commands | length'
```

## Configuration and environment

Configuration resolves by precedence: **CLI args > environment > config file > defaults**.

- **Config file:** `~/.config/myapp/config.json`, or an explicit path via `--config`. It is a flat JSON object of booleans (see [config.json](config.json)).
- **Environment:** `APP_LOG_LEVEL` (`ERROR`, `WARNING`, `INFO`, `DEBUG`) and `NO_COLOR`.

```bash
myapp --config ./myapp.json info   # explicit config file
APP_LOG_LEVEL=DEBUG myapp doctor   # raise log verbosity for one run
myapp --quiet info                 # a CLI flag overrides file and environment
```

## Integration patterns

### Docker

The default build links only libc (no ncurses), so the image stays small. Add ncurses only if you ship the TUI build (`-Denable-tui=true`).

```dockerfile
FROM debian:stable-slim
COPY zig-out/bin/myapp /usr/local/bin/myapp
HEALTHCHECK CMD myapp doctor || exit 1
ENTRYPOINT ["myapp"]
CMD ["--help"]
```

```bash
docker build -t myapp .
docker run --rm myapp --json info
```

### systemd timer

The template has no long-running daemon, so schedule periodic runs with a oneshot service plus a timer.

```ini
# /etc/systemd/system/myapp-health.service
[Unit]
Description=myapp health check

[Service]
Type=oneshot
ExecStart=/usr/local/bin/myapp doctor
```

```ini
# /etc/systemd/system/myapp-health.timer
[Unit]
Description=Run the myapp health check hourly

[Timer]
OnCalendar=hourly
Persistent=true

[Install]
WantedBy=timers.target
```

### cron

```bash
# Log machine-readable info hourly
0 * * * * /usr/local/bin/myapp --json info >> /var/log/myapp-info.log

# Nightly health check, mail on failure
0 2 * * * /usr/local/bin/myapp doctor || echo "myapp doctor failed" | mail -s "myapp" you@example.com
```

### CI contract check

Assert the binary's contract still matches the checked-in spec, the same invariant `zig build test` enforces:

```bash
diff <(myapp opencli) opencli.json
```

## See also

- [Adding a Command](adding-a-command.md)
- [Configuration example](config.json)
- [Public Contracts](../docs/CONTRACTS.md)
- [Documentation index](../docs/README.md)
