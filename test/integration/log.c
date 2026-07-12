SPN_TEST_SUITE(log)

UTEST_F(log, warn_multi) {
  tmpfs_init_named(&uf->fixture.fs, "log_warn_multi");

  run_command_test(utest_result, &uf->fixture, (command_test_t) {
    .project = "test/integration/fixtures/log/warn_multi",
    .copy = { "a.c", "b.c" },
    .args = { "build" },
    .expect.files = {
      {
        .file = work_file("log_warn_multi/log_warn_multi.build.log"),
        .contains = { "spn-log-probe-main", "spn-log-probe-a", "spn-log-probe-b" },
      },
    },
  });
}

UTEST_F(log, preserved_on_cache_hit) {
  tmpfs_init_named(&uf->fixture.fs, "log_preserved");

  run_rebuild_test(utest_result, &uf->fixture, (rebuild_test_t) {
    .project = "test/integration/fixtures/log/warn",
    .first = {
      .args = { "build" },
      .expect.files = {
        { .file = work_file("log_warn/log_warn.build.log"), .contains = { "spn-log-probe-warn" } },
      },
    },
    .rebuilds = {
      { .command.args = { "build" } },
    },
    .watches = {
      { .file = work_file("log_warn/log_warn.build.log"), .mtime = REBUILD_MTIME_UNCHANGED },
    },
  });
}

SPN_TEST_SUITE(user_log)

UTEST_F(user_log, hidden_normally) {
  tmpfs_init_named(&uf->fixture.fs, "log_script_log_hidden");

  run_command_test(utest_result, &uf->fixture, (command_test_t) {
    .project = "test/integration/fixtures/log/script_log",
    .args = { "build" },
    .expect.excludes = { "spn-script-probe-log" },
  });
}

UTEST_F(user_log, shown_verbose) {
  tmpfs_init_named(&uf->fixture.fs, "log_script_log_verbose");

  run_command_test(utest_result, &uf->fixture, (command_test_t) {
    .project = "test/integration/fixtures/log/script_log",
    .args = { "build", "--verbose" },
    .expect.contains = { "spn-script-probe-log" },
  });
}

UTEST_F(user_log, shown_on_failure) {
  tmpfs_init_named(&uf->fixture.fs, "log_script_log_failure");

  run_command_test(utest_result, &uf->fixture, (command_test_t) {
    .project = "test/integration/fixtures/log/script_log_fail",
    .args = { "build" },
    .expect = {
      .rc = 1,
      .contains = { "spn-script-probe-fail" },
    },
  });
}

UTEST_F(log, rewritten_on_rebuild) {
  tmpfs_init_named(&uf->fixture.fs, "log_rewritten");

  run_rebuild_test(utest_result, &uf->fixture, (rebuild_test_t) {
    .project = "test/integration/fixtures/log/warn",
    .first.args = { "build" },
    .rebuilds = {
      {
        .change.writes = {
          { .file = sp_str_lit("main.c"), .content = sp_str_lit("#warning \"spn-log-probe-rebuilt\"\nint main(void) { return 0; }\n") },
        },
        .command = {
          .args = { "build", "--force" },
          .expect.files = {
            {
              .file = work_file("log_warn/log_warn.build.log"),
              .contains = { "spn-log-probe-rebuilt" },
              .excludes = { "spn-log-probe-warn" },
            },
          },
        },
      },
    },
    .watches = {
      { .file = work_file("log_warn/log_warn.build.log"), .mtime = REBUILD_MTIME_CHANGED },
    },
  });
}
