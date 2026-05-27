# Demo Gallery

Animated demonstrations of the CLI starter. Recorded with
[asciinema](https://asciinema.org/) and converted to GIFs with
[agg](https://github.com/asciinema/agg).

> Run `./scripts/create-demo.sh` to (re)generate the GIFs after building.

## Available Demos

### Help and Version
<!-- ![Help and Version](help-and-version.gif) -->

```bash
myapp --help
myapp --version
```

### Hello and Echo
<!-- ![Hello and Echo](hello-and-echo.gif) -->

```bash
myapp hello
myapp hello Alice
myapp echo This is a CLI starter template.
```

### Info and Doctor
<!-- ![Info and Doctor](info-and-doctor.gif) -->

```bash
myapp info
myapp --json info
myapp doctor
```

## Adding More Demos

Open `scripts/create-demo.sh` and add a function modelled on the existing
`demo_*` helpers. Each helper calls `record_demo NAME TITLE SCRIPT`, where
`SCRIPT` is a shell snippet that runs against the freshly built `myapp` binary.

For the interactive TUI menu, drive a pre-recorded asciinema cast rather than
typing live - automated recordings give consistent timing and avoid leaking
your shell prompt.

## Generating GIFs

```bash
# Install dependencies
#   asciinema  - your OS package manager
#   agg        - cargo install --git https://github.com/asciinema/agg

zig build -Denable-tui=true
./scripts/create-demo.sh
```

## Embedding

```markdown
![Demo Name](docs/demos/demo-name.gif)
```

Or with HTML for sizing:

```html
<p align="center">
  <img src="docs/demos/demo-name.gif" alt="Demo Name" width="600">
</p>
```

## Demo Tips

- Keep each cast under ~30 seconds.
- Use clear titles and realistic flags.
- Record at a consistent terminal size (e.g. 100x30).
