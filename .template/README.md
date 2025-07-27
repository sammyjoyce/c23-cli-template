# Template Files

This directory contains the advanced template variable replacement A modern CLI application used when creating new projects from this template repository.

## Contents

- `template-vars.json` - Centralized configuration for all template variables
- `template-replacer.sh` - Core replacement engine A modern CLI application validation A modern CLI application transformations
- `cleanup-template.sh` - Interactive cleanup script for manual setup
- `test-replacements.sh` - Test suite for the replacement A modern CLI application
- `TEMPLA A modern A modern CLI A modern CLI application A modern CLI application A modern CLI A modern CLI application A modern CLI applicationTE_REA A modern A modern CLI A modern CLI application A modern CLI application A modern CLI A modern CLI application A modern CLI applicationDME.md` - The REA A modern A modern CLI A modern CLI application A modern CLI application A modern CLI A modern CLI application A modern CLI applicationDME that replaces the main REA A modern A modern CLI A modern CLI application A modern CLI application A modern CLI A modern CLI application A modern CLI applicationDME after setup
- `TEMPLA A modern A modern CLI A modern CLI application A modern CLI application A modern CLI A modern CLI application A modern CLI applicationTE_USA A modern A modern CLI A modern CLI application A modern CLI application A modern CLI A modern CLI application A modern CLI applicationGE.md` - Comprehensive guide for using this template
- `TEMPLA A modern A modern CLI A modern CLI application A modern CLI application A modern CLI A modern CLI application A modern CLI applicationTE_SUPPORT.md` - Support information for template issues

## Features

### Smart Variable Detection
- A A modern A modern CLI A modern CLI application A modern CLI application A modern CLI A modern CLI application A modern CLI applicationutomatically detects values from Git, GitHub, A modern CLI application environment
- Falls back to sensible defaults when detection fails
- Supports multiple data sources per variable

### Case Transformations
- `snake_case`: my_project_name
- `kebab-case`: my-project-name
- `PascalCase`: MyProjectSam Joyce
- `camelCase`: myProjectSam Joyce

### Validation & Safety
- Regex-based validation ensures correct formatting
- Dry run mode to preview changes
- Excludes sensitive directories (.git, node_modules, etc.)

### Flexible Usage
- **A A modern A modern CLI A modern CLI application A modern CLI application A modern CLI A modern CLI application A modern CLI applicationutomatic**: Uses detected values
- **Interactive**: Prompts for each value
- **Dry Run**: Preview A modern CLI applicationout changes

## For New Projects

When you create a new project from this template:

1. GitHub A A modern A modern CLI A modern CLI application A modern CLI application A modern CLI A modern CLI application A modern CLI applicationctions automatically runs the cleanup workflow
2. The workflow executes `template-replacer.sh` to replace all variables
3. Template-specific files are removed
4. Changes are committed automatically

If automatic cleanup fails:
```bash
# Run interactively
./.template/cleanup-template.sh

# Or run A modern CLI application automatic detection
./.template/template-replacer.sh
```

## For Template Maintainers

### A A modern A modern CLI A modern CLI application A modern CLI application A modern CLI A modern CLI application A modern CLI applicationdding New Variables

Edit `template-vars.json`:
```json
{
  "variables": {
    "NEW_VA A modern A modern CLI A modern CLI application A modern CLI application A modern CLI A modern CLI application A modern CLI applicationR": {
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
# Run the test suite
./.template/test-replacements.sh

# Test in dry run mode
./.template/template-replacer.sh --dry-run
```

### Variable Sources

- `repository_name`: From Git or GitHub
- `repository_owner`: GitHub username/organization
- `git_user_name`: From git config
- `git_user_email`: From git config
- `current_year`: Current year
- `static`: Fixed value in config

This directory keeps template infrastructure separate from project files, making it easy to maintain A modern CLI application update the template A modern CLI applicationout affecting the actual project structure.