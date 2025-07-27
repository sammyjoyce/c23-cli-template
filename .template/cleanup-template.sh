#!/bin/bash
# Manual template cleanup script
# Use this if the GitHub Action doesn't run or you're not using GitHub

set -e

# Script directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

echo "🧹 CLI Template Cleanup Script"
echo "=============================="
echo ""

# Check if template-replacer.sh exists
if [ ! -f "${SCRIPT_DIR}/template-replacer.sh" ]; then
    echo "Error: template-replacer.sh not found in ${SCRIPT_DIR}"
    exit 1
fi

# Make template-replacer.sh executable
chmod +x "${SCRIPT_DIR}/template-replacer.sh"

# Run the template replacer in interactive mode
"${SCRIPT_DIR}/template-replacer.sh" --interactive

echo "📁 Cleaning up template files..."

# Move template README if it exists
if [ -f ".template/TEMPLATE_README.md" ]; then
    mv .template/TEMPLATE_README.md README.md
    echo "  ✓ Moved TEMPLATE_README.md to README.md"
fi

# Remove template-specific files
rm -f .github/workflows/template-cleanup.yml
rm -f .github/template.yml
rm -rf .template

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