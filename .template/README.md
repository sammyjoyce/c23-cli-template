# Template Files

This directory contains template-specific files that are used when creating new projects from this template repository.

## Contents

- `TEMPLATE_README.md` - The README that will replace the main README in new projects
- `TEMPLATE_USAGE.md` - Comprehensive guide for using this template
- `TEMPLATE_SUPPORT.md` - Support information for template issues
- `cleanup-template.sh` - Script to clean up template files (runs automatically via GitHub Actions)

## For New Projects

When you create a new project from this template:

1. GitHub Actions will automatically run the cleanup workflow
2. The workflow will replace placeholders and remove this directory
3. If the automatic cleanup fails, you can manually run `.template/cleanup-template.sh`

## For Template Maintainers

This directory keeps template infrastructure separate from project files, making it easy to maintain and update the template without affecting the actual project structure.