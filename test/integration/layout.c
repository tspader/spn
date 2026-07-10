SPN_TEST_SUITE(layout)

UTEST_F(layout, staged_bin) {
  tmpfs_init_named(&uf->fixture.fs, "layout_staged_bin");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/layout/test_shared",
    .copy = { "check.c", "packages/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = exe("main") },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = staged_lib("spum") },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = store_file("bin/main") },
      { .kind = ACTION_RUN_BIN, .bin.name = "main" },
    },
  });
}

UTEST_F(layout, staged_test) {
  tmpfs_init_named(&uf->fixture.fs, "layout_staged_test");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/layout/test_shared",
    .copy = { "check.c", "packages/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = test_exe("check") },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = test_lib("spum") },
      { .kind = ACTION_RUN_TEST, .bin.name = "check" },
    },
  });
}

UTEST_F(layout, staged_script) {
  tmpfs_init_named(&uf->fixture.fs, "layout_staged_script");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/run/manifest",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .args = { "main" } } },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = exe("main") },
    },
  });
}

UTEST_F(layout, reserved_bin_name) {
  tmpfs_init_named(&uf->fixture.fs, "layout_reserved_bin_name");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/layout/reserved_bin",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .rc = 1 } },
    },
  });
}

UTEST_F(layout, target_triple) {
  tmpfs_init_named(&uf->fixture.fs, "layout_target_triple");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/layout/test_shared",
    .copy = { "check.c", "packages/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .args = { "--target", SPN_TEST_TRIPLE } } },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = target_exe("main", SPN_TEST_TRIPLE) },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = target_store_file("bin/main", SPN_TEST_TRIPLE) },
      { .kind = ACTION_VERIFY_NOT_EXISTS, .verify_not_exists.file = sp_str_lit("build/debug") },
    },
  });
}
