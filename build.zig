const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const version_str = "1.0.0";
    const app_name = "myapp";
    const binary_name = app_name;

    // Attempt to inject current git commit hash, fall back to "unknown"
    var git_commit_buf: [32]u8 = undefined;
    const git_commit = blk: {
        const child_res = std.process.Child.run(.{
            .allocator = b.allocator,
            .argv = &.{ "git", "rev-parse", "--short", "HEAD" },
            .max_output_bytes = 64,
        }) catch break :blk "unknown";
        if (child_res.stdout.len == 0) break :blk "unknown";
        const trimmed = std.mem.trim(u8, child_res.stdout, "\n");
        if (trimmed.len > git_commit_buf.len) break :blk "unknown";
        @memcpy(git_commit_buf[0..trimmed.len], trimmed);
        break :blk git_commit_buf[0..trimmed.len];
    };

    // Build options
    // Build options – TUI disabled by default so fresh clones build everywhere
    const enable_tui = b.option(bool, "enable-tui", "Enable TUI support with ncurses (default: false)") orelse false;
    const ncurses_prefix = b.option([]const u8, "ncurses-prefix", "Override ncurses prefix (e.g. /usr/local/opt/ncurses)");

    const aro_dep = b.dependency("aro", .{
        .target = target,
        .optimize = optimize,
    });

    const exe = b.addExecutable(.{
        .name = binary_name,
        .root_module = b.createModule(.{
            .root_source_file = null,
            .target = target,
            .optimize = optimize,
        }),
    });
    // Ensure local headers are discoverable regardless of include style
    exe.addIncludePath(.{ .cwd_relative = "src" });

    // Base source files
    const base_sources = [_][]const u8{
        "src/main.c",
        "src/core/error.c",
        "src/core/config.c",
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
        "-D_GNU_SOURCE",
    };

    // Add base sources
    for (base_sources) |src| {
        var flags = std.ArrayList([]const u8).init(b.allocator);
        flags.appendSlice(&base_flags) catch @panic("OOM");
        flags.append(b.fmt("-DAPP_VERSION=\"{s}\"", .{version_str})) catch @panic("OOM");
        flags.append(b.fmt("-DAPP_NAME=\"{s}\"", .{app_name})) catch @panic("OOM");
        flags.append(b.fmt("-DAPP_GIT_COMMIT=\"{s}\"", .{git_commit})) catch @panic("OOM");
        flags.append("-DAPP_BUILD_DATE=\"reproducible\"") catch @panic("OOM");

        if (enable_tui) {
            flags.append("-DENABLE_TUI=1") catch @panic("OOM");
        }

        exe.addCSourceFile(.{
            .file = b.path(src),
            .flags = flags.items,
        });
        flags.deinit();
    }

    // Add TUI source if enabled
    if (enable_tui) {
        var flags = std.ArrayList([]const u8).init(b.allocator);
        flags.appendSlice(&base_flags) catch @panic("OOM");
        flags.append(b.fmt("-DAPP_VERSION=\"{s}\"", .{version_str})) catch @panic("OOM");
        flags.append(b.fmt("-DAPP_NAME=\"{s}\"", .{app_name})) catch @panic("OOM");
        flags.append(b.fmt("-DAPP_GIT_COMMIT=\"{s}\"", .{git_commit})) catch @panic("OOM");
        flags.append("-DAPP_BUILD_DATE=\"reproducible\"") catch @panic("OOM");
        flags.append("-DENABLE_TUI=1") catch @panic("OOM");

        // TUI sources
        const tui_sources = [_][]const u8{
            "src/tui/tui.c",
            "src/tui/tui_progress.c",
        };

        for (tui_sources) |src| {
            exe.addCSourceFile(.{
                .file = b.path(src),
                .flags = flags.items,
            });
        }
        flags.deinit();
    }

    exe.linkLibC();

    // NCurses / PDCurses configuration (only if TUI is enabled)
    if (enable_tui) {
        // Allow caller to override ncurses install prefix explicitly.
        if (ncurses_prefix) |pref| {
            exe.addIncludePath(.{ .cwd_relative = b.fmt("{s}/include", .{pref}) });
            exe.addLibraryPath(.{ .cwd_relative = b.fmt("{s}/lib", .{pref}) });
        }

        if (target.result.os.tag == .windows) {
            // Prefer PDCurses on Windows
            exe.linkSystemLibrary("pdcurses");
        } else {
            exe.linkSystemLibrary("ncurses");
        }
    }

    b.installArtifact(exe);

    // Install Aro headers
    b.installDirectory(.{
        .source_dir = aro_dep.path("include"),
        .install_dir = .prefix,
        .install_subdir = "include",
    });

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
}
