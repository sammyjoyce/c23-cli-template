const std = @import("std");
const testing = std.testing;

test "installed binary starts" {
    const allocator = testing.allocator;
    const binary_path = "./zig-out/bin/myapp";

    const file = try std.Io.Dir.cwd().openFile(testing.io, binary_path, .{});
    file.close(testing.io);

    const result = try std.process.run(allocator, testing.io, .{ .argv = &.{ binary_path, "--version" } });
    defer allocator.free(result.stdout);
    defer allocator.free(result.stderr);

    try testing.expect(exitedWith(result, 0));
    try testing.expect(result.stdout.len > 0);
}

fn exitedWith(result: std.process.RunResult, code: u8) bool {
    return switch (result.term) {
        .exited => |status| status == code,
        else => false,
    };
}
