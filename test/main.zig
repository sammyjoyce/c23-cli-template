const std = @import("std");
const testing = std.testing;
const test_options = @import("test_options");

const allocator = testing.allocator;

const CommandResult = struct {
    term: std.process.Child.Term,
    stdout: []u8,
    stderr: []u8,

    fn deinit(self: *CommandResult) void {
        allocator.free(self.stdout);
        allocator.free(self.stderr);
    }
};

test "installed binary starts" {
    const binary_path = test_options.binary_path;

    const file = try std.Io.Dir.cwd().openFile(testing.io, binary_path, .{});
    file.close(testing.io);

    var result = try runCli(&.{"--version"}, null);
    defer result.deinit();

    try expectSuccess(&result);
    try testing.expect(result.stdout.len > 0);
}

test "help is human readable" {
    var result = try runCli(&.{"--help"}, null);
    defer result.deinit();

    try expectSuccess(&result);
    try expectStdoutContains(&result, "USAGE");
    try expectStdoutContains(&result, "COMMANDS");
    try expectStdoutContains(&result, "doctor");
}

test "builtins render expected output" {
    {
        var result = try runCli(&.{"hello"}, null);
        defer result.deinit();
        try expectSuccess(&result);
        try expectStdoutContains(&result, "Hello, World!");
    }

    {
        var result = try runCli(&.{ "hello", "Alice" }, null);
        defer result.deinit();
        try expectSuccess(&result);
        try expectStdoutContains(&result, "Hello, Alice!");
    }

    {
        var result = try runCli(&.{ "echo", "test", "message" }, null);
        defer result.deinit();
        try expectSuccess(&result);
        try expectStdoutContains(&result, "test message");
    }

    {
        var result = try runCli(&.{"info"}, null);
        defer result.deinit();
        try expectSuccess(&result);
        try expectStdoutContains(&result, "Application:");
        try expectStdoutContains(&result, "Version:");
    }
}

test "json info is versioned machine output" {
    var result = try runCli(&.{ "--json", "info" }, null);
    defer result.deinit();

    try expectSuccess(&result);
    try expectStdoutContains(&result, "\"format_version\":\"1.0\"");
    try expectStdoutContains(&result, "\"features\"");
    try expectStdoutContains(&result, "\"tui\"");
}

test "quiet json commands suppress stdout" {
    {
        var result = try runCli(&.{ "--quiet", "--json", "info" }, null);
        defer result.deinit();
        try expectSuccess(&result);
        try testing.expectEqualStrings("", result.stdout);
    }

    {
        var result = try runCli(&.{ "--quiet", "--json", "doctor" }, null);
        defer result.deinit();
        try expectSuccess(&result);
        try testing.expectEqualStrings("", result.stdout);
    }
}

test "doctor reports binary state" {
    var result = try runCli(&.{"doctor"}, null);
    defer result.deinit();

    try expectSuccess(&result);
    try expectStdoutContains(&result, "doctor");
    try expectStdoutContains(&result, "binary");
}

test "plain mode disables forced color" {
    var env = std.process.Environ.Map.init(allocator);
    defer env.deinit();
    try env.put("FORCE_COLOR", "1");

    var result = try runCli(&.{ "--plain", "doctor" }, &env);
    defer result.deinit();

    try expectSuccess(&result);
    try expectStdoutContains(&result, "color_output");
    try expectStdoutContains(&result, "disabled for this output");
}

test "command arguments are not global config flags" {
    var result = try runCli(&.{ "echo", "-c", "/definitely/not/a/config.json" }, null);
    defer result.deinit();

    try expectSuccess(&result);
    try expectStdoutContains(&result, "-c /definitely/not/a/config.json");
}

test "explicit config file failures are visible" {
    var result = try runCli(&.{ "--config", "/definitely/not/a/config.json", "hello" }, null);
    defer result.deinit();

    try expectNotSuccess(&result);
    try expectStderrContains(&result, "failed to load config");
    try expectStderrContains(&result, "/definitely/not/a/config.json");
}

test "verbose mode emits diagnostics on stderr" {
    var result = try runCli(&.{ "--verbose", "hello" }, null);
    defer result.deinit();

    try expectSuccess(&result);
    try expectStdoutContains(&result, "Hello, World!");
    try expectStderrContains(&result, "[INFO]");
}

test "invalid default config does not leak partial settings" {
    var tmp = testing.tmpDir(.{});
    defer tmp.cleanup();
    try tmp.dir.writeFile(testing.io, .{
        .sub_path = "config.json",
        .data = "{\"quiet\":true,\"ignored\":{\"nested\":true}}",
    });

    const config_path = try tmpPath("config.json", &tmp.sub_path);
    defer allocator.free(config_path);

    var env = std.process.Environ.Map.init(allocator);
    defer env.deinit();
    try env.put("APP_CONFIG_PATH", config_path);

    var result = try runCli(&.{"hello"}, &env);
    defer result.deinit();

    try expectSuccess(&result);
    try expectStdoutContains(&result, "Hello, World!");
}

test "valid flat config skips unknown scalar keys" {
    var tmp = testing.tmpDir(.{});
    defer tmp.cleanup();
    try tmp.dir.writeFile(testing.io, .{
        .sub_path = "config.json",
        .data = "{\"ignored\":\"debug\",\"quiet\":true}",
    });

    const config_path = try tmpPath("config.json", &tmp.sub_path);
    defer allocator.free(config_path);

    var result = try runCli(&.{ "--config", config_path, "hello" }, null);
    defer result.deinit();

    try expectSuccess(&result);
    try testing.expectEqualStrings("", result.stdout);
}

test "unknown command reports actionable error" {
    var result = try runCli(&.{"not-a-command"}, null);
    defer result.deinit();

    try expectNotSuccess(&result);
    try expectStderrContains(&result, "Unknown command: not-a-command");
    try expectStderrContains(&result, "--help");
}

fn runCli(args: []const []const u8, env: ?*const std.process.Environ.Map) !CommandResult {
    var argv = try allocator.alloc([]const u8, args.len + 1);
    defer allocator.free(argv);

    argv[0] = test_options.binary_path;
    @memcpy(argv[1..], args);

    const result = try std.process.run(allocator, testing.io, .{
        .argv = argv,
        .environ_map = env,
    });

    return .{
        .term = result.term,
        .stdout = result.stdout,
        .stderr = result.stderr,
    };
}

fn tmpPath(file_name: []const u8, sub_path: []const u8) ![]u8 {
    return std.fs.path.join(allocator, &.{ ".zig-cache", "tmp", sub_path, file_name });
}

fn expectSuccess(result: *const CommandResult) !void {
    if (!exitedWith(result.*, 0)) {
        std.debug.print("expected exit 0\nstdout:\n{s}\nstderr:\n{s}\n", .{
            result.stdout,
            result.stderr,
        });
        return error.ExpectedSuccess;
    }
}

fn expectNotSuccess(result: *const CommandResult) !void {
    if (exitedWith(result.*, 0)) {
        std.debug.print("expected non-zero exit\nstdout:\n{s}\nstderr:\n{s}\n", .{
            result.stdout,
            result.stderr,
        });
        return error.ExpectedFailure;
    }
}

fn expectStdoutContains(result: *const CommandResult, needle: []const u8) !void {
    try expectContains("stdout", result.stdout, needle);
}

fn expectStderrContains(result: *const CommandResult, needle: []const u8) !void {
    try expectContains("stderr", result.stderr, needle);
}

fn expectContains(label: []const u8, haystack: []const u8, needle: []const u8) !void {
    if (std.mem.indexOf(u8, haystack, needle) == null) {
        std.debug.print("expected {s} to contain: {s}\nactual {s}:\n{s}\n", .{
            label,
            needle,
            label,
            haystack,
        });
        return error.ExpectedSubstring;
    }
}

fn exitedWith(result: CommandResult, code: u8) bool {
    return switch (result.term) {
        .exited => |status| status == code,
        else => false,
    };
}
