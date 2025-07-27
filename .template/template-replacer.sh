#!/bin/bash
# Template variable replacement script
# This script handles the replacement of template variables with actual values

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Script directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
CONFIG_FILE="${SCRIPT_DIR}/template-vars.json"

# Check if jq is available
if ! command -v jq &> /dev/null; then
    echo -e "${RED}Error: jq is required but not installed.${NC}"
    echo "Please install jq to continue."
    exit 1
fi

# Function to transform text to different cases
transform_case() {
    local text="$1"
    local transform="$2"
    
    case "$transform" in
        "snake_case")
            echo "$text" | sed 's/[-[:space:]]/_/g' | tr '[:upper:]' '[:lower:]'
            ;;
        "kebab_case")
            echo "$text" | sed 's/[_[:space:]]/-/g' | tr '[:upper:]' '[:lower:]'
            ;;
        "pascal_case")
            echo "$text" | sed 's/[-_]/ /g' | awk '{for(i=1;i<=NF;i++) $i=toupper(substr($i,1,1)) tolower(substr($i,2))}1' | tr -d ' '
            ;;
        "camel_case")
            local pascal=$(transform_case "$text" "pascal_case")
            echo "$pascal" | awk '{print tolower(substr($0,1,1)) substr($0,2)}'
            ;;
        *)
            echo "$text"
            ;;
    esac
}

# Function to validate a value against a regex pattern
validate_value() {
    local value="$1"
    local pattern="$2"
    
    if [[ -n "$pattern" ]] && ! [[ "$value" =~ $pattern ]]; then
        return 1
    fi
    return 0
}

# Function to get value from source
get_value_from_source() {
    local source="$1"
    local transform="${2:-}"
    local fallback="${3:-}"
    
    local value=""
    
    case "$source" in
        "repository_name")
            if [ -d .git ]; then
                value=$(basename `git rev-parse --show-toplevel` 2>/dev/null || echo "")
            fi
            if [ -z "$value" ] && [ -n "$GITHUB_REPOSITORY" ]; then
                value="${GITHUB_REPOSITORY##*/}"
            fi
            ;;
        "repository_owner")
            if [ -n "$GITHUB_REPOSITORY_OWNER" ]; then
                value="$GITHUB_REPOSITORY_OWNER"
            elif [ -d .git ]; then
                value=$(git remote get-url origin 2>/dev/null | sed -E 's/.*[:/]([^/]+)\/[^/]+\.git/\1/' || echo "")
            fi
            ;;
        "git_user_name")
            value=$(git config user.name 2>/dev/null || echo "")
            ;;
        "git_user_email")
            value=$(git config user.email 2>/dev/null || echo "")
            ;;
        "repository_description")
            # This would need to be fetched from GitHub API or provided
            value=""
            ;;
        "current_year")
            value=$(date +%Y)
            ;;
        "license")
            if [ -f LICENSE ]; then
                # Try to detect license type from LICENSE file
                if grep -q "MIT License" LICENSE; then
                    value="MIT"
                elif grep -q "Apache License" LICENSE; then
                    value="Apache-2.0"
                elif grep -q "GNU General Public License" LICENSE; then
                    value="GPL-3.0"
                fi
            fi
            ;;
        "static")
            # Value is provided in the config
            value=""
            ;;
    esac
    
    # Apply fallback if value is empty
    if [ -z "$value" ] && [ -n "$fallback" ]; then
        value="$fallback"
    fi
    
    # Apply transformation if specified
    if [ -n "$transform" ] && [ -n "$value" ]; then
        value=$(transform_case "$value" "$transform")
    fi
    
    echo "$value"
}

# Function to perform replacements in a file
replace_in_file() {
    local file="$1"
    local placeholder="$2"
    local replacement="$3"
    
    if [[ "$OSTYPE" == "darwin"* ]]; then
        sed -i '' "s/${placeholder}/${replacement}/g" "$file"
    else
        sed -i "s/${placeholder}/${replacement}/g" "$file"
    fi
}

# Main replacement function
perform_replacements() {
    local dry_run="${1:-false}"
    local interactive="${2:-false}"
    
    echo -e "${BLUE}🔄 Template Variable Replacement${NC}"
    echo -e "${BLUE}================================${NC}"
    echo ""
    
    # Load configuration
    if [ ! -f "$CONFIG_FILE" ]; then
        echo -e "${RED}Error: Configuration file not found: $CONFIG_FILE${NC}"
        exit 1
    fi
    
    # Extract variables from config
    local variables=$(jq -r '.variables | keys[]' "$CONFIG_FILE")
    
    # Collect replacement values
    declare -A replacements
    
    for var in $variables; do
        local var_config=$(jq -r ".variables.\"$var\"" "$CONFIG_FILE")
        local description=$(echo "$var_config" | jq -r '.description')
        local source=$(echo "$var_config" | jq -r '.source')
        local transform=$(echo "$var_config" | jq -r '.transform // empty')
        local fallback=$(echo "$var_config" | jq -r '.fallback // empty')
        local validation=$(echo "$var_config" | jq -r '.validation // empty')
        local required=$(echo "$var_config" | jq -r '.required // true')
        local static_value=$(echo "$var_config" | jq -r '.value // empty')
        
        # Get value based on source
        local value=""
        if [ "$source" = "static" ] && [ -n "$static_value" ]; then
            value="$static_value"
        else
            value=$(get_value_from_source "$source" "$transform" "$fallback")
        fi
        
        # Interactive mode - ask for confirmation/modification
        if [ "$interactive" = "true" ] && [ "$source" != "static" ]; then
            echo -e "${YELLOW}$description${NC}"
            if [ -n "$value" ]; then
                read -p "Value [$value]: " user_value
                if [ -n "$user_value" ]; then
                    value="$user_value"
                fi
            else
                if [ "$required" = "true" ]; then
                    while [ -z "$value" ]; do
                        read -p "Value (required): " value
                    done
                else
                    read -p "Value (optional): " value
                fi
            fi
        fi
        
        # Validate value
        if [ -n "$value" ] && [ -n "$validation" ]; then
            if ! validate_value "$value" "$validation"; then
                echo -e "${RED}Warning: Value '$value' does not match expected pattern: $validation${NC}"
                if [ "$interactive" = "true" ]; then
                    read -p "Continue anyway? (y/n) " -n 1 -r
                    echo
                    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
                        exit 1
                    fi
                fi
            fi
        fi
        
        # Store the replacement value
        if [ -n "$value" ]; then
            replacements["$var"]="$value"
        fi
    done
    
    # Display replacement summary
    echo ""
    echo -e "${GREEN}📝 Replacement Summary:${NC}"
    for var in "${!replacements[@]}"; do
        echo "  $var: ${replacements[$var]}"
    done
    echo ""
    
    if [ "$dry_run" = "true" ]; then
        echo -e "${YELLOW}Dry run mode - no files will be modified${NC}"
        return
    fi
    
    # Get file patterns
    local file_patterns=$(jq -r '.file_patterns[]' "$CONFIG_FILE")
    local exclude_patterns=$(jq -r '.exclude_patterns[]' "$CONFIG_FILE")
    
    # Build find command
    local find_cmd="find . -type f \\( "
    local first=true
    for pattern in $file_patterns; do
        if [ "$first" = true ]; then
            find_cmd="$find_cmd -name \"$pattern\""
            first=false
        else
            find_cmd="$find_cmd -o -name \"$pattern\""
        fi
    done
    find_cmd="$find_cmd \\)"
    
    # Add exclusions
    for pattern in $exclude_patterns; do
        find_cmd="$find_cmd -not -path \"$pattern\""
    done
    
    # Perform replacements
    echo -e "${BLUE}🔄 Performing replacements...${NC}"
    
    # Regular replacements
    for var in $variables; do
        if [ -n "${replacements[$var]}" ]; then
            local placeholders=$(jq -r ".variables.\"$var\".placeholders[]" "$CONFIG_FILE" 2>/dev/null)
            for placeholder in $placeholders; do
                echo "  Replacing '$placeholder' with '${replacements[$var]}'..."
                eval "$find_cmd" | while read file; do
                    if grep -q "$placeholder" "$file" 2>/dev/null; then
                        replace_in_file "$file" "$placeholder" "${replacements[$var]}"
                    fi
                done
            done
        fi
    done
    
    # Special replacements
    echo -e "${BLUE}🔧 Applying special replacements...${NC}"
    local special_files=$(jq -r '.special_replacements | keys[]' "$CONFIG_FILE" 2>/dev/null)
    for special_file in $special_files; do
        if [ -f "$special_file" ]; then
            local replacements_json=$(jq -r ".special_replacements.\"$special_file\"" "$CONFIG_FILE")
            echo "$replacements_json" | jq -r 'to_entries[] | "\(.key)|\(.value)"' | while IFS='|' read -r pattern replacement; do
                # Substitute variables in the replacement string
                for var in "${!replacements[@]}"; do
                    replacement="${replacement//\$\{$var\}/${replacements[$var]}}"
                done
                echo "  Special: In $special_file, replacing '$pattern' with '$replacement'"
                replace_in_file "$special_file" "$pattern" "$replacement"
            done
        fi
    done
    
    echo ""
    echo -e "${GREEN}✅ Template replacement complete!${NC}"
}

# Parse command line arguments
DRY_RUN=false
INTERACTIVE=false

while [[ $# -gt 0 ]]; do
    case $1 in
        --dry-run)
            DRY_RUN=true
            shift
            ;;
        --interactive|-i)
            INTERACTIVE=true
            shift
            ;;
        --help|-h)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  --dry-run       Show what would be replaced without making changes"
            echo "  --interactive   Prompt for values interactively"
            echo "  --help          Show this help message"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

# Run the replacement
perform_replacements "$DRY_RUN" "$INTERACTIVE"