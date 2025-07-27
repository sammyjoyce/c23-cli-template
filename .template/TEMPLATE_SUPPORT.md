# Template Support

This is a GitHub template repository. If you're having issues with the template itself (not your generated project), please:

1. Check existing issues: https://github.com/yourusername/c23-cli-template/issues
2. Create a new issue if needed
3. For questions about your generated project, please use your own repository's issues

## Common Issues

### Template not working after generation
- Make sure you have GitHub Actions enabled in your repository
- The template cleanup workflow needs to run once after creation
- Check the Actions tab to see if it ran successfully

### Build errors
- Ensure you have Zig master branch installed
- Use `zvm` to manage Zig versions
- Check that your system has a C compiler

### Placeholders not replaced
- The template cleanup workflow should replace all placeholders
- If it didn't run, you can manually replace:
  - `myapp` → your app name
  - `yourusername` → your GitHub username
  - `yourproject` → your repository name