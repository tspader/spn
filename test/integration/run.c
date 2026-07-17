#include "common.h"

SPN_TEST_SUITE(run)

UTEST_F(run, manifest) {
  tmpfs_init_named(&uf->fixture.fs, "run_manifest");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/run/manifest",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_VERIFY_NOT_EXISTS, .verify_not_exists.file = store_file("bin/main") },
      { .kind = ACTION_RUN_CLI, .cli = { "build", .args = { "main" } } },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = store_file("bin/main") },
    },
  });
}

UTEST_F(run, script_name_c) {
  tmpfs_init_named(&uf->fixture.fs, "run_script_name_c");

  run_command_test(utest_result, &uf->fixture, (command_test_t) {
    .project = "test/integration/fixtures/run/script_name_c",
    .copy = { "script.c" },
    .args = { "run", "main.c" },
    .expect.files = {
      { .file = sp_str_lit("ran.txt"), .content = "script-name-c\n" },
    },
  });
}
