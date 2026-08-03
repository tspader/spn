#include "harness.h"

sp_test(layout, staged_bin) {
  return run_command_test(t, (command_test_t) {
    .project = "test/integration/fixtures/layout/test_shared",
    .copy = { "check.c", "packages/*" },
    .args = { "build" },
    .expect = {
      .bin.name = "main",
      .exists = { exe("main"), staged_lib("spum"), store_file("bin/main") },
    },
  });
}

sp_test(layout, staged_test) {
  return run_command_test(t, (command_test_t) {
    .project = "test/integration/fixtures/layout/test_shared",
    .copy = { "check.c", "packages/*" },
    .args = { "build" },
    .expect = {
      .bin.path = test_exe("check"),
      .exists = { test_exe("check"), test_lib("spum") },
    },
  });
}

sp_test(layout, staged_script) {
  return run_command_test(t, (command_test_t) {
    .project = "test/integration/fixtures/run/manifest",
    .args = { "build", "main" },
    .expect.exists = { exe("main") },
  });
}

sp_test(layout, script_ctx_footprint) {
  return run_command_test(t, (command_test_t) {
    .project = "test/integration/fixtures/script/default_script",
    .args = { "build" },
    .expect = {
      .exists = { sp_str_lit("build/wasm32-wasi/work/default_script/spn/configure.wasm") },
    },
  });
}

sp_test(layout, reserved_bin_name) {
  return run_command_test(t, (command_test_t) {
    .project = "test/integration/fixtures/layout/reserved_bin",
    .args = { "build" },
    .expect.rc = 1,
  });
}

sp_test(layout, target_triple) {
  return run_command_test(t, (command_test_t) {
    .project = "test/integration/fixtures/layout/test_shared",
    .copy = { "check.c", "packages/*" },
    .args = { "build", "--target", SPN_TEST_TRIPLE },
    .expect = {
      .exists = { target_exe("main", SPN_TEST_TRIPLE), target_store_file("bin/main", SPN_TEST_TRIPLE) },
      .missing = { sp_str_lit("build/debug") },
    },
  });
}
