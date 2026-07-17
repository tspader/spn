#include "common.h"

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
    },
  });
}

UTEST_F(platform, frameworks) {
  tmpfs_init_named(&uf->fixture.fs, "platform_frameworks");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/platform/frameworks",
    .copy = { "main.c" },
    .when.os = SPN_OS_MACOS,
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_RUN_BIN, .bin.name = "main" },
    },
  });
}

UTEST_F(platform, subsystem) {
  tmpfs_init_named(&uf->fixture.fs, "platform_subsystem");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/platform/subsystem",
    .copy = { "main.c" },
    .when.os = SPN_OS_WINDOWS,
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_RUN_BIN, .bin.name = "main" },
    },
  });
}
