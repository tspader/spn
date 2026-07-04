SPN_TEST_SUITE(consume)

UTEST_F(consume, static_lib) {
  tmpfs_init_named(&uf->fixture.fs, "consume_static");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/consume/static",
    .copy = { "packages/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .args = { "-p", "debug" } } },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = sp_str_lit("build/debug/store/lib/libspum.a") },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = sp_str_lit("build/debug/store/bin/main") },
    },
  });
}

UTEST_F(consume, static_lib_static_profile) {
  tmpfs_init_named(&uf->fixture.fs, "consume_static_static_profile");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/consume/static",
    .copy = { "packages/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .args = { "-p", "static" } } },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = sp_str_lit("build/static/store/lib/libspum.a") },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = sp_str_lit("build/static/store/bin/main") },
    },
  });
}

UTEST_F(consume, shared_lib) {
  tmpfs_init_named(&uf->fixture.fs, "consume_shared");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/consume/shared",
    .copy = { "packages/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .args = { "-p", "debug" } } },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = sp_str_lit("build/debug/store/lib/libspum.so") },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = sp_str_lit("build/debug/store/bin/main") },
    },
  });
}

UTEST_F(consume, shared_lib_static_profile) {
  tmpfs_init_named(&uf->fixture.fs, "consume_shared_static_profile");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/consume/shared",
    .copy = { "packages/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .args = { "-p", "static" }, .rc = 1 } },
    },
  });
}

UTEST_F(consume, source_lib) {
  tmpfs_init_named(&uf->fixture.fs, "consume_source");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/consume/source",
    .copy = { "packages/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .args = { "-p", "debug" } } },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = sp_str_lit("build/debug/store/bin/main") },
    },
  });
}

UTEST_F(consume, source_lib_static_profile) {
  tmpfs_init_named(&uf->fixture.fs, "consume_source_static_profile");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/consume/source",
    .copy = { "packages/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .args = { "-p", "static" } } },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = sp_str_lit("build/static/store/bin/main") },
    },
  });
}

UTEST_F(consume, system_dep) {
  tmpfs_init_named(&uf->fixture.fs, "consume_system_dep");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/consume/system_dep",
    .copy = { "packages/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .args = { "-p", "static" } } },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = sp_str_lit("build/static/store/lib/libspum.a") },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = sp_str_lit("build/static/store/bin/main") },
    },
  });
}

UTEST_F(consume, transitive) {
  tmpfs_init_named(&uf->fixture.fs, "consume_transitive");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/consume/transitive",
    .copy = { "packages/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .args = { "-p", "debug" } } },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = sp_str_lit("build/debug/store/lib/libspum.a") },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = sp_str_lit("build/debug/store/lib/libspam.a") },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = sp_str_lit("build/debug/store/bin/main") },
    },
  });
}

UTEST_F(consume, multi_kind_default) {
  tmpfs_init_named(&uf->fixture.fs, "consume_multi_kind_default");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/consume/multi_kind/default",
    .copy = { "packages/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = sp_str_lit("build/debug/store/bin/main") },
    },
  });
}

UTEST_F(consume, multi_kind_static) {
  tmpfs_init_named(&uf->fixture.fs, "consume_multi_kind_static");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/consume/multi_kind/static",
    .copy = { "packages/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = sp_str_lit("build/debug/store/lib/libspum.a") },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = sp_str_lit("build/debug/store/bin/main") },
    },
  });
}

UTEST_F(consume, multi_kind_shared) {
  tmpfs_init_named(&uf->fixture.fs, "consume_multi_kind_shared");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/consume/multi_kind/shared",
    .copy = { "packages/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = sp_str_lit("build/debug/store/lib/libspum.so") },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = sp_str_lit("build/debug/store/bin/main") },
    },
  });
}

UTEST_F(consume, multi_kind_source) {
  tmpfs_init_named(&uf->fixture.fs, "consume_multi_kind_source");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/consume/multi_kind/source",
    .copy = { "packages/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = sp_str_lit("build/debug/store/bin/main") },
    },
  });
}

UTEST_F(consume, kind_not_supported) {
  tmpfs_init_named(&uf->fixture.fs, "consume_kind_not_supported");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/consume/kind_not_supported",
    .copy = { "packages/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .rc = 1 } },
    },
  });
}
