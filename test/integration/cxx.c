#include "harness.h"

sp_test(cxx, static_lib) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/cxx/static_lib",
    .copy = { "packages/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_EXISTS, .exists = static_lib("spum") },
      { .kind = ACTION_RUN_BIN, .bin.name = "main" },
    },
  });
}

sp_test(cxx, shared_lib) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/cxx/shared_lib",
    .when.msvc_todo = true,
    .copy = { "packages/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_EXISTS, .exists = shared_lib("spum") },
      { .kind = ACTION_RUN_BIN, .bin.name = "main" },
    },
  });
}

sp_test(cxx, mixed_lib) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/cxx/mixed_lib",
    .copy = { "packages/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_RUN_BIN, .bin.name = "main" },
    },
  });
}

sp_test(cxx, standard) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/cxx/standard",
    .copy = { "packages/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_EXISTS, .exists = exe("main") },
    },
  });
}

sp_test(cxx, exceptions_off) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/cxx/exceptions_off",
    .copy = { "packages/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_EXISTS, .exists = exe("main") },
    },
  });
}

sp_test(cxx, rtti_off) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/cxx/rtti_off",
    .copy = { "packages/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_EXISTS, .exists = exe("main") },
    },
  });
}

sp_test(cxx, static_into_shared) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/cxx/static_into_shared",
    .when.msvc_todo = true,
    .copy = { "packages/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_EXISTS, .exists = shared_lib("spum") },
      { .kind = ACTION_RUN_BIN, .bin.name = "main" },
    },
  });
}

sp_test(cxx, transitive) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/cxx/transitive",
    .copy = { "packages/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_RUN_BIN, .bin.name = "main" },
    },
  });
}

sp_test(cxx, toolchain) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/cxx/toolchain",
    .copy = { "packages/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_RUN_BIN, .bin.name = "main" },
    },
  });
}

sp_test(cxx, bin) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/cxx/bin",
    .copy = { "main.cpp" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_RUN_BIN, .bin.name = "main" },
    },
  });
}

sp_test(cxx, script_rejected) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/cxx/script_rejected",
    .copy = { "tools" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .rc = 1 } },
      { .kind = ACTION_VERIFY_RESULT, .verify_result = { .err = SPN_ERR_MANIFEST_ISSUES } },
      { .kind = ACTION_VERIFY_NO_EVENT, .verify_event = { .event = SPN_EVENT_SCRIPT_COMPILE_FAILED } },
    },
  });
}

sp_test(cxx, toolchain_missing) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/cxx/toolchain_missing",
    .when.msvc_todo = true,
    .copy = { "packages/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .rc = 1 } },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = SPN_EVENT_ERR } },
      { .kind = ACTION_VERIFY_NO_EVENT, .verify_event = { .event = SPN_EVENT_TARGET_BUILD_FAILED } },
    },
  });
}
