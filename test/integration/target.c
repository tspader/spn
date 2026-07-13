SPN_TEST_SUITE(target)

UTEST_F(target, static_lib) {
  tmpfs_init_named(&uf->fixture.fs, "target_static_lib");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/target/static_lib",
    .copy = { "mylib.c" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = store_file("lib/libmylib.a") },
    },
  });
}

UTEST_F(target, shared_lib) {
  tmpfs_init_named(&uf->fixture.fs, "target_shared_lib");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/target/shared_lib",
    .copy = { "spum.c" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = shared_lib("spum") },
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

UTEST_F(target, shared_source) {
  tmpfs_init_named(&uf->fixture.fs, "target_shared_source");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/target/shared_source",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_RUN_BIN, .bin.name = "main" },
    },
  });
}

UTEST_F(target, multiple_roots) {
  tmpfs_init_named(&uf->fixture.fs, "target_multiple_roots");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/target/shared_source",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .args = { "main", "test" } } },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = store_file("lib/libspum.a") },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = store_file("bin/main") },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = sp_str_lit("build/debug/test/test") },
    },
  });
}

UTEST_F(target, selection_default) {
  tmpfs_init_named(&uf->fixture.fs, "target_selection_default");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/target/selection",
    .copy = { "spum.c", "script.c" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = store_file("lib/libspum.a") },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = store_file("bin/main") },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = sp_str_lit("build/debug/test/test") },
      { .kind = ACTION_VERIFY_NOT_EXISTS, .verify_not_exists.file = store_file("bin/script") },
    },
  });
}

UTEST_F(target, selection_named_library) {
  tmpfs_init_named(&uf->fixture.fs, "target_selection_named_library");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/target/selection_libs",
    .copy = { "one.c", "two.c" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .args = { "one" } } },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = static_lib("one") },
      { .kind = ACTION_VERIFY_NOT_EXISTS, .verify_not_exists.file = static_lib("two") },
    },
  });
}

UTEST_F(target, selection_multiple_kinds) {
  tmpfs_init_named(&uf->fixture.fs, "target_selection_multiple_kinds");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/target/selection",
    .copy = { "spum.c", "script.c" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .args = { "--bin", "--test" } } },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = exe("main") },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = static_lib("spum") },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = sp_str_lit("build/debug/test/test") },
      { .kind = ACTION_VERIFY_NOT_EXISTS, .verify_not_exists.file = store_file("bin/script") },
    },
  });
}

UTEST_F(target, selection_name_respects_kind) {
  tmpfs_init_named(&uf->fixture.fs, "target_selection_name_respects_kind");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/target/selection",
    .copy = { "spum.c", "script.c" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .args = { "--lib", "main" }, .rc = 1 } },
      { .kind = ACTION_VERIFY_CLI_CONTAINS, .verify_cli.needle = sp_str_lit("is not defined for the selected target kinds") },
      { .kind = ACTION_VERIFY_NOT_EXISTS, .verify_not_exists.file = store_file("bin/main") },
    },
  });
}

UTEST_F(target, selection_test_command) {
  tmpfs_init_named(&uf->fixture.fs, "target_selection_test_command");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/target/selection",
    .copy = { "spum.c", "script.c" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "test", .args = { "test" } } },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = sp_str_lit("build/debug/test/test") },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = store_file("lib/libspum.a") },
      { .kind = ACTION_VERIFY_NOT_EXISTS, .verify_not_exists.file = store_file("bin/main") },
      { .kind = ACTION_VERIFY_NOT_EXISTS, .verify_not_exists.file = store_file("bin/script") },
    },
  });
}

UTEST_F(target, selection_run_command) {
  tmpfs_init_named(&uf->fixture.fs, "target_selection_run_command");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/target/selection",
    .copy = { "spum.c", "script.c" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "run", .args = { "script" } } },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = store_file("bin/script") },
      { .kind = ACTION_VERIFY_NOT_EXISTS, .verify_not_exists.file = store_file("bin/main") },
      { .kind = ACTION_VERIFY_NOT_EXISTS, .verify_not_exists.file = sp_str_lit("build/debug/test/test") },
      { .kind = ACTION_VERIFY_CONTENT, .verify_content = { .file = sp_str_lit("ran.txt"), .content = sp_str_lit("script\n") } },
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
