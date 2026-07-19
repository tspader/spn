#include "common.h"

SPN_TEST_SUITE(reexport)

UTEST_F(reexport, transitive) {
  UTEST_SKIP("");
  tmpfs_init_named(&uf->fixture.fs, "reexport_transitive");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/reexport/transitive",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_RUN_BIN, .bin = { .name = "main", .rc = 0 } },
    },
  });
}

UTEST_F(reexport, private_hidden) {
  tmpfs_init_named(&uf->fixture.fs, "reexport_private_hidden");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/reexport/private_hidden",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .rc = 1 } },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = "target_build_failed" } },
    },
  });
}

UTEST_F(reexport, private_owner) {
  tmpfs_init_named(&uf->fixture.fs, "reexport_private_owner");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/reexport/private_owner",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_RUN_BIN, .bin = { .name = "main", .rc = 0 } },
    },
  });
}

UTEST_F(reexport, private_subtree) {
  tmpfs_init_named(&uf->fixture.fs, "reexport_private_subtree");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/reexport/private_subtree",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .rc = 1 } },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = "target_build_failed" } },
    },
  });
}

UTEST_F(reexport, shared_boundary) {
  UTEST_SKIP("");
  tmpfs_init_named(&uf->fixture.fs, "reexport_shared_boundary");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/reexport/shared_boundary",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_RUN_BIN, .bin = { .name = "main", .rc = 0 } },
    },
  });
}

UTEST_F(reexport, public_define) {
  UTEST_SKIP("");
  tmpfs_init_named(&uf->fixture.fs, "reexport_public_define");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/reexport/public_define",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_RUN_BIN, .bin = { .name = "main", .rc = 0 } },
    },
  });
}
