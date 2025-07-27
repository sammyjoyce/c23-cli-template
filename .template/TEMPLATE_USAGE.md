# Template Usage Guide

## Quick Start

### 1. Create Your Repository

**GitHub Web Interface:**
1. Click "Use this template" button
2. Name your repository
3. Click "Create repository from template"

**GitHub CLI:**
```bash
gh repo create my-cli-app --template sammyjoyce/c23-cli-template --clone
cd my-cli-app
```

### 2. Wait for Automatic Setup

GitHub Actions will automatically:
- Replace all placeholders with your repository info
- Remove template-specific files
- Update the README
- Commit the changes

Check the "Actions" tab to monitor progress.

### 3. Start Development

```bash
# Install Zig (if needed)
curl -sSL https://raw.githubusercontent.com/tristanisham/zvm/master/install.sh | bash
zvm install master

# Build your application
zig build

# Run tests
zig build test

# Run your app
./zig-out/bin/your-app --help
```

## Manual Setup (if automatic setup fails)

If the GitHub Action doesn't run:

```bash
# Run the cleanup script manually
chmod +x .template/cleanup-template.sh
.template/cleanup-template.sh
```

## Project Structure

```
├── src/           # Source code
│   ├── cli/       # Command-line interface
│   ├── core/      # Core functionality
│   ├── io/        # Input/output handling
│   ├── tui/       # Terminal UI (optional)
│   └── utils/     # Utilities
├── test/          # Tests
├── docs/          # Documentation
└── examples/      # Example code
```

## Customization

### Key Files to Update

After automatic setup, customize these files for your project:

1. **README.md** - Update with your project details
2. **CONTRIBUTING.md** - Adjust contribution guidelines
3. **.github/CODEOWNERS** - Set code ownership
4. **build.zig** - Configure build settings
5. **opencli.json** - Update CLI metadata

### Development Features

- **GitHub Codespaces**: Click "Code" → "Create codespace on main"
- **VS Code**: Pre-configured settings and extensions
- **Dev Container**: Consistent development environment

## Keeping Updated

The template includes automatic synchronization:

1. Weekly checks for template updates
2. Creates PRs with changes
3. Your customizations are preserved

To disable: Remove `.github/workflows/template-sync.yml`

## Troubleshooting

**Build fails?**
- Ensure Zig master is installed: `zvm install master`
- Check for ncurses: `apt install libncurses-dev` or `brew install ncurses`

**Template cleanup didn't run?**
- Check Actions tab for errors
- Run manually: `.template/cleanup-template.sh`

**Need help?**
- Template issues: [Create an issue](https://github.com/sammyjoyce/c23-cli-template/issues)
- Your project issues: Use your own repository's issues