#include "harness.h"

sp_test(exports, own_exported) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/exports/own_exported",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_RUN_BIN, .bin = { .name = "main", .rc = 0 } },
    },
  });
}

sp_test(exports, private_hidden) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/exports/private_hidden",
    .when.exports = true,
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_RUN_BIN, .bin = { .name = "main", .rc = 0 } },
    },
  });
}

sp_test(exports, public_reexported) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/exports/public_reexported",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_RUN_BIN, .bin = { .name = "main", .rc = 0 } },
    },
  });
}

sp_test(exports, collision_loud) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/exports/collision_loud",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .rc = 1 } },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = SPN_EVENT_LINK_FAILED } },
    },
  });
}

sp_test(exports, no_interference) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/exports/no_interference",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_RUN_BIN, .bin = { .name = "main", .rc = 0 } },
    },
  });
}
