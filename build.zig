const std = @import("std");

const TerminalTestBackend = enum {
    auto,
    ghostty,
};

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
    return commandSucceeds(b, &.{ "test", "-f", path });
}

fn ghosttyLibraryExists(b: *std.Build, lib_dir: []const u8) bool {
    return pathExists(b, b.fmt("{s}/libghostty-vt.so", .{lib_dir})) or
        pathExists(b, b.fmt("{s}/libghostty-vt.dylib", .{lib_dir})) or
        pathExists(b, b.fmt("{s}/libghostty-vt.a", .{lib_dir}));
}

fn hasGhosttyTerminalApi(b: *std.Build, prefix: ?[]const u8) bool {
    if (prefix) |pref| {
        return pathExists(b, b.fmt("{s}/include/ghostty/vt/terminal.h", .{pref})) and
            pathExists(b, b.fmt("{s}/include/ghostty/vt/formatter.h", .{pref})) and
            ghosttyLibraryExists(b, b.fmt("{s}/lib", .{pref}));
    }

    return commandSucceeds(b, &.{
        "sh",
        "-c",
        "pkg-config --exists libghostty-vt && " ++
            "inc=$(pkg-config --variable=includedir libghostty-vt) && " ++
            "lib=$(pkg-config --variable=libdir libghostty-vt) && " ++
            "test -f \"$inc/ghostty/vt/terminal.h\" && " ++
            "test -f \"$inc/ghostty/vt/formatter.h\" && " ++
            "{ test -f \"$lib/libghostty-vt.so\" || " ++
            "test -f \"$lib/libghostty-vt.dylib\" || " ++
            "test -f \"$lib/libghostty-vt.a\"; }",
    });
}

fn prependRunEnvPath(run: *std.Build.Step.Run, key: []const u8, dir: []const u8) void {
    const b = run.step.owner;
    const env_map = run.getEnvMap();

    if (env_map.get(key)) |current| {
        const value = if (current.len == 0)
            b.dupe(dir)
        else
            b.fmt("{s}{c}{s}", .{ dir, std.fs.path.delimiter, current });
        env_map.put(key, value) catch @panic("OOM");
    } else {
        env_map.put(key, b.dupe(dir)) catch @panic("OOM");
    }
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

    const enable_tui = b.option(bool, "enable-tui", "Enable TUI support with ncurses/PDCurses (default: false)") orelse false;
    const curses_prefix = b.option([]const u8, "curses-prefix", "Override ncurses/PDCurses prefix (e.g. /usr/local/opt/ncurses)");
    const terminal_backend = b.option(TerminalTestBackend, "terminal-backend", "Terminal test backend: auto or ghostty") orelse .auto;
    const ghostty_vt_prefix = b.option([]const u8, "ghostty-vt-prefix", "Override libghostty-vt install prefix for Ghostty-backed terminal tests");

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
    };

    // Base flags
    const base_flags = [_][]const u8{
        "-Wall",
        "-Wextra",
        "-std=c23",
        "-D_DEFAULT_SOURCE",
        "-D_XOPEN_SOURCE=700",
        "-D_GNU_SOURCE",
    };

    var c_flags: std.ArrayList([]const u8) = .empty;
    defer c_flags.deinit(b.allocator);

    c_flags.appendSlice(b.allocator, &base_flags) catch @panic("OOM");
    c_flags.append(b.allocator, b.fmt("-DAPP_VERSION=\"{s}\"", .{version_str})) catch @panic("OOM");
    c_flags.append(b.allocator, b.fmt("-DAPP_NAME=\"{s}\"", .{app_name})) catch @panic("OOM");
    c_flags.append(b.allocator, b.fmt("-DAPP_GIT_COMMIT=\"{s}\"", .{git_commit})) catch @panic("OOM");
    c_flags.append(b.allocator, "-DAPP_BUILD_DATE=\"reproducible\"") catch @panic("OOM");

    if (enable_tui) {
        c_flags.append(b.allocator, "-DENABLE_TUI=1") catch @panic("OOM");
    }

    exe.root_module.addCSourceFiles(.{
        .files = &base_sources,
        .flags = c_flags.items,
    });

    // Add TUI source if enabled
    if (enable_tui) {
        // TUI sources
        const tui_sources = [_][]const u8{
            "src/tui/tui.c",
            "src/tui/tui_demo.c",
            "src/tui/tui_progress.c",
        };

        exe.root_module.addCSourceFiles(.{
            .files = &tui_sources,
            .flags = c_flags.items,
        });
    }

    if (enable_tui) {
        // Allow caller to override ncurses install prefix explicitly.
        if (curses_prefix) |pref| {
            exe.root_module.addIncludePath(.{ .cwd_relative = b.fmt("{s}/include", .{pref}) });
            exe.root_module.addLibraryPath(.{ .cwd_relative = b.fmt("{s}/lib", .{pref}) });
        }

        if (target.result.os.tag == .windows) {
            exe.root_module.linkSystemLibrary("pdcurses", .{});
        } else {
            exe.root_module.linkSystemLibrary("ncursesw", .{});
        }
    }

    b.installArtifact(exe);

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
    test_exe.root_module.addCSourceFiles(.{
        .files = &.{"test/cli_contract_runner.c"},
        .flags = &base_flags,
    });
    const installed_binary_path = b.getInstallPath(.bin, exe.out_filename);

    const test_cmd = b.addRunArtifact(test_exe);
    test_cmd.addArgs(&.{ "--binary", installed_binary_path });
    test_cmd.step.dependOn(b.getInstallStep());

    const test_step = b.step("test", "Run test suite");
    test_step.dependOn(&test_cmd.step);

    if (terminal_backend == .ghostty and target.result.os.tag == .windows) {
        @panic("-Dterminal-backend=ghostty is not supported on Windows");
    }
    const have_ghostty_vt = hasGhosttyTerminalApi(b, ghostty_vt_prefix);
    if (terminal_backend == .ghostty and !have_ghostty_vt) {
        @panic("-Dterminal-backend=ghostty requires libghostty-vt with the terminal/formatter API via pkg-config or -Dghostty-vt-prefix=/path");
    }
    const ghostty_pkg_lib_dir = if (have_ghostty_vt and ghostty_vt_prefix == null)
        commandOutputTrimmed(b, &.{ "pkg-config", "--variable=libdir", "libghostty-vt" })
    else
        null;
    const use_ghostty_terminal_tests = target.result.os.tag != .windows and
        (terminal_backend == .ghostty or (terminal_backend == .auto and have_ghostty_vt));

    const terminal_test_step = b.step("terminal-test", "Run CLI contracts and optional PTY/TUI terminal scenarios");
    terminal_test_step.dependOn(test_step);
    if (use_ghostty_terminal_tests) {
        const vt_test_mod = b.createModule(.{
            .root_source_file = null,
            .target = target,
            .optimize = optimize,
            .link_libc = true,
        });
        vt_test_mod.addIncludePath(b.path("test"));
        vt_test_mod.addCSourceFiles(.{
            .files = &.{
                "test/terminal_vt_common.c",
                "test/terminal_vt_session.c",
                "test/terminal_vt_scenarios.c",
                "test/terminal_vt_runner.c",
            },
            .flags = &.{
                "-Wall",
                "-Wextra",
                "-std=c23",
                "-D_DEFAULT_SOURCE",
                "-D_XOPEN_SOURCE=700",
                "-D_GNU_SOURCE",
                b.fmt("-DAPP_NAME=\"{s}\"", .{app_name}),
            },
        });

        if (ghostty_vt_prefix) |pref| {
            vt_test_mod.addIncludePath(.{ .cwd_relative = b.fmt("{s}/include", .{pref}) });
            vt_test_mod.addLibraryPath(.{ .cwd_relative = b.fmt("{s}/lib", .{pref}) });
            vt_test_mod.addRPath(.{ .cwd_relative = b.fmt("{s}/lib", .{pref}) });
            vt_test_mod.linkSystemLibrary("ghostty-vt", .{ .use_pkg_config = .no });
        } else {
            if (ghostty_pkg_lib_dir) |lib_dir| {
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
            if (enable_tui) "1" else "0",
        });
        if (ghostty_vt_prefix) |pref| {
            const lib_path = b.fmt("{s}/lib", .{pref});
            prependRunEnvPath(vt_test_cmd, "LD_LIBRARY_PATH", lib_path);
            prependRunEnvPath(vt_test_cmd, "DYLD_FALLBACK_LIBRARY_PATH", lib_path);
        } else if (ghostty_pkg_lib_dir) |lib_dir| {
            prependRunEnvPath(vt_test_cmd, "LD_LIBRARY_PATH", lib_dir);
            prependRunEnvPath(vt_test_cmd, "DYLD_FALLBACK_LIBRARY_PATH", lib_dir);
        }
        vt_test_cmd.step.dependOn(b.getInstallStep());
        terminal_test_step.dependOn(&vt_test_cmd.step);
    }

    // Clean command – cross-platform
    const clean_cmd = if (target.result.os.tag == .windows)
        b.addSystemCommand(&.{ "cmd", "/C", "rmdir", "/S", "/Q", "zig-out", "&&", "rmdir", "/S", "/Q", ".zig-cache" })
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
