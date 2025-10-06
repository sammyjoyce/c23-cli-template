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
    local key="$1"
    jq -r --arg key "$key" '
        (.variables[$key] // {}) as $var |
        ($var.fallback // ($var.placeholders[0] // ""))
    ' "$VARS_JSON"
}

get_placeholder() {
    local key="$1"
    local description
    description=$(jq -r --arg key "$key" '.variables[$key].description // empty' "$VARS_JSON")
    if [[ -n $description ]]; then
        printf '%s' "$description"
    else
        printf 'Enter %s' "$key"
    fi
}

# --- User Prompts ---
# Use 'gum' to create a visually appealing and interactive setup process.
gum style --border normal --padding "1 2" --border-foreground 212 "C23 CLI Template Setup"

export PROJECT_NAME
PROJECT_NAME=$(gum input --value "$(get_default 'PROJECT_NAME')" --placeholder "$(get_placeholder 'PROJECT_NAME')")
export PROJECT_DESCRIPTION
PROJECT_DESCRIPTION=$(gum input --value "$(get_default 'PROJECT_DESCRIPTION')" --placeholder "$(get_placeholder 'PROJECT_DESCRIPTION')")
export AUTHOR_NAME
AUTHOR_NAME=$(gum input --value "$(get_default 'AUTHOR_NAME')" --placeholder "$(get_placeholder 'AUTHOR_NAME')")
export AUTHOR_EMAIL
AUTHOR_EMAIL=$(gum input --value "$(get_default 'AUTHOR_EMAIL')" --placeholder "$(get_placeholder 'AUTHOR_EMAIL')")
export GITHUB_USERNAME
GITHUB_USERNAME=$(gum input --value "$(get_default 'GITHUB_USERNAME')" --placeholder "$(get_placeholder 'GITHUB_USERNAME')")
export PROJECT_LICENSE
PROJECT_LICENSE=$(gum input --value "$(get_default 'PROJECT_LICENSE')" --placeholder "$(get_placeholder 'PROJECT_LICENSE')")

export CURRENT_YEAR
CURRENT_YEAR=$(date +%Y)

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
