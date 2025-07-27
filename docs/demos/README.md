# Demo Gallery

This directory contains animated demonstrations of the CLI application's features.

> **Note**: To generate the actual demo GIFs, run `./scripts/create-demo.sh` after building the project.

## Available Demos

### Basic Usage
<!-- ![Basic Usage Demo](basic-usage.gif) -->
*Demo showing basic command-line usage including help, version, and simple commands.*

```bash
# Show help
myapp --help

# Show version
myapp --version

# Run commands
myapp hello
myapp hello "Demo User"
myapp echo "Test message"
```

### Progress Bar
<!-- ![Progress Bar Demo](progress-bar.gif) -->
*Demo showing the TUI progress bar during long-running operations.*

```bash
# Show progress for a 10-step operation
myapp progress --steps 10 --delay 500
```

### Interactive Mode
<!-- ![Interactive Mode Demo](interactive.gif) -->
*Demo showing the interactive TUI mode for menu-driven operations.*

```
┌─────────────────────────────────────┐
│        MyApp Interactive Mode       │
├─────────────────────────────────────┤
│                                     │
│  1. Process File                    │
│  2. View Status                     │
│  3. Configure Settings              │
│  4. Exit                            │
│                                     │
│  Select option: _                   │
│                                     │
└─────────────────────────────────────┘
```

### Configuration Management
<!-- ![Configuration Demo](configuration.gif) -->
*Demo showing configuration management through the CLI.*

```bash
# Show configuration
myapp config show

# Set values
myapp config set output.format json

# Get specific values
myapp config get output.format

# Reset to defaults
myapp config reset
```

### Error Handling
<!-- ![Error Handling Demo](error-handling.gif) -->
*Demo showing graceful error handling and helpful error messages.*

```bash
# Non-existent file
myapp process /tmp/nonexistent.txt

# Invalid data
myapp validate invalid-data.txt

# Invalid options
myapp --invalid-option
```

## Creating Demo GIFs

To generate the actual animated GIFs:

1. **Install dependencies**:
   ```bash
   # Install asciinema
   pip install asciinema
   
   # Install agg (asciinema gif generator)
   cargo install --git https://github.com/asciinema/agg
   ```

2. **Build the project**:
   ```bash
   zig build
   ```

3. **Run the demo script**:
   ```bash
   ./scripts/create-demo.sh
   ```

This will create all the demo GIFs in this directory.

## Manual Demo Recording

To record a custom demo:

```bash
# Start recording
asciinema rec docs/demos/custom-demo.cast

# Perform your demo actions
myapp [commands...]

# Stop recording (Ctrl+D)

# Convert to GIF
agg docs/demos/custom-demo.cast docs/demos/custom-demo.gif
```

## Embedding Demos

To embed these demos in documentation:

```markdown
![Demo Name](docs/demos/demo-name.gif)
```

Or with HTML for more control:

```html
<p align="center">
  <img src="docs/demos/demo-name.gif" alt="Demo Name" width="600">
</p>
```

## Demo Best Practices

1. **Keep demos short** - Under 30 seconds
2. **Use clear titles** - Explain what's being demonstrated
3. **Show realistic usage** - Use practical examples
4. **Include errors** - Show how the app handles mistakes
5. **Use consistent styling** - Same terminal theme and size