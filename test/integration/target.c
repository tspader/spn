SPN_TEST_SUITE(target)

UTEST_F(target, static_lib) {
  tmpfs_init_named(&uf->fixture.fs, "target_static_lib");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/target/static_lib",
    .copy = { "mylib.c" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .args = { "mylib" } } },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = sp_str_lit("build/debug/store/lib/libmylib.a") },
    },
  });
}

UTEST_F(target, shared_lib) {
  tmpfs_init_named(&uf->fixture.fs, "target_shared_lib");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/target/shared_lib",
    .copy = { "spum.c" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .args = { "spum" } } },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = sp_str_lit("build/debug/store/lib/libspum.so") },
    },
  });
}

UTEST_F(target, source_glob) {
  tmpfs_init_named(&uf->fixture.fs, "target_source_glob");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/target/source_glob",
    .copy = { "src" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_RUN_BIN, .bin.name = "main" },
    },
  });
}

UTEST_F(target, publish) {
  tmpfs_init_named(&uf->fixture.fs, "target_publish");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/target/publish",
    .copy = { "packages/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_INCLUDE, .verify_include.file = sp_str_lit("kit.h") },
      { .kind = ACTION_VERIFY_INCLUDE, .verify_include.file = sp_str_lit("kit/a.h") },
      { .kind = ACTION_VERIFY_INCLUDE, .verify_include.file = sp_str_lit("kit/b.h") },
      { .kind = ACTION_RUN_BIN, .bin.name = "publish" },
    },
  });
}

UTEST_F(target, system_deps) {
  tmpfs_init_named(&uf->fixture.fs, "target_system_deps");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/target/system_deps",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = sp_str_lit("spn.lock") },
      { .kind = ACTION_RUN_BIN, .bin.name = "main" },
      { .kind = ACTION_RUN_CLI, .cli = { "build", .args = { "--force" } } },
      { .kind = ACTION_RUN_BIN, .bin.name = "main" },
    },
  });
}

UTEST_F(target, source_pin) {
  tmpfs_init_named(&uf->fixture.fs, "target_source_pin");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/target/source_pin",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_RUN_BIN, .bin = { .name = "main", .rc = 0 } },
    },
  });
}
