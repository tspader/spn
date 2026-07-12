SPN_TEST_SUITE(layout)

UTEST_F(layout, staged_bin) {
  tmpfs_init_named(&uf->fixture.fs, "layout_staged_bin");

  run_command_test(utest_result, &uf->fixture, (command_test_t) {
    .project = "test/integration/fixtures/layout/test_shared",
    .copy = { "check.c", "packages/*" },
    .args = { "build" },
    .expect = {
      .bin.name = "main",
      .exists = { exe("main"), staged_lib("spum"), store_file("bin/main") },
    },
  });
}

UTEST_F(layout, staged_test) {
  tmpfs_init_named(&uf->fixture.fs, "layout_staged_test");

  run_command_test(utest_result, &uf->fixture, (command_test_t) {
    .project = "test/integration/fixtures/layout/test_shared",
    .copy = { "check.c", "packages/*" },
    .args = { "build" },
    .expect = {
      .bin.path = test_exe("check"),
      .exists = { test_exe("check"), test_lib("spum") },
    },
  });
}

UTEST_F(layout, staged_script) {
  tmpfs_init_named(&uf->fixture.fs, "layout_staged_script");

  run_command_test(utest_result, &uf->fixture, (command_test_t) {
    .project = "test/integration/fixtures/run/manifest",
    .args = { "build", "main" },
    .expect.exists = { exe("main") },
  });
}

UTEST_F(layout, script_ctx_footprint) {
  tmpfs_init_named(&uf->fixture.fs, "layout_script_ctx");

  run_command_test(utest_result, &uf->fixture, (command_test_t) {
    .project = "test/integration/fixtures/script/default_script",
    .args = { "build" },
    .expect = {
      .exists = { sp_str_lit("build/script/work/default_script/spn/configure.wasm") },
      .missing = { sp_str_lit("build/script/store") },
    },
  });
}

UTEST_F(layout, reserved_bin_name) {
  tmpfs_init_named(&uf->fixture.fs, "layout_reserved_bin_name");

  run_command_test(utest_result, &uf->fixture, (command_test_t) {
    .project = "test/integration/fixtures/layout/reserved_bin",
    .args = { "build" },
    .expect.rc = 1,
  });
}

UTEST_F(layout, target_triple) {
  tmpfs_init_named(&uf->fixture.fs, "layout_target_triple");

  run_command_test(utest_result, &uf->fixture, (command_test_t) {
    .project = "test/integration/fixtures/layout/test_shared",
    .copy = { "check.c", "packages/*" },
    .args = { "build", "--target", SPN_TEST_TRIPLE },
    .expect = {
      .exists = { target_exe("main", SPN_TEST_TRIPLE), target_store_file("bin/main", SPN_TEST_TRIPLE) },
      .missing = { sp_str_lit("build/debug") },
    },
  });
}
