{
  description = "C23 CLI template development environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs =
    { nixpkgs, ... }:
    let
      systems = [
        "x86_64-linux"
        "aarch64-linux"
        "x86_64-darwin"
        "aarch64-darwin"
      ];
      eachSystem = nixpkgs.lib.genAttrs systems;
    in
    {
      devShells = eachSystem (
        system:
        let
          pkgs = import nixpkgs { inherit system; };
          lib = pkgs.lib;
          ncursesDev = lib.getDev pkgs.ncurses;
          ncursesLib = lib.getLib pkgs.ncurses;
          zig = pkgs.zig_0_16 or pkgs.zig;
          terminalTestPython = pkgs.python3.withPackages (ps: [
            ps.pexpect
            ps.pyte
          ]);
          projectTooling = [
            # Build and day-to-day workflow.
            zig
            pkgs.pkg-config
            pkgs.git
            pkgs.gh
            pkgs.just
            pkgs.bashInteractive
            pkgs.coreutils
            pkgs.findutils
            pkgs.gawk
            pkgs.gnugrep
            pkgs.gnused
            pkgs.gnutar
            pkgs.xz
            pkgs.curl

            # C/Zig editing, debugging, and CI lint parity.
            pkgs.clang
            pkgs.clang-tools
            pkgs.cmake
            pkgs.ninja
            pkgs.lldb
            pkgs.cppcheck
            pkgs.shellcheck
            pkgs.nixfmt

            # Pre-commit, terminal-test harness, and documentation hooks.
            pkgs.pre-commit
            terminalTestPython
            pkgs.nodejs
            pkgs.markdownlint-cli

            # Template setup/cleanup scripts.
            pkgs.jq
            pkgs.sd
            pkgs.gum

            # Demo, security, and release-support tooling used by repo scripts/CI.
            pkgs.asciinema
            pkgs.asciinema-agg
            pkgs.gitleaks
            pkgs.syft
          ]
          ++ lib.optionals pkgs.stdenv.isLinux [
            pkgs.gdb
            pkgs.valgrind
          ];
        in
        {
          default = pkgs.mkShell {
            name = "c23-cli-template";

            packages = projectTooling;

            buildInputs = [
              ncursesDev
              ncursesLib
            ];

            # Zig's linkSystemLibrary("ncurses", .{}) consults pkg-config first.
            # Keep the paths explicit so default `zig build run` also works
            # on NixOS, where system libraries are not visible under /lib or /usr.
            PKG_CONFIG_PATH = "${ncursesDev}/lib/pkgconfig";
            CPATH = "${ncursesDev}/include";
            LIBRARY_PATH = "${ncursesLib}/lib";
            LD_LIBRARY_PATH = lib.optionalString pkgs.stdenv.isLinux "${ncursesLib}/lib";
            DYLD_FALLBACK_LIBRARY_PATH = lib.optionalString pkgs.stdenv.isDarwin "${ncursesLib}/lib";
            TERMINFO_DIRS = "${ncursesLib}/share/terminfo";
          };
        }
      );

      formatter = eachSystem (
        system:
        let
          pkgs = import nixpkgs { inherit system; };
        in
        pkgs.writeShellScriptBin "format-nix" ''
          if [ "$#" -eq 0 ]; then
            set -- flake.nix
          fi

          exec ${pkgs.nixfmt}/bin/nixfmt "$@"
        ''
      );
    };
}
