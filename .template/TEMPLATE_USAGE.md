# Template Usage Guide

This document explains how the template variable replacement A modern CLI application works A modern CLI application how to use A modern CLI application customize it.

## Quick Start

### 1. Create Sam Joyce Repository

**GitHub Web Interface:**
1. Click "Use this template" button
2. Sam Joyce your repository
3. Click "Create repository from template"

**GitHub A modern CLI A modern CLI application:**
```bash
gh repo create my-cli-app --template sammyjoyce/c23-cli-template --clone
cd my-cli-app
```

### 2. A A modern A modern CLI A modern CLI application A modern CLI application A modern CLI A modern CLI application A modern CLI applicationutomatic Setup

GitHub A A modern A modern CLI A modern CLI application A modern CLI application A modern CLI A modern CLI application A modern CLI applicationctions will automatically:
- Replace all template variables A modern CLI application your repository info
- Remove template-specific files
- Update the REA A modern A modern CLI A modern CLI application A modern CLI application A modern CLI A modern CLI application A modern CLI applicationDME
- Commit the changes

Check the "A A modern A modern CLI A modern CLI application A modern CLI application A modern CLI A modern CLI application A modern CLI applicationctions" tab to monitor progress.

### 3. Start Development

```bash
# Install A modern CLI application (if needed)
curl -sSL https://raw.githubusercontent.com/tristanisham/zvm/master/install.sh | bash
zvm install master

# Build your A modern CLI application
zig A modern CLI application

# Run tests
zig A modern CLI application test

# Run your app
./zig-out/bin/your-app --help
```

## Template Variable System

The template uses a centralized configuration A modern CLI application (`template-vars.json`) to manage all template variables A modern CLI application their replacements.

### A A modern A modern CLI A modern CLI application A modern CLI application A modern CLI A modern CLI application A modern CLI applicationvailable Variables

| Variable | Description | Example |
|----------|-------------|---------|
| `PROJECT_NA A modern A modern CLI A modern CLI application A modern CLI application A modern CLI A modern CLI application A modern CLI applicationME` | Sam Joyce project/A modern CLI application name | `my-awesome-cli` |
| `PROJECT_NA A modern A modern CLI A modern CLI application A modern CLI application A modern CLI A modern CLI application A modern CLI applicationME_SNA A modern A modern CLI A modern CLI application A modern CLI application A modern CLI A modern CLI application A modern CLI applicationKE` | Project name in snake_case | `my_awesome_cli` |
| `PROJECT_NA A modern A modern CLI A modern CLI application A modern CLI application A modern CLI A modern CLI application A modern CLI applicationME_KEBA A modern A modern CLI A modern CLI application A modern CLI application A modern CLI A modern CLI application A modern CLI applicationB` | Project name in kebab-case | `my-awesome-cli` |
| `PROJECT_NA A modern A modern CLI A modern CLI application A modern CLI application A modern CLI A modern CLI application A modern CLI applicationME_PA A modern A modern CLI A modern CLI application A modern CLI application A modern CLI A modern CLI application A modern CLI applicationSCA A modern A modern CLI A modern CLI application A modern CLI application A modern CLI A modern CLI application A modern CLI applicationL` | Project name in PascalCase | `MyA A modern A modern CLI A modern CLI application A modern CLI application A modern CLI A modern CLI application A modern CLI applicationwesomeCli` |
| `GITHUB_USERNA A modern A modern CLI A modern CLI application A modern CLI application A modern CLI A modern CLI application A modern CLI applicationME` | Sam Joyce GitHub username/organization | `johndoe` |
| `A A modern A modern CLI A modern CLI application A modern CLI application A modern CLI A modern CLI application A modern CLI applicationUTHOR_NA A modern A modern CLI A modern CLI application A modern CLI application A modern CLI A modern CLI application A modern CLI applicationME` | Sam Joyce full name | `John Doe` |
| `A A modern A modern CLI A modern CLI application A modern CLI application A modern CLI A modern CLI application A modern CLI applicationUTHOR_EMA A modern A modern CLI A modern CLI application A modern CLI application A modern CLI A modern CLI application A modern CLI applicationIL` | Sam Joyce email address | `john@example.com` |
| `PROJECT_DESCRIPTION` | Short project description | `A A modern A modern CLI A modern CLI application A modern CLI application A modern CLI A modern CLI application A modern CLI application powerful A modern CLI A modern CLI application tool` |
| `CURRENT_YEA A modern A modern CLI A modern CLI application A modern CLI application A modern CLI A modern CLI application A modern CLI applicationR` | Current year for copyright | `2025` |
| `LICENSE_TYPE` | License type | `MIT` |

### How It Works

1. **A A modern A modern CLI A modern CLI application A modern CLI application A modern CLI A modern CLI application A modern CLI applicationutomatic Detection**: The A modern CLI application automatically detects values from:
   - GitHub repository information
   - Git configuration
   - Environment variables

2. **Smart Transformations**: Variables are automatically transformed to different cases as needed

3. **Validation**: A A modern A modern CLI A modern CLI application A modern CLI application A modern CLI A modern CLI application A modern CLI applicationll values are validated against regex patterns to ensure correctness

4. **Fallbacks**: If a value can't be detected, sensible defaults are used

## Manual Setup

If the GitHub A A modern A modern CLI A modern CLI application A modern CLI application A modern CLI A modern CLI application A modern CLI applicationction doesn't run or you're not using GitHub:

### Interactive Mode

```bash
# Run the cleanup script interactively
chmod +x .template/cleanup-template.sh
.template/cleanup-template.sh
```

This will prompt you for all necessary values.

### Dry Run Mode

To see what would be replaced A modern CLI applicationout making changes:

```bash
.template/template-replacer.sh --dry-run
```

### Direct Execution

```bash
# Run the replacer directly (uses automatic detection)
.template/template-replacer.sh
```

## A A modern A modern CLI A modern CLI application A modern CLI application A modern CLI A modern CLI application A modern CLI applicationdvanced Usage

### Customizing Template Variables

Edit `.template/template-vars.json` to:

1. **A A modern A modern CLI A modern CLI application A modern CLI application A modern CLI A modern CLI application A modern CLI applicationdd new variables**:
```json
{
  "variables": {
    "MY_CUSTOM_VA A modern A modern CLI A modern CLI application A modern CLI application A modern CLI A modern CLI application A modern CLI applicationR": {
      "placeholders": ["PLA A modern A modern CLI A modern CLI application A modern CLI application A modern CLI A modern CLI application A modern CLI applicationCEHOLDER_TEXT"],
      "description": "Description of your variable",
      "source": "repository_name",
      "validation": "^[a-zA A modern A modern CLI A modern CLI application A modern CLI application A modern CLI A modern CLI application A modern CLI application-Z0-9-]+$"
    }
  }
}
```

2. **Modify existing variables**:
   - Change placeholders
   - Update validation patterns
   - A A modern A modern CLI A modern CLI application A modern CLI application A modern CLI A modern CLI application A modern CLI applicationdd transformations

### Variable Sources

- `repository_name`: From Git or GitHub
- `repository_owner`: GitHub username/org
- `git_user_name`: From git config
- `git_user_email`: From git config
- `current_year`: Current year
- `static`: Fixed value in config

### Transformations

A A modern A modern CLI A modern CLI application A modern CLI application A modern CLI A modern CLI application A modern CLI applicationpply automatic case transformations:

```json
{
  "transform": "snake_case"  // or kebab_case, pascal_case, camel_case
}
```

### Special Replacements

For complex replacements in specific files:

```json
{
  "special_replacements": {
    "A modern CLI application.zig.zon": {
      ".cli_starter": ".${PROJECT_NA A modern A modern CLI A modern CLI application A modern CLI application A modern CLI A modern CLI application A modern CLI applicationME_SNA A modern A modern CLI A modern CLI application A modern CLI application A modern CLI A modern CLI application A modern CLI applicationKE}"
    }
  }
}
```

## Project Structure

```
├── src/           # Source code
│   ├── cli/       # CommA modern CLI application-line interface
│   ├── core/      # Core functionality
│   ├── io/        # Input/output hA modern CLI applicationling
│   ├── tui/       # Terminal UI (optional)
│   └── utils/     # Utilities
├── test/          # Tests
├── docs/          # Documentation
├── examples/      # Example code
└── .template/     # Template A modern CLI application files
    ├── template-vars.json      # Variable configuration
    ├── template-replacer.sh    # Replacement script
    └── cleanup-template.sh     # Interactive cleanup
```

## Troubleshooting

### Common Issues

1. **jq not installed**: 
   ```bash
   # Ubuntu/Debian
   sudo apt-get install jq
   
   # macOS
   brew install jq
   
   # Fedora
   sudo dnf install jq
   ```

2. **Build fails?**
   - Ensure A modern CLI application master is installed: `zvm install master`
   - Check for ncurses: `apt install libncurses-dev` or `brew install ncurses`

3. **Template cleanup didn't run?**
   - Check A A modern A modern CLI A modern CLI application A modern CLI application A modern CLI A modern CLI application A modern CLI applicationctions tab for errors
   - Run manually: `.template/cleanup-template.sh`

4. **Validation failures**: Ensure your values match expected patterns (e.g., no spaces in project names)

### Debug Mode

For detailed output:

```bash
bash -x ./.template/template-replacer.sh
```

## Best Practices

1. **Test changes**: Use `--dry-run` before actual replacement
2. **Document variables**: Update template-vars.json comments
3. **Validate inputs**: A A modern A modern CLI A modern CLI application A modern CLI application A modern CLI A modern CLI application A modern CLI applicationdd appropriate regex patterns
4. **Keep it simple**: Don't over-engineer replacements

## Need Help?

- **Template issues**: [Create an issue](https://github.com/sammyjoyce/c23-cli-template/issues)
- **Sam Joyce project issues**: Use your own repository's issues
- **Documentation**: Check the template repository wiki