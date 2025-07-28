#!/bin/bash
set -euo pipefail

# ====================================================================================
#
#          FILE: setup.sh
#
#         USAGE: bash ./.template/setup.sh
#
#   DESCRIPTION: An interactive script to set up the project locally. It uses 'gum'
#                to prompt for values, then calls the non-interactive replacer script.
#
#        AUTHOR: Sammy Joyce
#       CREATED: 2025-07-28
#
# ====================================================================================

# --- Helper Functions for Styled Output ---
info() {
    # Using gum for consistent styling, but checking if it exists first.
    if command -v gum &> /dev/null; then
        gum style --foreground 212 "ℹ $1"
    else
        echo "INFO: $1"
    fi
}

error() {
    if command -v gum &> /dev/null; then
        gum style --foreground 9 "✖ $1" >&2
    else
        echo "ERROR: $1" >&2
    fi
    exit 1
}

# --- Dependency Check ---
# Ensure all required command-line tools are available.
check_dependency() {
    if ! command -v "$1" &> /dev/null; then
        error "'$1' is not installed. Please install it to continue."
    fi
}

check_dependency "gum"
check_dependency "sd"
check_dependency "jq"
check_dependency "bash"

# --- Get Default Values from JSON ---
VARS_JSON=".template/template-vars.json"
if [ ! -f "$VARS_JSON" ]; then
    error "Could not find template variables file at '$VARS_JSON'"
fi

get_default() {
    jq -r ".$1" "$VARS_JSON"
}

# --- User Prompts ---
# Use 'gum' to create a visually appealing and interactive setup process.
gum style --border normal --padding "1 2" --border-foreground 212 "C23 CLI Template Setup"

export PROJECT_NAME
PROJECT_NAME=$(gum input --value "$(get_default '__PROJECT_NAME__')" --placeholder "Enter the project name")
export PROJECT_DESCRIPTION
PROJECT_DESCRIPTION=$(gum input --value "$(get_default '__PROJECT_DESCRIPTION__')" --placeholder "Enter the project description")
export PROJECT_URL
PROJECT_URL=$(gum input --value "$(get_default '__PROJECT_URL__')" --placeholder "Enter the project URL")
export PROJECT_AUTHOR
PROJECT_AUTHOR=$(gum input --value "$(get_default '__PROJECT_AUTHOR__')" --placeholder "Enter the author's name")
export AUTHOR_EMAIL
AUTHOR_EMAIL=$(gum input --value "$(get_default '__AUTHOR_EMAIL__')" --placeholder "Enter the author's email")
export GITHUB_USERNAME
GITHUB_USERNAME=$(gum input --value "$(get_default '__GITHUB_USERNAME__')" --placeholder "Enter the GitHub username")
export PROJECT_LICENSE
PROJECT_LICENSE=$(gum input --value "$(get_default '__PROJECT_LICENSE__')" --placeholder "Enter the license (e.g., MIT)")

# --- Run Replacer Script ---
info "Collected all values. Running the replacement script..."
# Calls the non-interactive script with the exported variables.
bash ./.template/replacer.sh

# --- Cleanup ---
info "Replacement complete."
if gum confirm "Do you want to clean up the template files now? (This will delete this setup script)"; then
    info "Cleaning up template files..."
    bash ./.template/setup.sh
    info "Cleanup complete. You are ready to go!"
else
    info "Cleanup skipped. You can run 'bash ./.template/setup.sh' later."
fi
