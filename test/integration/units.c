#include "common.h"

SPN_TEST_SUITE(units)

UTEST_F(units, build_dep_conflict) {
  tmpfs_init_named(&uf->fixture.fs, "units_build_dep_conflict");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/units/build_dep_conflict",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_RUN_BIN, .bin = { .name = "main", .rc = 0 } },
    },
  });
}

UTEST_F(units, build_dep_transitive_conflict) {
  tmpfs_init_named(&uf->fixture.fs, "units_build_dep_transitive_conflict");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/units/build_dep_transitive_conflict",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_VERIFY_NO_EVENT, .verify_event = { .event = SPN_EVENT_ERR_UNSATISFIABLE_VERSION } },
      { .kind = ACTION_RUN_BIN, .bin = { .name = "main", .rc = 0 } },
    },
  });
}

UTEST_F(units, shared_conflict) {
  tmpfs_init_named(&uf->fixture.fs, "units_shared_conflict");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/units/shared_conflict",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .rc = 1 } },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = SPN_EVENT_ERR_UNSATISFIABLE_VERSION } },
    },
  });
}

UTEST_F(units, shared_private) {
  tmpfs_init_named(&uf->fixture.fs, "units_shared_private");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/units/shared_private",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_VERIFY_NO_EVENT, .verify_event = { .event = SPN_EVENT_ERR_UNSATISFIABLE_VERSION } },
      { .kind = ACTION_VERIFY_NO_EVENT, .verify_event = { .event = SPN_EVENT_ERR_DYNAMIC_DUPLICATE } },
      { .kind = ACTION_RUN_BIN, .bin = { .name = "main", .rc = 0 } },
    },
  });
}

UTEST_F(units, static_conflict) {
  tmpfs_init_named(&uf->fixture.fs, "units_static_conflict");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/units/static_conflict",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .rc = 1 } },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = SPN_EVENT_ERR_UNSATISFIABLE_VERSION } },
    },
  });
}

UTEST_F(units, no_double_build) {
  tmpfs_init_named(&uf->fixture.fs, "units_no_double_build");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/units/no_double_build",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = SPN_EVENT_RESOLVE_PACKAGE, .key = "version", .value = "1.5.0" } },
      { .kind = ACTION_VERIFY_NO_EVENT, .verify_event = { .event = SPN_EVENT_RESOLVE_PACKAGE, .key = "version", .value = "2.0.0" } },
      { .kind = ACTION_VERIFY_EXISTS, .exists = exe("main") },
    },
  });
}

UTEST_F(units, no_downgrade) {
  tmpfs_init_named(&uf->fixture.fs, "units_no_downgrade");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/units/no_downgrade",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_VERIFY_NO_EVENT, .verify_event = { .event = SPN_EVENT_ERR_UNSATISFIABLE_VERSION } },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = SPN_EVENT_RESOLVE_PACKAGE, .key = "version", .value = "1.9.0" } },
      { .kind = ACTION_VERIFY_EXISTS, .exists = exe("main") },
    },
  });
}

UTEST_F(units, build_dep_cycle) {
  tmpfs_init_named(&uf->fixture.fs, "units_build_dep_cycle");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/units/build_dep_cycle",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .rc = 1 } },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = SPN_EVENT_ERR_UNIT_CYCLE } },
    },
  });
}

UTEST_F(units, build_dep_bootstrap) {
  tmpfs_init_named(&uf->fixture.fs, "units_build_dep_bootstrap");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/units/build_dep_bootstrap",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_VERIFY_NO_EVENT, .verify_event = { .event = SPN_EVENT_ERR_UNIT_CYCLE } },
      { .kind = ACTION_VERIFY_NO_EVENT, .verify_event = { .event = SPN_EVENT_ERR_UNSATISFIABLE_VERSION } },
      { .kind = ACTION_VERIFY_EXISTS, .exists = exe("main") },
    },
  });
}

UTEST_F(units, same_version_split) {
  UTEST_SKIP("");
  tmpfs_init_named(&uf->fixture.fs, "units_same_version_split");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/units/same_version_split",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_VERIFY_CLI_CONTAINS, .verify_cli.needle = sp_str_lit("Resolved 6 packages") },
      { .kind = ACTION_VERIFY_NO_EVENT, .verify_event = { .event = SPN_EVENT_ERR_UNSATISFIABLE_VERSION } },
      { .kind = ACTION_RUN_BIN, .bin = { .name = "main", .rc = 0 } },
    },
  });
}

UTEST_F(units, sibling_order) {
  UTEST_SKIP("");
  tmpfs_init_named(&uf->fixture.fs, "units_sibling_order");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/units/sibling_order",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_RUN_BIN, .bin = { .name = "main", .rc = 0 } },
    },
  });
}
