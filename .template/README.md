# Template Files

This directory contains the template variable replacement system used when creating new projects from this template repository.

## Contents

- `template-vars.json` - Centralized configuration for all template variables
- `replacer.sh` - Core replacement engine for automatic variable replacement and transformations
- `setup.sh` - Interactive setup script for manual configuration
- `TEMPLATE_README.md` - The README that replaces the main README after setup
- `TEMPLATE_USAGE.md` - Comprehensive guide for using this template
- `TEMPLATE_SUPPORT.md` - Support information for template issues
- `README.md` - This documentation file

## Features

### Smart Variable Detection

- Automatically detects values from Git, GitHub, and environment
- Falls back to sensible defaults when detection fails
- Supports multiple data sources per variable

### Case Transformations

- `snake_case`: my_project_name
- `kebab-case`: my-project-name
- `PascalCase`: MyProjectName
- `camelCase`: myProjectName

### Validation & Safety

- Regex-based validation ensures correct formatting
- Dry run mode to preview changes
- Excludes sensitive directories (.git, node_modules, etc.)

### Flexible Usage

- **Automatic**: Uses detected values
- **Interactive**: Prompts for each value
- **Dry Run**: Preview changes without making them

## For New Projects

When you create a new project from this template:

1. GitHub Actions automatically runs the template cleanup workflow
2. The workflow executes `replacer.sh` to replace all variables
3. Template-specific files are removed
4. Changes are committed automatically

If automatic cleanup fails:

```bash
# Run interactively
./.template/setup.sh

# Or run with automatic detection
./.template/replacer.sh
```

## For Template Maintainers

### Adding New Variables

Edit `template-vars.json`:

```json
{
  "variables": {
    "NEW_VAR": {
      "placeholders": ["OLD_TEXT"],
      "description": "What this variable represents",
      "source": "repository_name",
      "transform": "kebab_case",
      "validation": "^[a-z][a-z0-9-]*$"
    }
  }
}
```

### Testing Changes

```bash
# Test in dry run mode
./.template/replacer.sh --dry-run
```

### Variable Sources

- `repository_name`: From Git or GitHub
- `repository_owner`: GitHub username/organization
- `git_user_name`: From git config
- `git_user_email`: From git config
- `current_year`: Current year
- `static`: Fixed value in config

This directory keeps template infrastructure separate from project files, making it easy to maintain and update the template without affecting the actual project structure.
