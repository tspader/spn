sp_test(platform, inert) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/platform/inert",
    .copy = { "main.c" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_RUN_BIN, .bin.name = "main" },
    },
  });
}

sp_test(platform, dep_inert) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/platform/fingerprint",
    .copy = { "main.c", "packages/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
    },
  });
}

sp_test(platform, frameworks) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/platform/frameworks",
    .copy = { "main.c" },
    .when.os = SPN_OS_MACOS,
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_RUN_BIN, .bin.name = "main" },
    },
  });
}

sp_test(platform, subsystem) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/platform/subsystem",
    .copy = { "main.c" },
    .when.os = SPN_OS_WINDOWS,
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_RUN_BIN, .bin.name = "main" },
    },
  });
}
