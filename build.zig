const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const version_str = "1.0.0";
    const app_name = "myapp";
    const binary_name = app_name;
    const git_commit = "unknown";

    // Build options
    const enable_tui = b.option(bool, "enable-tui", "Enable TUI support with ncurses (default: true)") orelse true;

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

        if (enable_tui) {
            flags.append("-DENABLE_TUI=1") catch @panic("OOM");
        }

        exe.addCSourceFile(.{
            .file = b.path(src),
            .flags = flags.items,
        });
    }

    // Add TUI source if enabled
    if (enable_tui) {
        var flags = std.ArrayList([]const u8).init(b.allocator);
        flags.appendSlice(&base_flags) catch @panic("OOM");
        flags.append(b.fmt("-DAPP_VERSION=\"{s}\"", .{version_str})) catch @panic("OOM");
        flags.append(b.fmt("-DAPP_NAME=\"{s}\"", .{app_name})) catch @panic("OOM");
        flags.append(b.fmt("-DAPP_GIT_COMMIT=\"{s}\"", .{git_commit})) catch @panic("OOM");
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
    }

    exe.linkLibC();

    // NCurses configuration (only if TUI is enabled)
    if (enable_tui) {
        if (target.result.os.tag == .windows) {
            exe.linkSystemLibrary("pdcurses");
        } else {
            exe.linkSystemLibrary("ncurses");

            // Add common ncurses include paths for macOS
            if (target.result.os.tag == .macos) {
                exe.addIncludePath(.{ .cwd_relative = "/usr/local/opt/ncurses/include" });
                exe.addLibraryPath(.{ .cwd_relative = "/usr/local/opt/ncurses/lib" });
            }
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

    // Clean command
    const clean_cmd = b.addSystemCommand(&.{ "rm", "-rf", "zig-out", ".zig-cache" });
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
