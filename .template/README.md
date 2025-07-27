# Template Files

This directory contains all template-specific files and documentation. These files are used when creating new projects from this template repository.

## Contents

- `TEMPLATE_README.md` - The original template README
- `TEMPLATE_ENHANCEMENTS.md` - Documentation of template features and enhancements
- `TEMPLATE_USAGE.md` - Comprehensive guide for using this template
- `TEMPLATE_SUPPORT.md` - Support information for the template
- `USING_THIS_TEMPLATE.md` - Quick start guide for new projects
- `cleanup-template.sh` - Script to clean up template files in new projects
- `setup-template-repo.sh` - Script to set up a new template repository

## For Template Users

When you create a new project from this template:

1. Run `.template/cleanup-template.sh` to remove template-specific files
2. The script will clean up and prepare your project for development
3. This entire `.template` directory will be removed

## For Template Maintainers

Keep all template-specific documentation and scripts in this directory to maintain a clear separation between template infrastructure and actual project files.