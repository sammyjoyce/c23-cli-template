#!/usr/bin/env bash
# create-demo.sh - Record terminal demos for the CLI.
#
# Records short asciinema casts of the actual commands the binary supports and
# converts them to GIFs. Customise the demo functions below as you add features.
#
# Requirements:
#   asciinema  - install with your OS package manager
#   agg        - cargo install --git https://github.com/asciinema/agg
#
# The binary name is taken from build.zig.zon so the script keeps working after
# the template is renamed.

set -euo pipefail

repo_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$repo_root"

demo_dir="docs/demos"
recordings_dir="$demo_dir/recordings"

binary_name=$(awk '
    /\.name = / {
        gsub(/.*\.name = /, "")
        gsub(/[,.]/, "")
        gsub(/"/, "")
        print
    }
' build.zig.zon)

binary="./zig-out/bin/${binary_name}"

red='\033[0;31m'
green='\033[0;32m'
blue='\033[0;34m'
nc='\033[0m'

require_tools() {
  local missing=()
  for tool in asciinema agg zig; do
    command -v "$tool" >/dev/null 2>&1 || missing+=("$tool")
  done
  if ((${#missing[@]})); then
    printf "${red}Missing dependencies: %s${nc}\n" "${missing[*]}" >&2
    cat <<'EOF' >&2

Install with:
  asciinema  - your OS package manager (apt/brew/pacman/nix...)
  agg        - cargo install --git https://github.com/asciinema/agg
  zig        - https://ziglang.org/download/
EOF
    exit 1
  fi
}

build_binary() {
  printf "${blue}Building %s with TUI enabled...${nc}\n" "$binary_name"
  zig build -Denable-tui=true
  if [[ ! -x $binary ]]; then
    printf "${red}Binary not found at %s${nc}\n" "$binary" >&2
    exit 1
  fi
}

record_demo() {
  local name="$1"
  local title="$2"
  local script="$3"

  printf "${blue}Recording: %s${nc}\n" "$title"
  mkdir -p "$recordings_dir"

  local script_file
  script_file=$(mktemp -t "demo-${name}.XXXXXX.sh")
  trap 'rm -f "$script_file"' RETURN

  cat >"$script_file" <<EOF
#!/usr/bin/env bash
set -e
clear
printf '${green}Demo: %s${nc}\n\n' "$title"
sleep 1
$script
echo
printf '${green}Demo complete.${nc}\n'
sleep 1
EOF
  chmod +x "$script_file"

  asciinema rec \
    --title "$title" \
    --command "$script_file" \
    --overwrite \
    "$recordings_dir/${name}.cast"

  printf "${blue}Converting to GIF...${nc}\n"
  agg \
    --theme monokai \
    --font-size 14 \
    --line-height 1.4 \
    "$recordings_dir/${name}.cast" \
    "$demo_dir/${name}.gif"

  printf "${green}Wrote %s/%s.gif${nc}\n" "$demo_dir" "$name"
}

demo_help_and_version() {
  record_demo "help-and-version" "Help and version" "
$binary --help | head -40
sleep 2
$binary --version
sleep 1
"
}

demo_hello_and_echo() {
  record_demo "hello-and-echo" "Hello and echo commands" "
$binary hello
sleep 1
$binary hello Alice
sleep 1
$binary echo This is a CLI starter template.
sleep 1
"
}

demo_info_and_doctor() {
  record_demo "info-and-doctor" "Info and doctor commands" "
$binary info
sleep 2
$binary --json info
sleep 2
$binary doctor
sleep 2
"
}

# Add a recorder for the interactive TUI menu when you have a deterministic
# input sequence you want to demonstrate (e.g. driving via asciinema's
# scripting or a pre-recorded cast). Left as a TODO so it doesn't block the
# rest of the demo workflow.

main() {
  require_tools
  build_binary
  demo_help_and_version
  demo_hello_and_echo
  demo_info_and_doctor
  printf "${green}All demos written to %s/${nc}\n" "$demo_dir"
}

main "$@"
