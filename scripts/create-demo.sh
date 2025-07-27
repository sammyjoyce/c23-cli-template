#!/bin/bash
# create-demo.sh - Create animated demos of the TUI
# Requires: asciinema, agg (asciinema gif generator)

set -euo pipefail

# Configuration
DEMO_DIR="docs/demos"
BINARY="./zig-out/bin/myapp"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Ensure dependencies are installed
check_dependencies() {
    local deps=("asciinema" "agg")
    local missing=()
    
    for dep in "${deps[@]}"; do
        if ! command -v "$dep" &> /dev/null; then
            missing+=("$dep")
        fi
    done
    
    if [ ${#missing[@]} -ne 0 ]; then
        echo -e "${RED}Missing dependencies: ${missing[*]}${NC}"
        echo "Install with:"
        echo "  pip install asciinema"
        echo "  cargo install --git https://github.com/asciinema/agg"
        exit 1
    fi
}

# Build the project
build_project() {
    echo -e "${BLUE}Building project...${NC}"
    zig build
    
    if [ ! -f "$BINARY" ]; then
        echo -e "${RED}Binary not found at $BINARY${NC}"
        exit 1
    fi
}

# Create demo directory
setup_demo_dir() {
    mkdir -p "$DEMO_DIR"
    mkdir -p "$DEMO_DIR/recordings"
}

# Record a demo
record_demo() {
    local name="$1"
    local title="$2"
    local script="$3"
    
    echo -e "${BLUE}Recording demo: $title${NC}"
    
    # Create a temporary script file
    local script_file="/tmp/demo-$name.sh"
    cat > "$script_file" << EOF
#!/bin/bash
# Demo: $title

# Clear screen and show title
clear
echo -e "${GREEN}Demo: $title${NC}"
echo ""
sleep 2

# Run the demo commands
$script

# Show completion
echo ""
echo -e "${GREEN}Demo complete!${NC}"
sleep 2
EOF
    
    chmod +x "$script_file"
    
    # Record with asciinema
    asciinema rec \
        --title "$title" \
        --command "$script_file" \
        --overwrite \
        "$DEMO_DIR/recordings/$name.cast"
    
    # Convert to GIF
    echo -e "${BLUE}Converting to GIF...${NC}"
    agg \
        --theme monokai \
        --font-size 14 \
        --line-height 1.4 \
        "$DEMO_DIR/recordings/$name.cast" \
        "$DEMO_DIR/$name.gif"
    
    # Clean up
    rm -f "$script_file"
    
    echo -e "${GREEN}Created $DEMO_DIR/$name.gif${NC}"
}

# Demo 1: Basic Usage
demo_basic_usage() {
    record_demo "basic-usage" "Basic Command Usage" '
# Show help
'$BINARY' --help
sleep 3

# Show version
'$BINARY' --version
sleep 2

# Run hello command
'$BINARY' hello
sleep 2

# Run hello with name
'$BINARY' hello "Demo User"
sleep 2

# Run echo command
'$BINARY' echo "This is a test message"
sleep 2
'
}

# Demo 2: TUI Progress Bar
demo_progress_bar() {
    record_demo "progress-bar" "TUI Progress Bar Demo" '
# Create a test script that shows progress
cat > /tmp/progress-demo.sh << '\''SCRIPT'\''
#!/bin/bash
echo "Starting long operation..."
'$BINARY' progress --steps 10 --delay 500
echo "Operation complete!"
SCRIPT

chmod +x /tmp/progress-demo.sh
/tmp/progress-demo.sh
rm -f /tmp/progress-demo.sh
'
}

# Demo 3: Interactive Mode
demo_interactive() {
    record_demo "interactive" "Interactive TUI Mode" '
# Note: This is a simulated demo since we cannot automate interactive input
echo "Starting interactive mode..."
echo "(Simulated demo - actual interactive mode requires user input)"
sleep 1

# Show what the interactive mode would look like
cat << '\''DEMO'\''
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
DEMO
sleep 3

echo ""
echo "User selects option 1..."
sleep 2

cat << '\''DEMO'\''
┌─────────────────────────────────────┐
│          Process File               │
├─────────────────────────────────────┤
│                                     │
│  Enter filename: data.txt           │
│                                     │
│  [████████████████████░░░░] 75%     │
│                                     │
│  Processing... Please wait          │
│                                     │
└─────────────────────────────────────┘
DEMO
sleep 3
'
}

# Demo 4: Configuration
demo_config() {
    record_demo "configuration" "Configuration Management" '
# Show current configuration
'$BINARY' config show
sleep 3

# Set a configuration value
'$BINARY' config set output.format json
sleep 2

# Get a specific configuration value
'$BINARY' config get output.format
sleep 2

# Reset configuration
'$BINARY' config reset
sleep 2
'
}

# Demo 5: Error Handling
demo_error_handling() {
    record_demo "error-handling" "Error Handling Demo" '
# Try to process a non-existent file
'$BINARY' process /tmp/nonexistent.txt || true
sleep 3

# Show validation error
echo "Invalid data" > /tmp/invalid.txt
'$BINARY' validate /tmp/invalid.txt || true
rm -f /tmp/invalid.txt
sleep 3

# Show helpful error recovery
'$BINARY' --invalid-option || true
sleep 2
'
}

# Create README for demos
create_demo_readme() {
    cat > "$DEMO_DIR/README.md" << 'EOF'
# Demo Gallery

This directory contains animated demonstrations of the CLI application's features.

## Available Demos

### Basic Usage
![Basic Usage Demo](basic-usage.gif)

Demonstrates basic command-line usage including help, version, and simple commands.

### Progress Bar
![Progress Bar Demo](progress-bar.gif)

Shows the TUI progress bar in action during long-running operations.

### Interactive Mode
![Interactive Mode Demo](interactive.gif)

Demonstrates the interactive TUI mode for menu-driven operations.

### Configuration
![Configuration Demo](configuration.gif)

Shows how to manage application configuration through the CLI.

### Error Handling
![Error Handling Demo](error-handling.gif)

Demonstrates graceful error handling and helpful error messages.

## Creating New Demos

To create new demos, use the `scripts/create-demo.sh` script:

```bash
./scripts/create-demo.sh
```

### Requirements

- asciinema: `pip install asciinema`
- agg: `cargo install --git https://github.com/asciinema/agg`

### Adding a New Demo

1. Add a new demo function in `create-demo.sh`
2. Call `record_demo` with appropriate parameters
3. Add the demo to the main execution flow
4. Update this README with the new demo

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
EOF
    
    echo -e "${GREEN}Created $DEMO_DIR/README.md${NC}"
}

# Main execution
main() {
    echo -e "${GREEN}Creating TUI demos...${NC}"
    
    check_dependencies
    build_project
    setup_demo_dir
    
    # Create all demos
    demo_basic_usage
    demo_progress_bar
    demo_interactive
    demo_config
    demo_error_handling
    
    # Create README
    create_demo_readme
    
    echo -e "${GREEN}All demos created successfully!${NC}"
    echo -e "View demos in: ${BLUE}$DEMO_DIR/${NC}"
}

# Run main function
main "$@"