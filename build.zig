const std = @import("std");

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
    const test_exe = b.addTest(.{
        .root_module = b.createModule(.{
            .root_source_file = b.path("test/main.zig"),
            .target = target,
            .optimize = optimize,
        }),
    });

    const test_cmd = b.addRunArtifact(test_exe);
    test_cmd.step.dependOn(b.getInstallStep());

    const test_step = b.step("test", "Run test suite");
    test_step.dependOn(&test_cmd.step);

    const python = b.findProgram(&.{ "python3", "python" }, &.{}) catch "python3";
    const terminal_test_cmd = b.addSystemCommand(&.{ python, "test/run_terminal_tests.py" });
    const installed_binary_path = b.getInstallPath(.bin, exe.out_filename);
    terminal_test_cmd.addArg("--binary");
    terminal_test_cmd.addArg(installed_binary_path);
    terminal_test_cmd.addArg("--tui-enabled");
    terminal_test_cmd.addArg(if (enable_tui) "1" else "0");
    terminal_test_cmd.setEnvironmentVariable("APP_BINARY", installed_binary_path);
    terminal_test_cmd.setEnvironmentVariable("APP_TUI_ENABLED", if (enable_tui) "1" else "0");
    terminal_test_cmd.step.dependOn(b.getInstallStep());

    const terminal_test_step = b.step("terminal-test", "Run end-to-end CLI/TUI terminal scenario tests");
    terminal_test_step.dependOn(&terminal_test_cmd.step);

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
    const check_step = b.step("check", "Run all checks");
    check_step.dependOn(fmt_check_step);
    check_step.dependOn(test_step);
    check_step.dependOn(terminal_test_step);
}
