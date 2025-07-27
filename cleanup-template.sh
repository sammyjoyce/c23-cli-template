#!/bin/bash
# Manual template cleanup script
# Use this if the GitHub Action doesn't run or you're not using GitHub

set -e

echo "🧹 CLI Template Cleanup Script"
echo "=============================="
echo ""

# Get repository name from current directory or ask user
if [ -d .git ]; then
    REPO_NAME=$(basename `git rev-parse --show-toplevel`)
else
    read -p "Enter your application name: " REPO_NAME
fi

read -p "Enter your GitHub username (or organization): " REPO_OWNER
read -p "Enter your full name: " AUTHOR_NAME

echo ""
echo "📝 Configuration:"
echo "  App name: $REPO_NAME"
echo "  Owner: $REPO_OWNER"
echo "  Author: $AUTHOR_NAME"
echo ""
read -p "Continue? (y/n) " -n 1 -r
echo ""

if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo "Cancelled."
    exit 1
fi

echo "🔄 Replacing placeholders..."

# Function to replace in all relevant files
replace_all() {
    find . -type f \( -name "*.md" -o -name "*.c" -o -name "*.h" -o -name "*.zig" -o -name "*.json" -o -name "*.yaml" -o -name "*.yml" \) \
        -not -path "./.git/*" \
        -not -path "./zig-out/*" \
        -not -path "./.zig-cache/*" \
        -exec sed -i.bak "$1" {} \;
    
    # Clean up backup files
    find . -name "*.bak" -type f -delete
}

# Replace placeholders
replace_all "s/myapp/${REPO_NAME}/g"
replace_all "s/cli_starter/${REPO_NAME}/g"
replace_all "s/cli-starter/${REPO_NAME}/g"
replace_all "s/cli-starter-c23/${REPO_NAME}/g"
replace_all "s/c23-cli-template/${REPO_NAME}/g"
replace_all "s/yourusername/${REPO_OWNER}/g"
replace_all "s/yourproject/${REPO_NAME}/g"
replace_all "s/Your Name/${AUTHOR_NAME}/g"
replace_all "s/\[Your Name\]/${AUTHOR_NAME}/g"

# Update build.zig.zon
sed -i.bak "s/\.cli_starter/.${REPO_NAME}/g" build.zig.zon
rm -f build.zig.zon.bak

echo "📁 Cleaning up template files..."

# Move template README if it exists
if [ -f "TEMPLATE_README.md" ]; then
    mv TEMPLATE_README.md README.md
    echo "  ✓ Moved TEMPLATE_README.md to README.md"
fi

# Remove template-specific files
rm -f .github/workflows/template-cleanup.yml
rm -f .github/TEMPLATE_SUPPORT.md
rm -f .github/template.yml
rm -f USING_THIS_TEMPLATE.md
rm -f TEMPLATE_ENHANCEMENTS.md
rm -f TEMPLATE_README.md
rm -f cleanup-template.sh

echo "  ✓ Removed template-specific files"

echo ""
echo "✅ Template cleanup complete!"
echo ""
echo "Next steps:"
echo "1. Review the changes"
echo "2. Build your application: zig build"
echo "3. Run tests: zig build test"
echo "4. Start developing your CLI app!"
echo ""
echo "Happy coding! 🚀"