#!/bin/bash
# Script to configure GitHub repository as a template

set -e

echo "🚀 C23 CLI Template Repository Setup"
echo "========================================"
echo ""

# Check if gh CLI is installed
if ! command -v gh &> /dev/null; then
    echo "❌ GitHub CLI (gh) is not installed."
    echo "   Please install it from: https://cli.github.com/"
    exit 1
fi

# Check if we're in a git repository
if [ ! -d .git ]; then
    echo "❌ Not in a git repository."
    echo "   Please run this from the repository root."
    exit 1
fi

# Get repository info
REPO_NAME=$(basename `git rev-parse --show-toplevel`)
REPO_OWNER=$(git remote get-url origin | sed -E 's/.*[:/]([^/]+)\/[^/]+\.git/\1/')
REPO_FULL="$REPO_OWNER/$REPO_NAME"

echo "📦 Repository: $REPO_FULL"
echo ""

# Check if authenticated
if ! gh auth status &> /dev/null; then
    echo "🔐 Please authenticate with GitHub CLI:"
    gh auth login
fi

echo "🔧 Configuring repository settings..."

# Mark as template repository
echo "   ✓ Marking as template repository"
gh repo edit "$REPO_FULL" --template

# Set description
echo "   ✓ Setting description"
gh repo edit "$REPO_FULL" --description "Modern C23 CLI application starter with Zig build system, NCurses TUI support, and comprehensive project structure"

# Add topics
echo "   ✓ Adding topics"
gh repo edit "$REPO_FULL" --add-topic "cli,c,c23,zig,template,starter-template,command-line,cli-app,ncurses,tui,zig-build"

# Enable features
echo "   ✓ Enabling repository features"
gh repo edit "$REPO_FULL" --enable-issues --enable-wiki

echo ""
echo "✅ Template repository setup complete!"
echo ""
echo "Next steps:"
echo "1. Push all changes: git push"
echo "2. Visit: https://github.com/$REPO_FULL"
echo "3. Test the template by clicking 'Use this template'"
echo "4. Update the README.md with your actual GitHub username"
echo ""
echo "To use this template:"
echo "  - Via UI: https://github.com/$REPO_FULL/generate"
echo "  - Via CLI: gh repo create my-app --template $REPO_FULL"