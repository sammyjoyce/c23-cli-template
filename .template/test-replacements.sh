#!/bin/bash
# Test script for template replacement system
# This script validates that the template replacements work correctly

# Don't exit on error for tests
set +e

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Script directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

echo "🧪 Template Replacement Test Suite"
echo "================================="
echo ""

# Test counter
TESTS_PASSED=0
TESTS_FAILED=0

# Test function
run_test() {
    local test_name="$1"
    local test_command="$2"
    local expected_result="$3"
    
    echo -n "Testing: $test_name... "
    
    if eval "$test_command"; then
        if [ -z "$expected_result" ] || [ "$expected_result" = "true" ]; then
            echo -e "${GREEN}✓ PASSED${NC}"
            ((TESTS_PASSED++))
        else
            echo -e "${RED}✗ FAILED${NC} (expected failure but passed)"
            ((TESTS_FAILED++))
        fi
    else
        if [ "$expected_result" = "false" ]; then
            echo -e "${GREEN}✓ PASSED${NC} (expected failure)"
            ((TESTS_PASSED++))
        else
            echo -e "${RED}✗ FAILED${NC}"
            ((TESTS_FAILED++))
        fi
    fi
}

# Test case transformations
test_transformations() {
    echo ""
    echo "📝 Testing case transformations..."
    
    # Test snake_case
    result=$(echo "My-Project-Name" | sed 's/[-[:space:]]/_/g' | tr '[:upper:]' '[:lower:]')
    run_test "snake_case transformation" "[ '$result' = 'my_project_name' ]"
    
    # Test kebab_case
    result=$(echo "My_Project_Name" | sed 's/[_[:space:]]/-/g' | tr '[:upper:]' '[:lower:]')
    run_test "kebab_case transformation" "[ '$result' = 'my-project-name' ]"
    
    # Test pascal_case
    result=$(echo "my-project-name" | sed 's/[-_]/ /g' | awk '{for(i=1;i<=NF;i++) $i=toupper(substr($i,1,1)) tolower(substr($i,2))}1' | tr -d ' ')
    run_test "pascal_case transformation" "[ '$result' = 'MyProjectName' ]"
    
    # Test camel_case
    pascal=$(echo "my-project-name" | sed 's/[-_]/ /g' | awk '{for(i=1;i<=NF;i++) $i=toupper(substr($i,1,1)) tolower(substr($i,2))}1' | tr -d ' ')
    result=$(echo "$pascal" | awk '{print tolower(substr($0,1,1)) substr($0,2)}')
    run_test "camel_case transformation" "[ '$result' = 'myProjectName' ]"
}

# Test validation patterns
test_validations() {
    echo ""
    echo "🔍 Testing validation patterns..."
    
    # Define validate_value function inline for testing
    validate_value() {
        local value="$1"
        local pattern="$2"
        
        if [[ -n "$pattern" ]] && ! [[ "$value" =~ $pattern ]]; then
            return 1
        fi
        return 0
    }
    
    # Test project name validation
    run_test "Valid project name" "validate_value 'my-project' '^[a-zA-Z][a-zA-Z0-9_-]*$'"
    run_test "Invalid project name (starts with number)" "validate_value '123-project' '^[a-zA-Z][a-zA-Z0-9_-]*$'" "false"
    run_test "Invalid project name (special chars)" "validate_value 'my@project!' '^[a-zA-Z][a-zA-Z0-9_-]*$'" "false"
    
    # Test email validation
    run_test "Valid email" "validate_value 'user@example.com' '^[^@]+@[^@]+\.[^@]+$'"
    run_test "Invalid email (no @)" "validate_value 'userexample.com' '^[^@]+@[^@]+\.[^@]+$'" "false"
    run_test "Invalid email (no domain)" "validate_value 'user@' '^[^@]+@[^@]+\.[^@]+$'" "false"
}

# Test configuration file
test_config_file() {
    echo ""
    echo "📄 Testing configuration file..."
    
    CONFIG_FILE="${SCRIPT_DIR}/template-vars.json"
    
    # Check if config file exists
    run_test "Config file exists" "[ -f '$CONFIG_FILE' ]"
    
    # Check if config is valid JSON
    run_test "Config is valid JSON" "jq . '$CONFIG_FILE' > /dev/null 2>&1"
    
    # Check required fields
    run_test "Config has variables section" "jq -e '.variables' '$CONFIG_FILE' > /dev/null 2>&1"
    run_test "Config has file_patterns section" "jq -e '.file_patterns' '$CONFIG_FILE' > /dev/null 2>&1"
    run_test "Config has exclude_patterns section" "jq -e '.exclude_patterns' '$CONFIG_FILE' > /dev/null 2>&1"
}

# Test dry run mode
test_dry_run() {
    echo ""
    echo "🏃 Testing dry run mode..."
    
    # Create a temporary test directory
    TEST_DIR=$(mktemp -d)
    cp -r "${SCRIPT_DIR}/.." "$TEST_DIR/template"
    cd "$TEST_DIR/template"
    
    # Create a test file with placeholders
    echo "myapp cli_starter Your Name" > test_file.txt
    
    # Run in dry run mode
    output=$(.template/template-replacer.sh --dry-run 2>&1)
    
    # Check that file wasn't modified
    run_test "Dry run doesn't modify files" "grep -q 'myapp cli_starter Your Name' test_file.txt"
    
    # Clean up
    cd - > /dev/null
    rm -rf "$TEST_DIR"
}

# Test replacement in different file types
test_file_types() {
    echo ""
    echo "📁 Testing different file types..."
    
    # Create temporary test directory
    TEST_DIR=$(mktemp -d)
    mkdir -p "$TEST_DIR/src"
    
    # Create test files
    echo "myapp" > "$TEST_DIR/test.md"
    echo "cli_starter" > "$TEST_DIR/test.c"
    echo "cli-starter" > "$TEST_DIR/test.h"
    echo "yourusername" > "$TEST_DIR/test.json"
    echo "Your Name" > "$TEST_DIR/test.yaml"
    echo "namespace-profile-linux-amd64" > "$TEST_DIR/test.yml"
    
    # Files that should be ignored
    echo "myapp" > "$TEST_DIR/test.exe"
    echo "myapp" > "$TEST_DIR/test.o"
    
    cd "$TEST_DIR"
    
    # Check that appropriate files would be processed
    files_to_process=$(find . -type f \( -name "*.md" -o -name "*.c" -o -name "*.h" -o -name "*.json" -o -name "*.yaml" -o -name "*.yml" \) | wc -l)
    run_test "Correct number of files identified" "[ $files_to_process -eq 6 ]"
    
    # Clean up
    cd - > /dev/null
    rm -rf "$TEST_DIR"
}

# Main test execution
main() {
    # Check prerequisites
    if ! command -v jq &> /dev/null; then
        echo -e "${RED}Error: jq is required to run tests${NC}"
        echo "Please install jq first."
        exit 1
    fi
    
    # Run all tests
    test_transformations
    test_validations
    test_config_file
    test_dry_run
    test_file_types
    
    # Summary
    echo ""
    echo "================================="
    echo "Test Summary:"
    echo -e "  ${GREEN}Passed: $TESTS_PASSED${NC}"
    echo -e "  ${RED}Failed: $TESTS_FAILED${NC}"
    echo ""
    
    if [ $TESTS_FAILED -eq 0 ]; then
        echo -e "${GREEN}✅ All tests passed!${NC}"
        exit 0
    else
        echo -e "${RED}❌ Some tests failed!${NC}"
        exit 1
    fi
}

# Run tests
main "$@"