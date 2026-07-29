sp_test(reexport, transitive) {
  return sp_test_skip(t, "disabled");
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/reexport/transitive",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_RUN_BIN, .bin = { .name = "main", .rc = 0 } },
    },
  });
}

sp_test(reexport, private_hidden) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/reexport/private_hidden",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .rc = 1 } },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = SPN_EVENT_TARGET_BUILD_FAILED } },
    },
  });
}

sp_test(reexport, private_owner) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/reexport/private_owner",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_RUN_BIN, .bin = { .name = "main", .rc = 0 } },
    },
  });
}

sp_test(reexport, private_subtree) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/reexport/private_subtree",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .rc = 1 } },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = SPN_EVENT_TARGET_BUILD_FAILED } },
    },
  });
}

sp_test(reexport, shared_boundary) {
  return sp_test_skip(t, "disabled");
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/reexport/shared_boundary",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_RUN_BIN, .bin = { .name = "main", .rc = 0 } },
    },
  });
}

sp_test(reexport, public_define) {
  return sp_test_skip(t, "disabled");
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/reexport/public_define",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_RUN_BIN, .bin = { .name = "main", .rc = 0 } },
    },
  });
}
