SPN_TEST_SUITE(deps_file)

UTEST_F(deps_file, basic) {
  tmpfs_init_named(&uf->fixture.fs, "deps_file_basic");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/deps/file/basic",
    .copy = { "packages/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .args = { "--force" } } },
      { .kind = ACTION_VERIFY_LOCKED },
      { .kind = ACTION_VERIFY_PKG_LOCKED, .verify_locked = { .name = "core/spum" } },
    },
  });
}

UTEST_F(deps_file, custom_manifest_name) {
  tmpfs_init_named(&uf->fixture.fs, "deps_file_custom_manifest_name");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/deps/file/custom_manifest_name",
    .copy = { "vendor/spum/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_RUN_BIN, .bin = { .name = "main", .rc = 0 } },
    },
  });
}

UTEST_F(deps_file, invalid_manifest) {
  tmpfs_init_named(&uf->fixture.fs, "deps_file_invalid_manifest");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/deps/file/invalid_manifest",
    .copy = { "vendor/spum/spn.toml" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .rc = 1 } },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = "err_manifest", .key = "name", .value = "core/spum" } },
      { .kind = ACTION_VERIFY_NO_EVENT, .verify_event = { .event = "err_unknown_pkg" } },
    },
  });
}

// A file dep whose manifest declares a different name than the edge
// requested must fail at load: everything downstream routes by the
// requested name, so a silent mismatch strands option requests and config
UTEST_F(deps_file, name_mismatch) {
  tmpfs_init_named(&uf->fixture.fs, "deps_file_name_mismatch");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/deps/file/name_mismatch",
    .copy = { "vendor/spum/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .rc = 1 } },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = "err_manifest", .key = "name", .value = "core/spum" } },
      { .kind = ACTION_VERIFY_CLI_CONTAINS, .verify_cli = { .needle = sp_str_lit("core/spork") } },
    },
  });
}

UTEST_F(deps_file, missing_manifest) {
  tmpfs_init_named(&uf->fixture.fs, "deps_file_missing_manifest");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/deps/file/missing_manifest",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .rc = 1 } },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = "err_manifest", .key = "name", .value = "core/spum" } },
      { .kind = ACTION_VERIFY_NO_EVENT, .verify_event = { .event = "err_unknown_pkg" } },
    },
  });
}

UTEST_F(deps_file, remote_source) {
  tmpfs_init_named(&uf->fixture.fs, "deps_file_remote_source");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/deps/file/remote_source",
    .copy = { "vendor/spum/spn.toml" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_VERIFY_LOCKED },
      { .kind = ACTION_VERIFY_PKG_LOCKED, .verify_locked = { .name = "core/spum" } },
      { .kind = ACTION_RUN_BIN, .bin = { .name = "main", .rc = 0 } },
    },
  });
}

UTEST_F(deps_file, editable) {
  tmpfs_init_named(&uf->fixture.fs, "deps_file_editable");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/deps/file/editable",
    .copy = { "packages/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      {
        .kind = ACTION_RUN_BIN,
        .bin = {
          .name = "editable_package",
          .rc = 69,
        },
      },
      {
        .kind = ACTION_REMOVE_FILE,
        .rm.file = "packages/spum/spum.h",
      },
      {
        .kind = ACTION_MOVE_FILE,
        .mv = {
          .from = sp_str_lit("packages/spum/kram.h"),
          .to = sp_str_lit("packages/spum/spum.h"),
        },
      },
      { .kind = ACTION_REMOVE_DIR, .rm.dir = "build" },
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      {
        .kind = ACTION_RUN_BIN,
        .bin = {
          .name = "editable_package",
          .rc = 42,
        },
      },
      { .kind = ACTION_VERIFY_LOCKED },
      { .kind = ACTION_VERIFY_PKG_LOCKED, .verify_locked.name = "core/spum" },
    },
  });
}

SPN_TEST_SUITE(deps_index)

UTEST_F(deps_index, basic) {
  tmpfs_init_named(&uf->fixture.fs, "deps_index_basic");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/deps/index/basic",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_VERIFY_LOCKED },
      { .kind = ACTION_VERIFY_PKG_LOCKED, .verify_locked = { .name = "core/spum" } },
      { .kind = ACTION_REMOVE_DIR, .rm = { .dir = "build" } },
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_VERIFY_LOCKED },
      { .kind = ACTION_VERIFY_PKG_LOCKED, .verify_locked = { .name = "core/spum" } },
    },
  });
}

// name_mismatch for the index path: the release publishes as spum but the
// fetched manifest declares spork. Short names compare because published
// manifests routinely omit the namespace the index assigns.
UTEST_F(deps_index, name_mismatch) {
  tmpfs_init_named(&uf->fixture.fs, "deps_index_name_mismatch");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/deps/index/name_mismatch",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .rc = 1 } },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = "err_manifest", .key = "name", .value = "core/spum" } },
      { .kind = ACTION_VERIFY_CLI_CONTAINS, .verify_cli = { .needle = sp_str_lit("spork") } },
    },
  });
}

UTEST_F(deps_index, pinned_commit) {
  tmpfs_init_named(&uf->fixture.fs, "deps_index_pinned_commit");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/deps/index/pinned_commit",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_RUN_BIN, .bin = { .name = "index_package_pinned_commit", .rc = 0 } },
    },
  });
}

UTEST_F(deps_index, without_source) {
  tmpfs_init_named(&uf->fixture.fs, "deps_index_without_source");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/deps/index/without_source",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_VERIFY_LOCKED },
      { .kind = ACTION_VERIFY_PKG_LOCKED, .verify_locked = { .name = "core/spum" } },
      { .kind = ACTION_RUN_BIN, .bin = { .name = "main", .rc = 0 } },
    },
  });
}

UTEST_F(deps_index, binary_static) {
  tmpfs_init_named(&uf->fixture.fs, "deps_index_binary_static");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/deps/index/binary_static",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_VERIFY_LOCKED },
      { .kind = ACTION_VERIFY_PKG_LOCKED, .verify_locked = { .name = "core/spum" } },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = sp_str_lit("build/debug/store/bin/main") },
    },
  });
}

UTEST_F(deps_index, binary_shared) {
  tmpfs_init_named(&uf->fixture.fs, "deps_index_binary_shared");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/deps/index/binary_shared",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_VERIFY_LOCKED },
      { .kind = ACTION_VERIFY_PKG_LOCKED, .verify_locked = { .name = "core/spum" } },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = sp_str_lit("build/debug/store/bin/main") },
    },
  });
}

UTEST_F(deps_index, split_recipe) {
  tmpfs_init_named(&uf->fixture.fs, "deps_index_split_recipe");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/deps/index/split_recipe",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_VERIFY_PKG_LOCKED, .verify_locked = { .name = "core/spum" } },
      { .kind = ACTION_RUN_BIN, .bin = { .name = "main", .rc = 0 } },
    },
  });
}

UTEST_F(deps_index, patched) {
  tmpfs_init_named(&uf->fixture.fs, "deps_index_patched");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/deps/index/patched",
    .copy = { "patches/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_RUN_BIN, .bin = { .name = "main", .rc = 0 } },
    },
  });
}

UTEST_F(deps_index, fetch_fails) {
  tmpfs_init_named(&uf->fixture.fs, "deps_index_fetch_fails");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/deps/index/fetch_fails",
    .actions = {
      { .kind = ACTION_REMOVE_DIR, .rm = { .dir = "remote/spum" } },
      { .kind = ACTION_RUN_CLI, .cli = { "build", .rc = 1 } },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = "sync_failed", .key = "name", .value = "core/spum" } },
    },
  });
}

UTEST_F(deps_index, invalid_manifest) {
  tmpfs_init_named(&uf->fixture.fs, "deps_index_invalid_manifest");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/deps/index/invalid_manifest",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .rc = 1 } },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = "err_manifest", .key = "name", .value = "core/spum" } },
    },
  });
}

// UTEST_F(deps_index, codeberg) {
//   run_test(utest_result, &uf->fixture, (test_t) {
//     .project = "test/manual/spn_build/codeberg",
//     .actions = {
//       { .kind = ACTION_RUN_CLI, .cli = { "build" } },
//       { .kind = ACTION_VERIFY_LOCKED },
//       { .kind = ACTION_VERIFY_PKG_LOCKED, .verify_locked = { .name = "sp" } },
//     },
//   });
// }
