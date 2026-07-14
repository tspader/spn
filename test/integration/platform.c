SPN_TEST_SUITE(platform)

UTEST_F(platform, inert) {
  tmpfs_init_named(&uf->fixture.fs, "platform_inert");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/platform/inert",
    .copy = { "main.c" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_RUN_BIN, .bin.name = "main" },
    },
  });
}

UTEST_F(platform, dep_inert) {
  tmpfs_init_named(&uf->fixture.fs, "platform_dep_inert");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/platform/fingerprint",
    .copy = { "main.c", "packages/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_RUN_BIN, .bin.name = "main" },
    },
  });
}

UTEST_F(platform, frameworks) {
  if (sp_os_get_kind() != SP_OS_MACOS) {
    UTEST_SKIP("frameworks only link on macos");
  }

  tmpfs_init_named(&uf->fixture.fs, "platform_frameworks");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/platform/frameworks",
    .copy = { "main.c" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_RUN_BIN, .bin.name = "main" },
    },
  });
}

UTEST_F(platform, subsystem) {
  if (sp_os_get_kind() != SP_OS_WIN32) {
    UTEST_SKIP("the windows subsystem flag only links on windows");
  }

  tmpfs_init_named(&uf->fixture.fs, "platform_subsystem");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/platform/subsystem",
    .copy = { "main.c" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_RUN_BIN, .bin.name = "main" },
    },
  });
}
