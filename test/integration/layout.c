#include "harness.h"

sp_test(layout, staged_bin) {
  return run_command_test(t, (command_test_t) {
    .project = "test/integration/fixtures/layout/test_shared",
    .when.msvc_todo = true,
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
    .when.msvc_todo = true,
    .copy = { "check.c", "packages/*" },
    .args = { "build" },
    .expect = {
      .bin.path = test_exe("check"),
      .exists = { test_exe("check"), test_lib("spum") },
    },
  });
}

// macOS dyld resolves a hardlinked main executable to an arbitrary one of the
// inode's link names, so @loader_path is only well-defined when a staged exe
// is its inode's sole name.
sp_test(layout, staged_identity) {
  fixture_t fixture = sp_zero;
  sp_try(fixture_init(t, &fixture));
  sp_try(test_when(t, (test_when_t) { .msvc_todo = true }));
  sp_try(run_command(t, &fixture, (command_test_t) {
    .project = "test/integration/fixtures/layout/test_shared",
    .copy = { "check.c", "packages/*" },
    .args = { "build" },
    .expect.exists = { exe("main"), test_exe("check") },
  }));

  sp_str_t staged[] = { exe("main"), test_exe("check") };
  sp_carr_for(staged, it) {
    sp_str_t path = fixture_path(&fixture, staged[it]);
    sp_sys_file_meta_t meta = sp_zero;
    sp_sys_get_path_metadata_s(sp_sys_get_root(0), path, &meta);
    sp_test_kv(t, "path", path);
    sp_expect_eq(t, (u64)1, meta.nlink);
  }
  return SP_OK;
}

sp_test(layout, staged_script) {
  return run_command_test(t, (command_test_t) {
    .project = "test/integration/fixtures/script/staged",
    .when.msvc_todo = true,
    .args = { "build", "main" },
    .expect.exists = { exe("main") },
  });
}

sp_test(layout, script_ctx_footprint) {
  return run_command_test(t, (command_test_t) {
    .project = "test/integration/fixtures/script/default_script",
    .args = { "build" },
    .expect = {
      .exists = { sp_str_lit("build/wasm32-wasi-musl/.spn/default_script/configure.wasm") },
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
    .when.msvc_todo = true,
    .copy = { "check.c", "packages/*" },
    .args = { "build", "--target", SPN_TEST_TRIPLE },
    .expect = {
      .exists = { target_exe("main", SPN_TEST_TRIPLE), target_store_file("bin/main", SPN_TEST_TRIPLE) },
      .missing = { sp_str_lit("build/debug") },
    },
  });
}
