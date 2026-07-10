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
      { .kind = ACTION_REMOVE_DIR, .rm = { .dir = "build" } },
      { .kind = ACTION_RUN_CLI, .cli = { "run", .args = { "main" } } },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = store_file("bin/main") },
      { .kind = ACTION_VERIFY_CONTENT, .verify_content = { .file = sp_str_lit("ran.txt"), .content = sp_str_lit("script\n") } },
    },
  });
}

UTEST_F(run, script_name_c) {
  tmpfs_init_named(&uf->fixture.fs, "run_script_name_c");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/run/script_name_c",
    .copy = { "script.c" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "run", .args = { "main.c" } } },
      { .kind = ACTION_VERIFY_CONTENT, .verify_content = { .file = sp_str_lit("ran.txt"), .content = sp_str_lit("script-name-c\n") } },
    },
  });
}
