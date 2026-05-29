const std = @import("std");

fn buildPanic(err: anyerror) noreturn {
    std.debug.panic("build allocation failed: {s}", .{@errorName(err)});
}

fn appendEscapedCString(out: *std.ArrayList(u8), allocator: std.mem.Allocator, value: []const u8) void {
    out.append(allocator, '"') catch |err| buildPanic(err);
    for (value) |ch| {
        switch (ch) {
            '\\' => out.appendSlice(allocator, "\\\\") catch |err| buildPanic(err),
            '"' => out.appendSlice(allocator, "\\\"") catch |err| buildPanic(err),
            '\n' => out.appendSlice(allocator, "\\n") catch |err| buildPanic(err),
            '\r' => out.appendSlice(allocator, "\\r") catch |err| buildPanic(err),
            '\t' => out.appendSlice(allocator, "\\t") catch |err| buildPanic(err),
            0 => out.appendSlice(allocator, "\\0") catch |err| buildPanic(err),
            else => if (ch < 0x20) {
                const hex = "0123456789abcdef";
                out.appendSlice(allocator, "\\x") catch |err| buildPanic(err);
                out.append(allocator, hex[ch >> 4]) catch |err| buildPanic(err);
                out.append(allocator, hex[ch & 0x0f]) catch |err| buildPanic(err);
            } else {
                out.append(allocator, ch) catch |err| buildPanic(err);
            },
        }
    }
    out.append(allocator, '"') catch |err| buildPanic(err);
}

fn cStringDefine(b: *std.Build, name: []const u8, value: []const u8) []const u8 {
    var out: std.ArrayList(u8) = .empty;
    errdefer out.deinit(b.allocator);
    out.appendSlice(b.allocator, "-D") catch |err| buildPanic(err);
    out.appendSlice(b.allocator, name) catch |err| buildPanic(err);
    out.append(b.allocator, '=') catch |err| buildPanic(err);
    appendEscapedCString(&out, b.allocator, value);
    return out.toOwnedSlice(b.allocator) catch |err| buildPanic(err);
}

const TerminalTestBackend = enum {
    auto,
    none,
    ghostty,
};

const GhosttyTerminalApi = struct {
    pkg_lib_dir: ?[]const u8 = null,
    prefix_lib_dir: ?[]const u8 = null,
};

const TerminalTestPlan = union(enum) {
    run_ghostty: GhosttyTerminalApi,
    skip: []const u8,
    fail: []const u8,
};

fn resolveTerminalTestPlan(b: *std.Build, enable_tui: bool, terminal_backend: TerminalTestBackend, target: std.Build.ResolvedTarget, ghostty_vt_prefix: ?[]const u8) TerminalTestPlan {
    const target_is_windows = target.result.os.tag == .windows;

    if (terminal_backend == .ghostty and target_is_windows) {
        return .{ .fail = "-Dterminal-backend=ghostty is not supported on Windows" };
    }
    if (terminal_backend == .ghostty and !enable_tui) {
        return .{ .fail = "-Dterminal-backend=ghostty requires -Denable-tui=true because the PTY scenarios exercise the TUI." };
    }
    if (terminal_backend == .none) {
        return .{ .skip = "Skipping PTY/TUI terminal scenarios: disabled by -Dterminal-backend=none." };
    }
    if (!enable_tui) {
        return .{ .skip = "Skipping PTY/TUI terminal scenarios: pass -Denable-tui=true to build the TUI." };
    }
    if (target_is_windows) {
        return .{ .skip = "Skipping PTY/TUI terminal scenarios: Ghostty VT backend is POSIX-only." };
    }

    const ghostty_api = if (ghostty_vt_prefix) |pref|
        ghosttyPrefixTerminalApi(b, pref)
    else
        ghosttyPkgConfigTerminalApi(b);
    if (ghostty_api == null and terminal_backend == .ghostty) {
        return .{ .fail = "-Dterminal-backend=ghostty requires libghostty-vt with the terminal/formatter API.\n  Install it so pkg-config can find libghostty-vt.pc, or pass -Dghostty-vt-prefix=/path." };
    }
    if (ghostty_api == null) {
        return .{ .skip = "Skipping PTY/TUI terminal scenarios: libghostty-vt was not found; use -Dterminal-backend=ghostty to require it." };
    }

    return .{ .run_ghostty = ghostty_api.? };
}

fn commandSucceeds(b: *std.Build, argv: []const []const u8) bool {
    const child_res = std.process.run(b.allocator, b.graph.io, .{ .argv = argv }) catch return false;
    defer b.allocator.free(child_res.stdout);
    defer b.allocator.free(child_res.stderr);

    return switch (child_res.term) {
        .exited => |code| code == 0,
        else => false,
    };
}

fn commandOutputTrimmed(b: *std.Build, argv: []const []const u8) ?[]const u8 {
    const child_res = std.process.run(b.allocator, b.graph.io, .{ .argv = argv }) catch return null;
    defer b.allocator.free(child_res.stdout);
    defer b.allocator.free(child_res.stderr);

    switch (child_res.term) {
        .exited => |code| if (code != 0) return null,
        else => return null,
    }

    const trimmed = std.mem.trim(u8, child_res.stdout, "\r\n\t ");
    if (trimmed.len == 0) return null;
    return b.dupe(trimmed);
}

fn pathExists(b: *std.Build, path: []const u8) bool {
    std.Io.Dir.cwd().access(b.graph.io, path, .{}) catch return false;
    return true;
}

fn ghosttyLibraryExists(b: *std.Build, lib_dir: []const u8) bool {
    return pathExists(b, b.fmt("{s}/libghostty-vt.so", .{lib_dir})) or
        pathExists(b, b.fmt("{s}/libghostty-vt.dylib", .{lib_dir})) or
        pathExists(b, b.fmt("{s}/libghostty-vt.a", .{lib_dir}));
}

fn ghosttyApiPresent(b: *std.Build, include_dir: []const u8, lib_dir: []const u8) bool {
    return pathExists(b, b.fmt("{s}/ghostty/vt.h", .{include_dir})) and
        pathExists(b, b.fmt("{s}/ghostty/vt/terminal.h", .{include_dir})) and
        pathExists(b, b.fmt("{s}/ghostty/vt/formatter.h", .{include_dir})) and
        ghosttyLibraryExists(b, lib_dir);
}

fn ghosttyPrefixLibDir(b: *std.Build, prefix: []const u8) ?[]const u8 {
    const lib_dir = b.fmt("{s}/lib", .{prefix});
    if (ghosttyLibraryExists(b, lib_dir)) return lib_dir;

    const lib64_dir = b.fmt("{s}/lib64", .{prefix});
    return if (ghosttyLibraryExists(b, lib64_dir)) lib64_dir else null;
}

fn ghosttyPrefixTerminalApi(b: *std.Build, prefix: []const u8) ?GhosttyTerminalApi {
    const include_dir = b.fmt("{s}/include", .{prefix});
    const lib_dir = ghosttyPrefixLibDir(b, prefix) orelse return null;
    return if (ghosttyApiPresent(b, include_dir, lib_dir))
        .{ .prefix_lib_dir = lib_dir }
    else
        null;
}

fn ghosttyPkgConfigTerminalApi(b: *std.Build) ?GhosttyTerminalApi {
    if (!commandSucceeds(b, &.{ "pkg-config", "--exists", "libghostty-vt" })) return null;
    const include_dir = commandOutputTrimmed(b, &.{ "pkg-config", "--variable=includedir", "libghostty-vt" }) orelse return null;
    const lib_dir = commandOutputTrimmed(b, &.{ "pkg-config", "--variable=libdir", "libghostty-vt" }) orelse return null;

    return if (ghosttyApiPresent(b, include_dir, lib_dir))
        .{ .pkg_lib_dir = lib_dir }
    else
        null;
}

fn addMessageCommand(b: *std.Build, message: []const u8) *std.Build.Step.Run {
    if (b.graph.host.result.os.tag == .windows) {
        return b.addSystemCommand(&.{ "cmd", "/C", "echo", message });
    }

    return b.addSystemCommand(&.{ "printf", "%s\n", message });
}

fn prependRunEnvPath(run: *std.Build.Step.Run, key: []const u8, dir: []const u8) void {
    const b = run.step.owner;
    const env_map = run.getEnvMap();

    if (env_map.get(key)) |current| {
        const value = if (current.len == 0)
            b.dupe(dir)
        else
            b.fmt("{s}{c}{s}", .{ dir, std.fs.path.delimiter, current });
        env_map.put(key, value) catch |err| std.debug.panic(
            "failed to set {s}: {s}",
            .{ key, @errorName(err) },
        );
    } else {
        env_map.put(key, b.dupe(dir)) catch |err| std.debug.panic(
            "failed to set {s}: {s}",
            .{ key, @errorName(err) },
        );
    }
}

// Link the curses/terminfo library into `module`. `lib_name`, when provided,
// is the exact library detected at configure time (e.g. the terminfo backend's
// detected name on a host that only ships `tinfo` or `ncurses`); pass null to
// fall back to the platform default (`ncursesw` on Unix, `pdcurses` on Windows).
// Hardcoding `ncursesw` here would make the build link a library that may not
// exist even though detection picked a different one.
fn linkCurses(module: *std.Build.Module, target: std.Build.ResolvedTarget, curses_prefix: ?[]const u8, b: *std.Build, lib_name: ?[]const u8) void {
    if (curses_prefix) |pref| {
        module.addIncludePath(.{ .cwd_relative = b.fmt("{s}/include", .{pref}) });
        module.addLibraryPath(.{ .cwd_relative = b.fmt("{s}/lib", .{pref}) });
    }

    if (lib_name) |name| {
        module.linkSystemLibrary(name, .{});
    } else if (target.result.os.tag == .windows) {
        module.linkSystemLibrary("pdcurses", .{});
    } else {
        module.linkSystemLibrary("ncursesw", .{});
    }
}

// Return true if pkg-config reports the named library exists.
fn pkgConfigExists(b: *std.Build, name: []const u8) bool {
    const res = std.process.run(b.allocator, b.graph.io, .{
        .argv = &.{ "pkg-config", "--exists", name },
    }) catch return false;
    defer b.allocator.free(res.stdout);
    defer b.allocator.free(res.stderr);
    return switch (res.term) {
        .exited => |code| code == 0,
        else => false,
    };
}

// Detect a terminfo-capable library for the CLI styling backend. Returns the
// pkg-config name to link, or null when none is available (callers then use the
// ANSI fallback backend). Windows ships PDCurses, which provides terminfo too.
fn detectTerminfoLib(b: *std.Build, target: std.Build.ResolvedTarget) ?[]const u8 {
    if (target.result.os.tag == .windows) {
        return if (pkgConfigExists(b, "pdcurses")) "pdcurses" else null;
    }
    const candidates = [_][]const u8{ "ncursesw", "tinfo", "ncurses" };
    for (candidates) |name| {
        if (pkgConfigExists(b, name)) {
            return name;
        }
    }
    return null;
}

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const version_str = b.option([]const u8, "version", "Application version string") orelse "0.1.0";
    const app_name = b.option([]const u8, "app-name", "Application and binary name") orelse "myapp";
    const binary_name = app_name;

    // Attempt to inject current git commit hash, fall back to "unknown".
    const git_commit = blk: {
        const child_res = std.process.run(b.allocator, b.graph.io, .{
            .argv = &.{ "git", "rev-parse", "--short", "HEAD" },
        }) catch break :blk "unknown";
        defer b.allocator.free(child_res.stdout);
        defer b.allocator.free(child_res.stderr);

        switch (child_res.term) {
            .exited => |code| if (code != 0) break :blk "unknown",
            else => break :blk "unknown",
        }

        if (child_res.stdout.len == 0) break :blk "unknown";
        const trimmed = std.mem.trim(u8, child_res.stdout, "\r\n");
        if (trimmed.len == 0) break :blk "unknown";
        break :blk b.dupe(trimmed);
    };

    // Read SOURCE_DATE_EPOCH from the build graph's captured environment rather
    // than std.c.getenv: the latter forces the build runner to link libc, which
    // breaks Linux/Windows CI ("dependency on libc must be explicitly
    // specified"). graph.environ_map is the libc-free std way to read env in a
    // build script. Returns a []const u8 to match how build_date is consumed.
    const build_date = b.graph.environ_map.get("SOURCE_DATE_EPOCH") orelse "omitted";

    const enable_tui = b.option(bool, "enable-tui", "Enable TUI support with ncurses/PDCurses (default: false)") orelse false;
    const curses_prefix = b.option([]const u8, "curses-prefix", "Override ncurses/PDCurses prefix (e.g. /usr/local/opt/ncurses)");
    const terminal_backend = b.option(TerminalTestBackend, "terminal-backend", "Terminal test backend: auto, none, or ghostty") orelse .auto;
    const ghostty_vt_prefix = b.option([]const u8, "ghostty-vt-prefix", "Override libghostty-vt install prefix for Ghostty-backed terminal tests");
    const strict = b.option(bool, "strict", "Treat warnings as errors and enable extra diagnostics") orelse false;
    const harden = b.option(bool, "harden", "Add supported compiler hardening flags") orelse false;

    // CLI styling layer (Fang-style help/errors/version). Independent of the
    // TUI: it uses terminfo for capability detection/emission without entering
    // curses screen mode, and falls back to an ANSI backend when terminfo is
    // unavailable.
    const enable_cli_style = b.option(bool, "enable-cli-style", "Enable styled CLI help/errors/version (default: true)") orelse true;
    const cli_terminfo_mode = b.option([]const u8, "cli-terminfo", "CLI terminfo backend: auto, required, disabled (default: auto)") orelse "auto";

    const want_terminfo = enable_cli_style and !std.mem.eql(u8, cli_terminfo_mode, "disabled");
    const terminfo_lib: ?[]const u8 = if (want_terminfo) detectTerminfoLib(b, target) else null;
    const have_terminfo = terminfo_lib != null;
    if (want_terminfo and !have_terminfo and std.mem.eql(u8, cli_terminfo_mode, "required")) {
        std.debug.panic("-Dcli-terminfo=required but no terminfo library (ncursesw/tinfo/ncurses) was found via pkg-config", .{});
    }

    const exe = b.addExecutable(.{
        .name = binary_name,
        .root_module = b.createModule(.{
            .root_source_file = null,
            .target = target,
            .optimize = optimize,
            .link_libc = true,
        }),
    });
    // Ensure local headers are discoverable regardless of include style
    exe.root_module.addIncludePath(b.path("src"));

    // Base source files
    const base_sources = [_][]const u8{
        "src/main.c",
        "src/core/error.c",
        "src/core/config.c",
        "src/core/config_json.c",
        "src/utils/logging.c",
        "src/utils/memory.c",
        "src/utils/colors.c",
        "src/io/input.c",
        "src/io/output.c",
        "src/cli/help.c",
        "src/cli/args.c",
        "src/cli/commands.c",
        "src/cli/commands_basic.c",
        "src/cli/commands_info.c",
        "src/cli/commands_doctor.c",
        "src/cli/commands_menu.c",
        "src/cli/opencli_contract.c",
        "src/cli/commands_opencli.c",
    };

    // Base flags shared by the binary and test targets.
    // _GNU_SOURCE pulls in the POSIX surface we need (timespec_get, mlock,
    // forkpty etc.) without conflicting with _XOPEN_SOURCE on glibc.
    const base_flags = [_][]const u8{
        "-Wall",
        "-Wextra",
        "-std=c23",
        "-D_GNU_SOURCE",
    };

    var c_flags: std.ArrayList([]const u8) = .empty;
    defer c_flags.deinit(b.allocator);

    const oom = struct {
        fn die(err: anyerror) noreturn {
            std.debug.panic("build allocation failed: {s}", .{@errorName(err)});
        }
    }.die;
    c_flags.appendSlice(b.allocator, &base_flags) catch |err| oom(err);
    if (strict) {
        c_flags.appendSlice(b.allocator, &.{
            "-Werror",
            "-Wpedantic",
            "-Wshadow",
        }) catch |err| oom(err);
    }
    if (harden) {
        c_flags.appendSlice(b.allocator, &.{
            "-fstack-protector-strong",
            "-D_FORTIFY_SOURCE=2",
        }) catch |err| oom(err);
    }
    c_flags.append(b.allocator, cStringDefine(b, "APP_VERSION", version_str)) catch |err| oom(err);
    c_flags.append(b.allocator, cStringDefine(b, "APP_NAME", app_name)) catch |err| oom(err);
    c_flags.append(b.allocator, cStringDefine(b, "APP_GIT_COMMIT", git_commit)) catch |err| oom(err);
    c_flags.append(b.allocator, cStringDefine(b, "APP_BUILD_DATE", build_date)) catch |err| oom(err);

    if (enable_tui) {
        c_flags.append(b.allocator, "-DENABLE_TUI=1") catch |err| oom(err);
    }

    if (enable_cli_style) {
        c_flags.append(b.allocator, "-DAPP_ENABLE_CLI_STYLE=1") catch |err| oom(err);
    }
    c_flags.append(b.allocator, b.fmt("-DAPP_HAVE_TERMINFO={d}", .{@intFromBool(have_terminfo)})) catch |err| oom(err);

    // CLI styling sources (shared tokens + cli/style renderers). The terminal
    // backend is chosen at build time: terminfo when available, else ANSI.
    const cli_style_sources = [_][]const u8{
        "src/style/color_math.c",
        "src/style/design_tokens.c",
        "src/cli/style/cli_term.c",
        "src/cli/style/cli_term_osc11.c",
        "src/cli/style/cli_theme.c",
        "src/cli/style/cli_sgr.c",
        "src/cli/style/cli_layout.c",
        "src/cli/style/cli_help_render.c",
        "src/cli/style/cli_error_render.c",
        "src/cli/style/cli_version_render.c",
    };

    exe.root_module.addCSourceFiles(.{
        .files = &base_sources,
        .flags = c_flags.items,
    });

    if (enable_cli_style) {
        exe.root_module.addCSourceFiles(.{
            .files = &cli_style_sources,
            .flags = c_flags.items,
        });
        const backend = if (have_terminfo)
            "src/cli/style/cli_term_terminfo.c"
        else
            "src/cli/style/cli_term_ansi.c";
        exe.root_module.addCSourceFiles(.{
            .files = &.{backend},
            .flags = c_flags.items,
        });
        // The terminfo backend needs the curses/terminfo library + headers. If
        // the TUI is also enabled it already links curses below; avoid linking
        // twice. Link the exact library detection chose (`terminfo_lib`) rather
        // than assuming `ncursesw`: a host may only ship `tinfo` or `ncurses`.
        if (have_terminfo and !enable_tui) {
            linkCurses(exe.root_module, target, curses_prefix, b, terminfo_lib);
        }
    }

    // Add TUI source if enabled
    if (enable_tui) {
        // TUI sources
        const tui_sources = [_][]const u8{
            "src/tui/tui.c",
            "src/tui/tui_app.c",
            "src/tui/tui_menu.c",
            "src/tui/tui_menu_model.c",
            "src/tui/tui_progress.c",
        };

        exe.root_module.addCSourceFiles(.{
            .files = &tui_sources,
            .flags = c_flags.items,
        });
    }

    if (enable_tui) {
        // The TUI needs the full curses screen API (ncursesw / pdcurses), so
        // link the platform default. When the CLI-style terminfo backend is
        // also compiled and detection chose a *different* library (e.g. `tinfo`
        // or plain `ncurses`), additionally link that exact name so its
        // terminfo symbols resolve too.
        linkCurses(exe.root_module, target, curses_prefix, b, null);
        const default_curses = if (target.result.os.tag == .windows) "pdcurses" else "ncursesw";
        if (have_terminfo) {
            if (terminfo_lib) |name| {
                if (!std.mem.eql(u8, name, default_curses)) {
                    exe.root_module.linkSystemLibrary(name, .{});
                }
            }
        }
    }

    b.installArtifact(exe);

    // Narrow reusable primitive: a static library for the TUI lifecycle and
    // modal menu, without the demo app or CLI command policy.
    var tui_menu_lib_flags: std.ArrayList([]const u8) = .empty;
    defer tui_menu_lib_flags.deinit(b.allocator);
    tui_menu_lib_flags.appendSlice(b.allocator, &base_flags) catch |err| oom(err);
    tui_menu_lib_flags.append(b.allocator, "-DENABLE_TUI=1") catch |err| oom(err);

    const tui_menu_lib = b.addLibrary(.{
        .name = "tui-menu",
        .linkage = .static,
        .root_module = b.createModule(.{
            .root_source_file = null,
            .target = target,
            .optimize = optimize,
            .link_libc = true,
        }),
    });
    tui_menu_lib.root_module.addIncludePath(b.path("src"));
    tui_menu_lib.root_module.addCSourceFiles(.{
        .files = &.{
            "src/core/error.c",
            "src/utils/logging.c",
            // Shared design palette: tui.c seeds its truecolor entries from
            // APP_DESIGN_PALETTE, so the token definition must be linked in.
            "src/style/design_tokens.c",
            "src/tui/tui.c",
            "src/tui/tui_menu.c",
            "src/tui/tui_menu_model.c",
            "src/tui/tui_progress.c",
        },
        .flags = tui_menu_lib_flags.items,
    });
    linkCurses(tui_menu_lib.root_module, target, curses_prefix, b, null);

    const install_tui_menu_lib = b.addInstallArtifact(tui_menu_lib, .{});
    const install_tui_headers = [_]*std.Build.Step.InstallFile{
        b.addInstallFile(b.path("src/core/error.h"), "include/c23-cli-template/core/error.h"),
        b.addInstallFile(b.path("src/core/types.h"), "include/c23-cli-template/core/types.h"),
        b.addInstallFile(b.path("src/tui/tui.h"), "include/c23-cli-template/tui/tui.h"),
        b.addInstallFile(b.path("src/tui/tui_menu.h"), "include/c23-cli-template/tui/tui_menu.h"),
        b.addInstallFile(b.path("src/tui/tui_progress.h"), "include/c23-cli-template/tui/tui_progress.h"),
    };
    const tui_menu_lib_step = b.step("tui-menu-lib", "Build and install the reusable TUI menu static library");
    tui_menu_lib_step.dependOn(&install_tui_menu_lib.step);
    for (install_tui_headers) |install_header| {
        tui_menu_lib_step.dependOn(&install_header.step);
    }

    // Run command
    const run_cmd = b.addRunArtifact(exe);
    run_cmd.step.dependOn(b.getInstallStep());
    if (b.args) |args| {
        run_cmd.addArgs(args);
    }

    const run_step = b.step("run", "Run the application");
    run_step.dependOn(&run_cmd.step);

    // Test command
    const test_exe = b.addExecutable(.{
        .name = "cli-contract-tests",
        .root_module = b.createModule(.{
            .root_source_file = null,
            .target = target,
            .optimize = optimize,
            .link_libc = true,
        }),
    });
    test_exe.root_module.addIncludePath(b.path("test"));
    test_exe.root_module.addCSourceFiles(.{
        .files = &.{
            "test/cli_contract_runner.c",
            "test/cli_contract_helpers.c",
            "test/cli_contract_cases.c",
        },
        .flags = &base_flags,
    });
    const installed_binary_path = b.getInstallPath(.bin, exe.out_filename);

    const test_cmd = b.addRunArtifact(test_exe);
    test_cmd.addArgs(&.{ "--binary", installed_binary_path });
    test_cmd.step.dependOn(b.getInstallStep());

    const test_step = b.step("test", "Run test suite");
    test_step.dependOn(&test_cmd.step);

    // Unit tests link against a subset of the production sources so they can
    // exercise pieces (error.c, config_json.c, memory.c) directly instead of
    // through a subprocess.
    const unit_exe = b.addExecutable(.{
        .name = "unit-tests",
        .root_module = b.createModule(.{
            .root_source_file = null,
            .target = target,
            .optimize = optimize,
            .link_libc = true,
        }),
    });
    unit_exe.root_module.addIncludePath(b.path("src"));
    unit_exe.root_module.addCSourceFiles(.{
        .files = &.{
            "test/unit_runner.c",
            "test/unit_config_tests.c",
            "test/unit_input_tests.c",
            "test/unit_tui_menu_tests.c",
            "test/unit_cli_style_tests.c",
            "test/unit_cli_osc11_tests.c",
            "src/core/error.c",
            "src/core/config.c",
            "src/core/config_json.c",
            "src/io/input.c",
            "src/tui/tui_menu_model.c",
            "src/utils/colors.c",
            "src/utils/memory.c",
            "src/utils/logging.c",
            // CLI styling layer (ANSI backend: no ncurses link needed).
            "src/style/color_math.c",
            "src/style/design_tokens.c",
            "src/cli/style/cli_term.c",
            "src/cli/style/cli_term_osc11.c",
            "src/cli/style/cli_term_ansi.c",
            "src/cli/style/cli_theme.c",
            "src/cli/style/cli_sgr.c",
            "src/cli/style/cli_layout.c",
            "src/cli/style/cli_error_render.c",
        },
        .flags = c_flags.items,
    });
    const unit_cmd = b.addRunArtifact(unit_exe);
    const unit_step = b.step("unit-test", "Run in-process unit tests");
    unit_step.dependOn(&unit_cmd.step);
    test_step.dependOn(&unit_cmd.step);

    const terminal_test_plan = resolveTerminalTestPlan(b, enable_tui, terminal_backend, target, ghostty_vt_prefix);
    if (terminal_test_plan == .fail) {
        std.log.err("{s}", .{terminal_test_plan.fail});
        std.process.exit(1);
    }
    const terminal_test_step = b.step("terminal-test", "Run CLI contracts and optional PTY/TUI terminal scenarios");
    terminal_test_step.dependOn(test_step);
    if (terminal_test_plan == .run_ghostty) {
        const ghostty_api = terminal_test_plan.run_ghostty;
        const vt_test_mod = b.createModule(.{
            .root_source_file = null,
            .target = target,
            .optimize = optimize,
            .link_libc = true,
        });
        vt_test_mod.addIncludePath(b.path("test"));
        var vt_test_flags: std.ArrayList([]const u8) = .empty;
        defer vt_test_flags.deinit(b.allocator);
        vt_test_flags.appendSlice(b.allocator, &base_flags) catch |err| oom(err);
        vt_test_flags.append(b.allocator, b.fmt("-DAPP_NAME=\"{s}\"", .{app_name})) catch |err| oom(err);

        vt_test_mod.addCSourceFiles(.{
            .files = &.{
                "test/terminal_vt_common.c",
                "test/terminal_vt_session.c",
                "test/terminal_vt_scenarios.c",
                "test/terminal_vt_runner.c",
            },
            .flags = vt_test_flags.items,
        });

        if (ghostty_vt_prefix) |pref| {
            const lib_dir = ghostty_api.prefix_lib_dir.?;
            vt_test_mod.addIncludePath(.{ .cwd_relative = b.fmt("{s}/include", .{pref}) });
            vt_test_mod.addLibraryPath(.{ .cwd_relative = lib_dir });
            vt_test_mod.addRPath(.{ .cwd_relative = lib_dir });
            vt_test_mod.linkSystemLibrary("ghostty-vt", .{ .use_pkg_config = .no });
        } else {
            if (ghostty_api.pkg_lib_dir) |lib_dir| {
                vt_test_mod.addRPath(.{ .cwd_relative = lib_dir });
            }
            vt_test_mod.linkSystemLibrary("libghostty-vt", .{});
        }

        const vt_test_exe = b.addExecutable(.{
            .name = "terminal-vt-tests",
            .root_module = vt_test_mod,
        });

        const vt_test_cmd = b.addRunArtifact(vt_test_exe);
        vt_test_cmd.addArgs(&.{
            "--binary",
            installed_binary_path,
            "--tui-enabled",
            "auto",
        });
        if (ghostty_api.prefix_lib_dir) |lib_dir| {
            prependRunEnvPath(vt_test_cmd, "LD_LIBRARY_PATH", lib_dir);
            prependRunEnvPath(vt_test_cmd, "DYLD_FALLBACK_LIBRARY_PATH", lib_dir);
        } else if (ghostty_api.pkg_lib_dir) |lib_dir| {
            prependRunEnvPath(vt_test_cmd, "LD_LIBRARY_PATH", lib_dir);
            prependRunEnvPath(vt_test_cmd, "DYLD_FALLBACK_LIBRARY_PATH", lib_dir);
        }
        vt_test_cmd.step.dependOn(b.getInstallStep());
        terminal_test_step.dependOn(&vt_test_cmd.step);
    } else if (terminal_test_plan == .skip) {
        const skip_cmd = addMessageCommand(b, terminal_test_plan.skip);
        terminal_test_step.dependOn(&skip_cmd.step);
    }

    // Clean command – cross-platform
    const clean_cmd = if (target.result.os.tag == .windows)
        b.addSystemCommand(&.{ "cmd", "/C", "if exist zig-out rmdir /S /Q zig-out & if exist .zig-cache rmdir /S /Q .zig-cache" })
    else
        b.addSystemCommand(&.{ "rm", "-rf", "zig-out", ".zig-cache" });
    const clean_step = b.step("clean", "Clean build artifacts");
    clean_step.dependOn(&clean_cmd.step);

    // Format commands
    const fmt_step = b.step("fmt", "Format all source files");
    const fmt = b.addFmt(.{
        .paths = &.{ "build.zig", "src", "test" },
        .check = false,
    });
    fmt_step.dependOn(&fmt.step);

    const fmt_check_step = b.step("fmt-check", "Check source formatting");
    const fmt_check = b.addFmt(.{
        .paths = &.{ "build.zig", "src", "test" },
        .check = true,
    });
    fmt_check_step.dependOn(&fmt_check.step);

    // Check command
    const check_step = b.step("check", "Run baseline checks");
    check_step.dependOn(fmt_check_step);
    check_step.dependOn(test_step);
}
