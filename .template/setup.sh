#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage: .template/setup.sh [--non-interactive] [--cleanup] [--cleanup-only]

  --non-interactive  Use environment variables, git metadata, and defaults.
  --cleanup          Remove template-only files after replacement.
  --cleanup-only     Remove template-only files without running replacement.
  -h, --help         Show this message.
EOF
}

non_interactive=0
cleanup_after=0
cleanup_only=0

while [[ $# -gt 0 ]]; do
  case "$1" in
  --non-interactive)
    non_interactive=1
    ;;
  --cleanup)
    cleanup_after=1
    ;;
  --cleanup-only)
    cleanup_only=1
    ;;
  -h | --help)
    usage
    exit 0
    ;;
  *)
    echo "Error: unknown option '$1'" >&2
    usage >&2
    exit 1
    ;;
  esac
  shift
done

script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
repo_root=$(cd "$script_dir/.." && pwd)
vars_json="$script_dir/template-vars.json"

info() {
  if command -v gum >/dev/null 2>&1 && ((non_interactive == 0)); then
    gum style --foreground 212 "INFO: $1"
  else
    printf 'INFO: %s\n' "$1"
  fi
}

die() {
  printf 'ERROR: %s\n' "$1" >&2
  exit 1
}

require_command() {
  if ! command -v "$1" >/dev/null 2>&1; then
    die "'$1' is required."
  fi
}

cleanup_template_files() {
  rm -rf "$repo_root/.template"
  rm -rf "$repo_root/.agents/skills/github-template-repositories"
  rm -f "$repo_root/.templatesyncignore"
  rm -f "$repo_root/.github/template.yml"
  rm -f "$repo_root/.github/workflows/template-cleanup.yml"
}

refresh_zig_fingerprint() {
  local zon_file="$repo_root/build.zig.zon"
  [[ -f "$zon_file" ]] || return

  local output status new_fingerprint current_pattern
  set +e
  output=$(cd "$repo_root" && zig build --fetch --summary none 2>&1)
  status=$?
  set -e

  if ((status == 0)); then
    return
  fi

  if [[ $output =~ use[[:space:]]this[[:space:]]value:[[:space:]](0x[[:xdigit:]]+) ]]; then
    new_fingerprint="${BASH_REMATCH[1]}"
    current_pattern=$(grep -Eo '\.fingerprint = 0x[[:xdigit:]]+' "$zon_file" | head -n 1 || true)
    if [[ -z $current_pattern ]]; then
      printf '%s\n' "$output" >&2
      die "Cannot find existing build.zig.zon fingerprint to replace."
    fi

    sd -F "$current_pattern" ".fingerprint = $new_fingerprint" "$zon_file"
    info "Updated build.zig.zon fingerprint to match the package name."

    set +e
    output=$(cd "$repo_root" && zig build --fetch --summary none 2>&1)
    status=$?
    set -e
    if ((status != 0)); then
      printf '%s\n' "$output" >&2
      die "Zig manifest check failed after fingerprint refresh."
    fi
    return
  fi

  printf '%s\n' "$output" >&2
  die "Zig manifest check failed."
}

if ((cleanup_only)); then
  cleanup_template_files
  info "Removed template-only files."
  exit 0
fi

[[ -f "$vars_json" ]] || die "Cannot find template variables at '$vars_json'."
require_command jq
require_command sd
require_command zig

get_default() {
  local key="$1"
  jq -r --arg key "$key" '
        (.variables[$key] // {}) as $var |
        ($var.fallback // ($var.placeholders[0] // ""))
    ' "$vars_json"
}

get_prompt() {
  local key="$1"
  local description
  description=$(jq -r --arg key "$key" '.variables[$key].description // empty' "$vars_json")
  if [[ -n $description ]]; then
    printf '%s' "$description"
  else
    printf 'Enter %s' "$key"
  fi
}

prompt_var() {
  # Interactive prompt that leaves the var empty when the user hits Enter
  # without typing. That way replacer.sh's source detection (git metadata,
  # repository name etc.) gets a chance to run; the placeholder is shown
  # only as the gum placeholder text, never pre-filled into --value.
  local name="$1"
  local current="${!name:-}"
  local result
  result=$(gum input --value "$current" --placeholder "$(get_prompt "$name") (default: $(get_default "$name"))")
  printf -v "$name" '%s' "$result"
  export "$name"
}

if ((non_interactive == 0)); then
  require_command gum
  gum style --border normal --padding "1 2" --border-foreground 212 "C23 TUI + CLI Template Setup"

  for v in PROJECT_NAME PROJECT_DESCRIPTION AUTHOR_NAME AUTHOR_EMAIL GITHUB_USERNAME PROJECT_LICENSE; do
    prompt_var "$v"
  done
else
  # Pass through only explicitly-set env vars so replacer can detect
  # sources. Do not pre-populate with placeholder defaults.
  for v in PROJECT_NAME PROJECT_DESCRIPTION AUTHOR_NAME AUTHOR_EMAIL GITHUB_USERNAME PROJECT_URL PROJECT_LICENSE; do
    if [[ -n ${!v:-} ]]; then
      export "$v"
    fi
  done
fi

export CURRENT_YEAR="${CURRENT_YEAR:-$(date +%Y)}"

if [[ -f "$script_dir/TEMPLATE_README.md" ]]; then
  cp "$script_dir/TEMPLATE_README.md" "$repo_root/README.md"
fi

info "Running template replacement."
bash "$script_dir/replacer.sh"
refresh_zig_fingerprint

if ((cleanup_after == 0 && non_interactive == 0)); then
  if gum confirm "Remove template-only files now?"; then
    cleanup_after=1
  fi
fi

if ((cleanup_after)); then
  cleanup_template_files
  info "Removed template-only files."
else
  info "Template-only files kept. Run '.template/setup.sh --cleanup-only' when ready."
fi
