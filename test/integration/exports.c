#include "common.h"

SPN_TEST_SUITE(exports)

UTEST_F(exports, own_exported) {
  tmpfs_init_named(&uf->fixture.fs, "exports_own_exported");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/exports/own_exported",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_RUN_BIN, .bin = { .name = "main", .rc = 0 } },
    },
  });
}

UTEST_F(exports, private_hidden) {
  tmpfs_init_named(&uf->fixture.fs, "exports_private_hidden");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/exports/private_hidden",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_RUN_BIN, .bin = { .name = "main", .rc = 0 } },
    },
  });
}

UTEST_F(exports, public_reexported) {
  tmpfs_init_named(&uf->fixture.fs, "exports_public_reexported");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/exports/public_reexported",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_RUN_BIN, .bin = { .name = "main", .rc = 0 } },
    },
  });
}

UTEST_F(exports, collision_loud) {
  tmpfs_init_named(&uf->fixture.fs, "exports_collision_loud");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/exports/collision_loud",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .rc = 1 } },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = "link_failed" } },
    },
  });
}

UTEST_F(exports, no_interference) {
  tmpfs_init_named(&uf->fixture.fs, "exports_no_interference");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/exports/no_interference",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_RUN_BIN, .bin = { .name = "main", .rc = 0 } },
    },
  });
}
