SPN_TEST_SUITE(log)

UTEST_F(log, clean) {
  tmpfs_init_named(&uf->fixture.fs, "log_clean");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/log/clean",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_NOT_EXISTS, .verify_not_exists.file = work_file("log_clean/log_clean.build.log") },
    },
  });
}

UTEST_F(log, warn) {
  tmpfs_init_named(&uf->fixture.fs, "log_warn");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/log/warn",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_FILE_NONEMPTY, .verify_file_nonempty.file = work_file("log_warn/log_warn.build.log") },
      { .kind = ACTION_VERIFY_FILE_CONTAINS, .verify_file_contains = { .file = work_file("log_warn/log_warn.build.log"), .needle = sp_str_lit("spn-log-probe-warn") } },
    },
  });
}

UTEST_F(log, error) {
  tmpfs_init_named(&uf->fixture.fs, "log_error");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/log/error",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .rc = 1 } },
      { .kind = ACTION_VERIFY_FILE_NONEMPTY, .verify_file_nonempty.file = work_file("log_error/log_error.build.log") },
      { .kind = ACTION_VERIFY_FILE_CONTAINS, .verify_file_contains = { .file = work_file("log_error/log_error.build.log"), .needle = sp_str_lit("spn-log-probe-error") } },
    },
  });
}

UTEST_F(log, link_error) {
  tmpfs_init_named(&uf->fixture.fs, "log_link_error");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/log/link_error",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .rc = 1 } },
      { .kind = ACTION_VERIFY_FILE_NONEMPTY, .verify_file_nonempty.file = work_file("log_link_error/log_link_error.build.log") },
      { .kind = ACTION_VERIFY_FILE_CONTAINS, .verify_file_contains = { .file = work_file("log_link_error/log_link_error.build.log"), .needle = sp_str_lit("spn_log_missing_symbol") } },
    },
  });
}

UTEST_F(log, warn_multi) {
  tmpfs_init_named(&uf->fixture.fs, "log_warn_multi");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/log/warn_multi",
    .copy = { "a.c", "b.c" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_FILE_CONTAINS, .verify_file_contains = { .file = work_file("log_warn_multi/log_warn_multi.build.log"), .needle = sp_str_lit("spn-log-probe-main") } },
      { .kind = ACTION_VERIFY_FILE_CONTAINS, .verify_file_contains = { .file = work_file("log_warn_multi/log_warn_multi.build.log"), .needle = sp_str_lit("spn-log-probe-a") } },
      { .kind = ACTION_VERIFY_FILE_CONTAINS, .verify_file_contains = { .file = work_file("log_warn_multi/log_warn_multi.build.log"), .needle = sp_str_lit("spn-log-probe-b") } },
    },
  });
}

UTEST_F(log, preserved_on_cache_hit) {
  tmpfs_init_named(&uf->fixture.fs, "log_preserved");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/log/warn",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_FILE_CONTAINS, .verify_file_contains = { .file = work_file("log_warn/log_warn.build.log"), .needle = sp_str_lit("spn-log-probe-warn") } },
      { .kind = ACTION_SNAPSHOT_MTIME, .snapshot_mtime.file = work_file("log_warn/log_warn.build.log") },
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_MTIME_UNCHANGED, .verify_mtime.file = work_file("log_warn/log_warn.build.log") },
    },
  });
}

UTEST_F(log, script_log_hidden_normally) {
  tmpfs_init_named(&uf->fixture.fs, "log_script_log_hidden");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/log/script_log",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_CLI_NOT_CONTAINS, .verify_cli.needle = sp_str_lit("spn-script-probe-log") },
    },
  });
}

UTEST_F(log, script_log_shown_verbose) {
  tmpfs_init_named(&uf->fixture.fs, "log_script_log_verbose");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/log/script_log",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .args = { "--verbose" } } },
      { .kind = ACTION_VERIFY_CLI_CONTAINS, .verify_cli.needle = sp_str_lit("spn-script-probe-log") },
    },
  });
}

UTEST_F(log, script_log_shown_on_failure) {
  tmpfs_init_named(&uf->fixture.fs, "log_script_log_failure");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/log/script_log_fail",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .rc = 1 } },
      { .kind = ACTION_VERIFY_CLI_CONTAINS, .verify_cli.needle = sp_str_lit("spn-script-probe-fail") },
    },
  });
}

UTEST_F(log, rewritten_on_rebuild) {
  tmpfs_init_named(&uf->fixture.fs, "log_rewritten");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/log/warn",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_SNAPSHOT_MTIME, .snapshot_mtime.file = work_file("log_warn/log_warn.build.log") },
      { .kind = ACTION_CREATE_FILE, .create = { .file = sp_str_lit("main.c"), .content = sp_str_lit("#warning \"spn-log-probe-rebuilt\"\nint main(void) { return 0; }\n") } },
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .args = { "--force" } } },
      { .kind = ACTION_VERIFY_MTIME_CHANGED, .verify_mtime.file = work_file("log_warn/log_warn.build.log") },
      { .kind = ACTION_VERIFY_FILE_CONTAINS, .verify_file_contains = { .file = work_file("log_warn/log_warn.build.log"), .needle = sp_str_lit("spn-log-probe-rebuilt") } },
      { .kind = ACTION_VERIFY_FILE_NOT_CONTAINS, .verify_file_not_contains = { .file = work_file("log_warn/log_warn.build.log"), .needle = sp_str_lit("spn-log-probe-warn") } },
    },
  });
}
