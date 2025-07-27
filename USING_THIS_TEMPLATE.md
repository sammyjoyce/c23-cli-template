# Using This Template

This guide walks you through using this template to create your own CLI application.

## Step 1: Create Your Repository

### Option A: GitHub Web Interface
1. Click the green "Use this template" button
2. Choose a name for your repository
3. Select public or private
4. Click "Create repository from template"

### Option B: GitHub CLI
```bash
gh repo create my-awesome-cli --template yourusername/c23-cli-template --public
```

## Step 2: Wait for Automatic Setup

After creating your repository:
1. GitHub Actions will automatically run a cleanup workflow
2. This workflow will:
   - Replace all placeholders with your repository info
   - Remove template-specific files
   - Update the README
   - Commit the changes

Check the "Actions" tab in your repository to monitor progress.

## Step 3: Clone and Start Development

```bash
# Clone your new repository
git clone https://github.com/YOUR_USERNAME/YOUR_REPO_NAME
cd YOUR_REPO_NAME

# Install Zig (if not already installed)
# Recommended: use zvm
curl -sSL https://raw.githubusercontent.com/tristanisham/zvm/master/install.sh | bash
zvm install master
zvm use master

# Build your application (with TUI support)
zig build

# Or build without TUI if ncurses is not available
zig build -Denable-tui=false

# Run tests
zig build test

# Run your application
./zig-out/bin/YOUR_REPO_NAME --help
```

## Step 4: Customize Your Application

### 1. Update Basic Information

Edit `build.zig`:
```zig
const version_str = "1.0.0";  // Your version
const app_name = "myapp";     // Your app name
```

### 2. Modify Commands

Edit `src/main.c` to add your commands:
```c
static app_error handle_command(const app_config_t *config, 
                                const char *command,
                                int argc, char *argv[]) {
  // Add your commands here
  if (strcmp(command, "mycommand") == 0) {
    // Your command logic
    return APP_SUCCESS;
  }
  
  // ... existing commands ...
}
```

### 3. Update Help Text

Edit `src/cli/help.c` to describe your commands.

### 4. Add Source Files

When adding new `.c` files:
1. Create the file in the appropriate directory
2. Add it to `c_sources` in `build.zig`:
```zig
const c_sources = [_][]const u8{
    "src/main.c",
    // ... existing files ...
    "src/your_new_file.c",  // Add your file here
};
```

## Step 5: Configure CI/CD

The template includes GitHub Actions for CI. You may want to:
- Add deployment steps
- Configure release automation
- Add platform-specific builds

## Common Customizations

### Change Application Name Everywhere

If the automatic replacement didn't catch everything:
```bash
# Find all occurrences
grep -r "myapp" .

# Replace in all files
find . -type f -exec sed -i 's/myapp/yourapp/g' {} +
```

### Add Dependencies

Edit `build.zig.zon` to add Zig dependencies:
```zig
.dependencies = .{
    .aro = .{
        // ... existing ...
    },
    .your_dep = .{
        .url = "...",
        .hash = "...",
    },
},
```

### Configure Logging

Set default log level via environment:
```bash
export APP_LOG_LEVEL=DEBUG
./zig-out/bin/yourapp
```

## Troubleshooting

### Template Cleanup Didn't Run
- Check if GitHub Actions is enabled
- Manually run the replacements listed in `.github/workflows/template-cleanup.yml`

### Build Errors
- Ensure Zig master branch is installed
- Check that you have a C compiler
- Try `zig build -Doptimize=Debug` for more info

### Missing Features
- This template provides a foundation
- Add features as needed for your use case
- Consider contributing improvements back!

## Next Steps

1. Read the generated README.md
2. Explore the example commands
3. Write tests for your functionality
4. Set up your preferred IDE/editor
5. Start building your CLI application!

## Getting Help

- Template issues: Create an issue in the template repository
- Your app issues: Use your own repository's issues
- Zig help: https://ziglang.org/documentation/
- C23 reference: https://en.cppreference.com/w/c/23