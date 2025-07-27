# Template Usage Guide

This document provides comprehensive guidance on using this GitHub template repository effectively.

## Table of Contents

1. [Getting Started](#getting-started)
2. [Creating a New Project](#creating-a-new-project)
3. [Initial Setup](#initial-setup)
4. [Customization](#customization)
5. [Keeping Your Project Updated](#keeping-your-project-updated)
6. [Best Practices](#best-practices)

## Getting Started

This template provides a complete foundation for C/Zig projects with:
- Modern build system using Zig
- Pre-configured CI/CD with GitHub Actions
- Development container support
- Comprehensive documentation structure
- Security and dependency management

## Creating a New Project

### Method 1: GitHub Web Interface

1. Navigate to this template repository
2. Click the green "Use this template" button
3. Select "Create a new repository"
4. Fill in:
   - Owner (your username or organization)
   - Repository name
   - Description (optional but recommended)
   - Visibility (Public/Private)
5. Choose whether to include all branches
6. Click "Create repository from template"

### Method 2: GitHub CLI

```bash
# Create a new repository from this template
gh repo create my-new-project \
  --template yourusername/yourtemplaterepo \
  --private \
  --clone \
  --description "My awesome new project"

# Navigate to your new project
cd my-new-project
```

### Method 3: Direct URL

Use this URL format to pre-fill the creation form:
```
https://github.com/yourusername/yourtemplaterepo/generate
```

## Initial Setup

After creating your repository from the template:

### 1. Run the Template Cleanup Script

```bash
# Make the script executable
chmod +x cleanup-template.sh

# Run the cleanup script
./cleanup-template.sh
```

This script will:
- Update placeholder values in documentation
- Remove template-specific files
- Prepare your project for development

### 2. Update Project Information

Replace the following placeholders throughout the project:
- `yourusername` → Your GitHub username
- `yourrepo` → Your repository name
- `yourorganization` → Your organization name
- `your-app` → Your application name
- Update the LICENSE file with your information
- Update security contact in SECURITY.md

### 3. Configure GitHub Repository Settings

1. **Enable GitHub Pages** (if needed):
   - Settings → Pages → Source: Deploy from a branch
   - Branch: main, folder: /docs

2. **Set up Branch Protection**:
   - Settings → Branches → Add rule
   - Branch name pattern: `main`
   - Enable:
     - Require pull request reviews
     - Require status checks to pass
     - Require branches to be up to date
     - Include administrators

3. **Configure Security**:
   - Settings → Security → Enable:
     - Dependency graph
     - Dependabot alerts
     - Dependabot security updates

4. **Set up Secrets** (if needed):
   - Settings → Secrets → Actions
   - Add any required secrets for CI/CD

## Customization

### Project Structure

```
.
├── .devcontainer/      # Development container configuration
├── .github/            # GitHub-specific files
│   ├── ISSUE_TEMPLATE/ # Issue templates
│   ├── workflows/      # GitHub Actions workflows
│   └── ...            # Other GitHub configs
├── .vscode/           # VS Code workspace settings
├── docs/              # Project documentation
├── examples/          # Example code and usage
├── src/               # Source code
├── test/              # Test files
└── ...                # Other project files
```

### Key Files to Customize

1. **README.md**
   - Update project name and description
   - Modify feature list
   - Update installation instructions
   - Add project-specific documentation

2. **CONTRIBUTING.md**
   - Adjust contribution guidelines
   - Update development setup instructions
   - Modify code style requirements

3. **.github/CODEOWNERS**
   - Update with your team structure
   - Define ownership for different parts of the codebase

4. **opencli.json** (if applicable)
   - Update application metadata
   - Configure CLI behavior

5. **build.zig**
   - Adjust build configuration
   - Add/remove dependencies
   - Configure target platforms

### Development Environment

#### VS Code / Codespaces

The template includes configurations for:
- Recommended extensions (.vscode/extensions.json)
- Workspace settings (.vscode/settings.json)
- Development container (.devcontainer/)

To use GitHub Codespaces:
1. Click "Code" → "Codespaces" → "Create codespace on main"
2. Wait for the environment to build
3. Start coding!

#### Local Development

1. Install prerequisites:
   - Zig (latest version)
   - C compiler (gcc/clang)
   - Git

2. Clone your repository:
   ```bash
   git clone https://github.com/yourusername/yourrepo.git
   cd yourrepo
   ```

3. Build the project:
   ```bash
   zig build
   ```

4. Run tests:
   ```bash
   zig build test
   ```

## Keeping Your Project Updated

### Automated Template Sync

This template includes a GitHub Action that periodically checks for updates from the template repository.

To enable it:
1. Update `.github/workflows/template-sync.yml`:
   - Change `source_repo_path` to point to this template
   - Adjust `pr_reviewers` to your username

2. The workflow will:
   - Run weekly (configurable)
   - Create a PR with template updates
   - Exclude your custom code from updates

### Manual Template Updates

To manually sync with the template:

```bash
# Add the template as a remote
git remote add template https://github.com/yourusername/yourtemplaterepo.git

# Fetch the latest changes
git fetch template

# Merge template updates
git merge template/main --allow-unrelated-histories

# Resolve any conflicts and commit
```

### Selective Updates

To update specific files from the template:

```bash
# Fetch latest template
git fetch template

# Cherry-pick specific files
git checkout template/main -- .github/workflows/ci.yml
git checkout template/main -- .devcontainer/devcontainer.json

# Commit the updates
git commit -m "Update CI workflow and devcontainer from template"
```

## Best Practices

### 1. Maintain Template Connection

- Keep the template sync workflow enabled
- Regularly review and merge template updates
- Document any significant deviations from the template

### 2. Documentation

- Keep README.md up to date
- Document all custom modifications
- Maintain CHANGELOG.md for version history

### 3. Dependencies

- Let Dependabot manage dependency updates
- Review and test all dependency updates
- Keep dependencies minimal

### 4. Security

- Never commit secrets or credentials
- Use GitHub Secrets for sensitive data
- Keep security policies updated
- Respond promptly to security alerts

### 5. CI/CD

- Ensure all tests pass before merging
- Keep CI workflows fast and efficient
- Use caching to speed up builds

### 6. Contribution

- Follow the established code style
- Write tests for new features
- Update documentation with changes
- Use conventional commits

## Troubleshooting

### Common Issues

1. **Build Failures**
   - Ensure Zig is installed and up to date
   - Check for missing dependencies
   - Review build.zig for configuration issues

2. **Template Sync Conflicts**
   - Manually resolve conflicts in PRs
   - Consider excluding more files if needed
   - Document why files are excluded

3. **CI/CD Issues**
   - Check GitHub Actions logs
   - Verify all secrets are set
   - Ensure branch protection rules allow CI

### Getting Help

- Check existing issues in the template repository
- Review the template's documentation
- Ask questions in discussions
- Report bugs using issue templates

## Contributing Back

If you discover improvements that would benefit the template:

1. Fork the template repository
2. Make your improvements
3. Submit a pull request
4. Share your use case

Your contributions help make the template better for everyone!

---

Remember: This template is a starting point. Feel free to modify, extend, and adapt it to meet your specific needs. The goal is to provide a solid foundation that accelerates your development while maintaining best practices.