# Unslopify diff bundle — entire codebase

## git status --short --branch

```text
## main...origin/main
 M src/cli/args.h
 M src/core/config.c
 M src/core/config.h
 M src/io/output.h
 M src/tui/tui.h
 M src/tui/tui_progress.c
?? .pi/
```

## git diff --stat

```text
 src/cli/args.h         |  2 --
 src/core/config.c      | 32 +++++++++++++++++---------------
 src/core/config.h      |  6 +-----
 src/io/output.h        |  5 ++---
 src/tui/tui.h          |  5 -----
 src/tui/tui_progress.c |  9 ++++-----
 6 files changed, 24 insertions(+), 35 deletions(-)
```

## git diff -- source/header changes

```diff
diff --git a/src/cli/args.h b/src/cli/args.h
index 06b04dd..9903e31 100644
--- a/src/cli/args.h
+++ b/src/cli/args.h
@@ -12,9 +12,7 @@
 #include "../core/error.h"
 #include "../core/types.h"
 
-// Forward declaration avoids circular dependency with config.h.
 // The parser updates config based on parsed arguments.
-typedef struct app_config app_config_t;
 
 // Parse command line arguments and update configuration accordingly.
 // Returns APP_SUCCESS on success, or appropriate error code for invalid
diff --git a/src/core/config.c b/src/core/config.c
index 6fc6cf8..8e08129 100644
--- a/src/core/config.c
+++ b/src/core/config.c
@@ -36,6 +36,17 @@ struct app_config {
   bool no_color;
 };
 
+static void app_config_set_string(char **slot, const char *value) {
+  if (!slot || !value) {
+    return;
+  }
+
+  if (*slot) {
+    free(*slot);
+  }
+  *slot = strdup(value);
+}
+
 app_error app_config_create(app_config_t **config) {
   CHECK_NULL(config, APP_ERROR_INVALID_ARG);
 
@@ -290,20 +301,14 @@ void app_config_set_no_color(app_config_t *config, bool no_color) {
 }
 
 void app_config_set_program_name(app_config_t *config, const char *name) {
-  if (config && name) {
-    if (config->program_name) {
-      free(config->program_name);
-    }
-    config->program_name = strdup(name);
+  if (config) {
+    app_config_set_string(&config->program_name, name);
   }
 }
 
 void app_config_set_command(app_config_t *config, const char *command) {
-  if (config && command) {
-    if (config->command) {
-      free(config->command);
-    }
-    config->command = strdup(command);
+  if (config) {
+    app_config_set_string(&config->command, command);
   }
 }
 
@@ -314,10 +319,7 @@ void app_config_add_command_arg(app_config_t *config, const char *arg) {
 }
 
 void app_config_set_config_file(app_config_t *config, const char *path) {
-  if (config && path) {
-    if (config->config_file) {
-      free(config->config_file);
-    }
-    config->config_file = strdup(path);
+  if (config) {
+    app_config_set_string(&config->config_file, path);
   }
 }
diff --git a/src/core/config.h b/src/core/config.h
index 2709032..78a2f5d 100644
--- a/src/core/config.h
+++ b/src/core/config.h
@@ -14,11 +14,7 @@
 #include "error.h"
 #include "types.h"
 
-// Opaque configuration structure hides implementation details from callers.
-// This allows us to change the internal representation without breaking API
-// compatibility, and prevents direct manipulation that could leave config in
-// an inconsistent state.
-typedef struct app_config app_config_t;
+// The opaque app_config_t type is declared in types.h.
 
 // Create and destroy configuration objects with proper lifecycle management.
 // The create function allocates and initializes with defaults, while destroy
diff --git a/src/io/output.h b/src/io/output.h
index 19c94e1..65b03f6 100644
--- a/src/io/output.h
+++ b/src/io/output.h
@@ -14,9 +14,8 @@
 
 #include "../core/types.h"
 
-// Forward declaration prevents circular dependency with config.h.
-// Allows output module to respect config settings without tight coupling.
-typedef struct app_config app_config_t;
+// The app_config_t type comes from core/types.h so this header can respect
+// config settings without including config.h.
 
 // Output text with appropriate formatting based on configuration.
 // Handles JSON output mode, plain text, or colored output as configured.
diff --git a/src/tui/tui.h b/src/tui/tui.h
index 313bf2e..c827a23 100644
--- a/src/tui/tui.h
+++ b/src/tui/tui.h
@@ -15,11 +15,6 @@
 #include "../core/error.h"
 #include "../core/types.h"
 
-// Forward declaration for progress API
-struct tui_progress;
-
-typedef struct tui_progress tui_progress_t;
-
 // TUI color pairs
 typedef enum {
   TUI_COLOR_DEFAULT = 0,
diff --git a/src/tui/tui_progress.c b/src/tui/tui_progress.c
index 927aa3b..029efa8 100644
--- a/src/tui/tui_progress.c
+++ b/src/tui/tui_progress.c
@@ -17,15 +17,14 @@ struct tui_progress {
   char *title;
 };
 
-static void tui_progress_draw(const tui_progress_t *progress,
-                              const char *status) {
+static void tui_progress_draw(tui_progress_t *progress, const char *status) {
   if (!progress || !progress->window) {
     return;
   }
 
-  const tui_window_t *window = progress->window;
+  tui_window_t *window = progress->window;
   WINDOW *win = window->win;
-  tui_clear_window((tui_window_t *)window);  // Cast away const for ncurses API
+  tui_clear_window(window);
 
   // Draw title
   if (progress->title) {
@@ -61,7 +60,7 @@ static void tui_progress_draw(const tui_progress_t *progress,
     tui_print_centered(win, bar_y + 2, status);
   }
 
-  tui_refresh_window((tui_window_t *)window);
+  tui_refresh_window(window);
 }
 
 // Public API
```

## Notes
- `.pi/` contains Unslopify state/review artifacts and is intentionally untracked.
- No generated/vendor/build output directories are part of the cleanup diff.
