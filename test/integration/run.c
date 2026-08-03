#include "harness.h"

sp_test(run, manifest) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/run/manifest",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_VERIFY_NOT_EXISTS, .exists = store_file("bin/main") },
      { .kind = ACTION_RUN_CLI, .cli = { "build", .args = { "main" } } },
      { .kind = ACTION_VERIFY_EXISTS, .exists = store_file("bin/main") },
    },
  });
}

sp_test(run, script_name_c) {
  return run_command_test(t, (command_test_t) {
    .project = "test/integration/fixtures/run/script_name_c",
    .copy = { "script.c" },
    .args = { "run", "main.c" },
    .expect.files = {
      { .file = sp_str_lit("ran.txt"), .content = "script-name-c\n" },
    },
  });
}
