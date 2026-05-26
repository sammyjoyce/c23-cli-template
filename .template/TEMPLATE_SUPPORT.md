# Template Support

For issues with the template itself (not your generated project):

- **Check existing issues**: [c23-cli-template/issues](https://github.com/sammyjoyce/c23-cli-template/issues)
- **Create new issue**: Use the issue templates provided
- **Your project issues**: Use your own repository's issue tracker

## Quick Fixes

**Template cleanup didn't run?**

- Check Actions tab for the workflow status
- Run manually: `.template/setup.sh`
- Run without prompts: `.template/setup.sh --non-interactive --cleanup`

**Placeholders still visible?**

The cleanup script replaces:

- `myapp` to your repository name
- `yourusername` to your GitHub username
- `sammyjoyce` to your GitHub username
