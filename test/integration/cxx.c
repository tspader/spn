#include "common.h"

SPN_TEST_SUITE(cxx)

UTEST_F(cxx, static_lib) {
  tmpfs_init_named(&uf->fixture.fs, "cxx_static_lib");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/cxx/static_lib",
    .copy = { "packages/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_EXISTS, .exists = static_lib("spum") },
      { .kind = ACTION_RUN_BIN, .bin.name = "main" },
    },
  });
}

UTEST_F(cxx, shared_lib) {
  tmpfs_init_named(&uf->fixture.fs, "cxx_shared_lib");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/cxx/shared_lib",
    .copy = { "packages/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_EXISTS, .exists = shared_lib("spum") },
      { .kind = ACTION_RUN_BIN, .bin.name = "main" },
    },
  });
}

UTEST_F(cxx, mixed_lib) {
  tmpfs_init_named(&uf->fixture.fs, "cxx_mixed_lib");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/cxx/mixed_lib",
    .copy = { "packages/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_RUN_BIN, .bin.name = "main" },
    },
  });
}

UTEST_F(cxx, standard) {
  tmpfs_init_named(&uf->fixture.fs, "cxx_standard");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/cxx/standard",
    .copy = { "packages/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_EXISTS, .exists = exe("main") },
    },
  });
}

UTEST_F(cxx, exceptions_off) {
  tmpfs_init_named(&uf->fixture.fs, "cxx_exceptions_off");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/cxx/exceptions_off",
    .copy = { "packages/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_EXISTS, .exists = exe("main") },
    },
  });
}

UTEST_F(cxx, rtti_off) {
  tmpfs_init_named(&uf->fixture.fs, "cxx_rtti_off");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/cxx/rtti_off",
    .copy = { "packages/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_EXISTS, .exists = exe("main") },
    },
  });
}

UTEST_F(cxx, static_into_shared) {
  tmpfs_init_named(&uf->fixture.fs, "cxx_static_into_shared");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/cxx/static_into_shared",
    .copy = { "packages/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_EXISTS, .exists = shared_lib("spum") },
      { .kind = ACTION_RUN_BIN, .bin.name = "main" },
    },
  });
}

UTEST_F(cxx, transitive) {
  tmpfs_init_named(&uf->fixture.fs, "cxx_transitive");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/cxx/transitive",
    .copy = { "packages/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_RUN_BIN, .bin.name = "main" },
    },
  });
}

UTEST_F(cxx, toolchain) {
  tmpfs_init_named(&uf->fixture.fs, "cxx_toolchain");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/cxx/toolchain",
    .copy = { "packages/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_RUN_BIN, .bin.name = "main" },
    },
  });
}

UTEST_F(cxx, bin) {
  tmpfs_init_named(&uf->fixture.fs, "cxx_bin");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/cxx/bin",
    .copy = { "main.cpp" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_RUN_BIN, .bin.name = "main" },
    },
  });
}

UTEST_F(cxx, script_rejected) {
  tmpfs_init_named(&uf->fixture.fs, "cxx_script_rejected");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/cxx/script_rejected",
    .copy = { "tools" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .rc = 1 } },
      { .kind = ACTION_VERIFY_CLI_CONTAINS, .verify_cli.needle = sp_str_lit("package.build.source") },
      { .kind = ACTION_VERIFY_NO_EVENT, .verify_event = { .event = "script_compile_failed" } },
    },
  });
}

UTEST_F(cxx, toolchain_missing) {
  tmpfs_init_named(&uf->fixture.fs, "cxx_toolchain_missing");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/cxx/toolchain_missing",
    .copy = { "packages/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .rc = 1 } },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = "err" } },
      { .kind = ACTION_VERIFY_NO_EVENT, .verify_event = { .event = "target_build_failed" } },
    },
  });
}
