#include "harness.h"

sp_test(units, build_dep_conflict) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/units/build_dep_conflict",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
    },
  });
}

sp_test(units, build_dep_transitive_conflict) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/units/build_dep_transitive_conflict",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
    },
  });
}

sp_test(units, shared_conflict) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/units/shared_conflict",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .rc = 1 } },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = SPN_EVENT_ERR, .key = "kind", .value = "pkg_conflict" } },
    },
  });
}

sp_test(units, shared_private) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/units/shared_private",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
    },
  });
}

sp_test(units, static_conflict) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/units/static_conflict",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .rc = 1 } },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = SPN_EVENT_ERR, .key = "kind", .value = "pkg_conflict" } },
    },
  });
}

sp_test(units, no_double_build) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/units/no_double_build",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = SPN_EVENT_RESOLVE_PACKAGE, .key = "version", .value = "1.5.0" } },
      { .kind = ACTION_VERIFY_NO_EVENT, .verify_event = { .event = SPN_EVENT_RESOLVE_PACKAGE, .key = "version", .value = "2.0.0" } },
      { .kind = ACTION_VERIFY_EXISTS, .exists = exe("main") },
    },
  });
}

sp_test(units, no_downgrade) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/units/no_downgrade",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = SPN_EVENT_RESOLVE_PACKAGE, .key = "version", .value = "1.9.0" } },
      { .kind = ACTION_VERIFY_EXISTS, .exists = exe("main") },
    },
  });
}

sp_test(units, build_dep_cycle) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/units/build_dep_cycle",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .rc = 1 } },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = SPN_EVENT_ERR, .key = "kind", .value = "unit_cycle" } },
    },
  });
}

sp_test(units, build_dep_bootstrap) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/units/build_dep_bootstrap",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_VERIFY_EXISTS, .exists = exe("main") },
    },
  });
}

sp_test(units, same_version_split) {
  return sp_test_skip(t, "disabled");
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/units/same_version_split",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = SPN_EVENT_RESOLVE_END, .key = "num_resolved", .value = "6" } },
    },
  });
}

sp_test(units, sibling_order) {
  return sp_test_skip(t, "disabled");
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/units/sibling_order",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
    },
  });
}
