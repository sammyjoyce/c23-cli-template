const std = @import("std");
const testing = std.testing;

test "application test suite" {
    std.debug.print("\n🧪 Application Test Suite\n", .{});
    std.debug.print("========================\n\n", .{});

    // Ensure binary is built
    const allocator = testing.allocator;
    const binary_path = "./zig-out/bin/myapp";
    const file = std.Io.Dir.cwd().openFile(testing.io, binary_path, .{}) catch {
        std.debug.print("📦 Building application binary...\n", .{});
        try runBuild(allocator);
        std.debug.print("✓ Build complete\n\n", .{});
        return;
    };
    file.close(testing.io);

    std.debug.print("Running all tests...\n\n", .{});

    // Run basic functionality tests
    try testHelp(allocator);
    try testVersion(allocator);
    try testCommands(allocator);
    try testInvalidCommand(allocator);

    // Note: TUI menu command is not tested here as it requires
    // an interactive terminal. It should be tested manually.
}

fn runBuild(allocator: std.mem.Allocator) !void {
    const result = try runCommand(allocator, &.{ "zig", "build" });
    defer allocator.free(result.stdout);
    defer allocator.free(result.stderr);

    if (!exitedWith(result, 0)) {
        std.debug.print("Build failed:\n{s}\n", .{result.stderr});
        return error.BuildFailed;
    }
}

fn runCommand(allocator: std.mem.Allocator, argv: []const []const u8) !std.process.RunResult {
    return try std.process.run(allocator, testing.io, .{ .argv = argv });
}

fn runCommandWithEnv(
    allocator: std.mem.Allocator,
    argv: []const []const u8,
    environ_map: *const std.process.Environ.Map,
) !std.process.RunResult {
    return try std.process.run(allocator, testing.io, .{ .argv = argv, .environ_map = environ_map });
}

fn exitedWith(result: std.process.RunResult, code: u8) bool {
    return switch (result.term) {
        .exited => |status| status == code,
        else => false,
    };
}

fn testHelp(allocator: std.mem.Allocator) !void {
    std.debug.print("Testing --help flag...\n", .{});

    const result = try runCommand(allocator, &.{ "./zig-out/bin/myapp", "--help" });
    defer allocator.free(result.stdout);
    defer allocator.free(result.stderr);

    try testing.expect(exitedWith(result, 0));
    try testing.expect(result.stdout.len > 0);
    try testing.expect(std.mem.indexOf(u8, result.stdout, "USAGE") != null);

    std.debug.print("✓ Help flag works correctly\n", .{});
}

fn testVersion(allocator: std.mem.Allocator) !void {
    std.debug.print("Testing --version flag...\n", .{});

    const result = try runCommand(allocator, &.{ "./zig-out/bin/myapp", "--version" });
    defer allocator.free(result.stdout);
    defer allocator.free(result.stderr);

    try testing.expect(exitedWith(result, 0));
    try testing.expect(result.stdout.len > 0);

    std.debug.print("✓ Version flag works correctly\n", .{});
}

fn testCommands(allocator: std.mem.Allocator) !void {
    std.debug.print("Testing built-in commands...\n", .{});

    // Test hello command
    {
        const result = try runCommand(allocator, &.{ "./zig-out/bin/myapp", "hello" });
        defer allocator.free(result.stdout);
        defer allocator.free(result.stderr);

        try testing.expect(exitedWith(result, 0));
        try testing.expect(std.mem.indexOf(u8, result.stdout, "Hello, World!") != null);
    }

    // Test hello with name
    {
        const result = try runCommand(allocator, &.{ "./zig-out/bin/myapp", "hello", "Alice" });
        defer allocator.free(result.stdout);
        defer allocator.free(result.stderr);

        try testing.expect(exitedWith(result, 0));
        try testing.expect(std.mem.indexOf(u8, result.stdout, "Hello, Alice!") != null);
    }

    // Test echo command
    {
        const result = try runCommand(allocator, &.{ "./zig-out/bin/myapp", "echo", "test", "message" });
        defer allocator.free(result.stdout);
        defer allocator.free(result.stderr);

        try testing.expect(exitedWith(result, 0));
        try testing.expect(std.mem.indexOf(u8, result.stdout, "test message") != null);
    }

    // Test info command
    {
        const result = try runCommand(allocator, &.{ "./zig-out/bin/myapp", "info" });
        defer allocator.free(result.stdout);
        defer allocator.free(result.stderr);

        try testing.expect(exitedWith(result, 0));
        try testing.expect(std.mem.indexOf(u8, result.stdout, "Application:") != null);
        try testing.expect(std.mem.indexOf(u8, result.stdout, "Version:") != null);
    }

    // Test JSON info command
    {
        const result = try runCommand(allocator, &.{ "./zig-out/bin/myapp", "--json", "info" });
        defer allocator.free(result.stdout);
        defer allocator.free(result.stderr);

        try testing.expect(exitedWith(result, 0));
        try testing.expect(std.mem.indexOf(u8, result.stdout, "\"format_version\":\"1.0\"") != null);
        try testing.expect(std.mem.indexOf(u8, result.stdout, "\"features\"") != null);
    }

    // Test quiet JSON info command
    {
        const result = try runCommand(allocator, &.{ "./zig-out/bin/myapp", "--quiet", "--json", "info" });
        defer allocator.free(result.stdout);
        defer allocator.free(result.stderr);

        try testing.expect(exitedWith(result, 0));
        try testing.expectEqual(@as(usize, 0), result.stdout.len);
    }

    // Test doctor command
    {
        const result = try runCommand(allocator, &.{ "./zig-out/bin/myapp", "doctor" });
        defer allocator.free(result.stdout);
        defer allocator.free(result.stderr);

        try testing.expect(exitedWith(result, 0));
        try testing.expect(std.mem.indexOf(u8, result.stdout, "doctor") != null);
        try testing.expect(std.mem.indexOf(u8, result.stdout, "binary") != null);
    }

    // Test quiet JSON doctor command
    {
        const result = try runCommand(allocator, &.{ "./zig-out/bin/myapp", "--quiet", "--json", "doctor" });
        defer allocator.free(result.stdout);
        defer allocator.free(result.stderr);

        try testing.expect(exitedWith(result, 0));
        try testing.expectEqual(@as(usize, 0), result.stdout.len);
    }

    // Test command arguments do not get parsed as global config options
    {
        const result = try runCommand(allocator, &.{ "./zig-out/bin/myapp", "echo", "-c", "/definitely/not/a/config.json" });
        defer allocator.free(result.stdout);
        defer allocator.free(result.stderr);

        try testing.expect(exitedWith(result, 0));
        try testing.expect(std.mem.indexOf(u8, result.stdout, "-c /definitely/not/a/config.json") != null);
    }

    // Test verbose mode enables informational diagnostics on stderr
    {
        const result = try runCommand(allocator, &.{ "./zig-out/bin/myapp", "--verbose", "hello" });
        defer allocator.free(result.stdout);
        defer allocator.free(result.stderr);

        try testing.expect(exitedWith(result, 0));
        try testing.expect(std.mem.indexOf(u8, result.stdout, "Hello, World!") != null);
        try testing.expect(std.mem.indexOf(u8, result.stderr, "[INFO]") != null);
    }

    // Test invalid default config files do not leak partial settings
    {
        var tmp = testing.tmpDir(.{});
        defer tmp.cleanup();

        try tmp.dir.writeFile(testing.io, .{
            .sub_path = "config.json",
            .data = "{\"quiet\":true,",
        });

        const config_path = try std.fmt.allocPrint(
            allocator,
            ".zig-cache/tmp/{s}/config.json",
            .{&tmp.sub_path},
        );
        defer allocator.free(config_path);

        var env = std.process.Environ.Map.init(allocator);
        defer env.deinit();
        try env.put("APP_CONFIG_PATH", config_path);

        const result = try runCommandWithEnv(allocator, &.{ "./zig-out/bin/myapp", "hello" }, &env);
        defer allocator.free(result.stdout);
        defer allocator.free(result.stderr);

        try testing.expect(exitedWith(result, 0));
        try testing.expect(std.mem.indexOf(u8, result.stdout, "Hello, World!") != null);
    }

    std.debug.print("✓ All commands work correctly\n", .{});
}

fn testInvalidCommand(allocator: std.mem.Allocator) !void {
    std.debug.print("Testing invalid command handling...\n", .{});

    const result = try runCommand(allocator, &.{ "./zig-out/bin/myapp", "invalid" });
    defer allocator.free(result.stdout);
    defer allocator.free(result.stderr);

    try testing.expect(!exitedWith(result, 0));
    try testing.expect(std.mem.indexOf(u8, result.stderr, "Unknown command") != null);

    std.debug.print("✓ Invalid command handling works correctly\n", .{});
}
