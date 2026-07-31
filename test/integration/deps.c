sp_test(deps_file, basic) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/deps/file/basic",
    .copy = { "packages/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .args = { "--force" } } },
      { .kind = ACTION_VERIFY_LOCKED },
      { .kind = ACTION_VERIFY_PKG_LOCKED, .verify_locked = { .name = "core/spum" } },
    },
  });
}

sp_test(deps_file, invalid_manifest) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/deps/file/invalid_manifest",
    .copy = { "vendor/spum/spn.toml" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .rc = 1 } },
      { .kind = ACTION_VERIFY_RESULT, .verify_result = { .err = SPN_ERR_MANIFEST_ISSUES } },
    },
  });
}

sp_test(deps_file, name_mismatch) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/deps/file/name_mismatch",
    .copy = { "vendor/spum/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .rc = 1 } },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = SPN_EVENT_ERR, .key = "requested", .value = "core/spum" } },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = SPN_EVENT_ERR, .key = "kind", .value = "pkg_mismatch" } },
    },
  });
}

sp_test(deps_file, missing_manifest) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/deps/file/missing_manifest",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .rc = 1 } },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = SPN_EVENT_ERR, .key = "kind", .value = "no_manifest" } },
      { .kind = ACTION_VERIFY_NO_EVENT, .verify_event = { .event = SPN_EVENT_ERR, .key = "kind", .value = "pkg_unknown" } },
    },
  });
}

sp_test(deps_file, remote_source) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/deps/file/remote_source",
    .copy = { "vendor/spum/spn.toml" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_VERIFY_LOCKED },
      { .kind = ACTION_VERIFY_PKG_LOCKED, .verify_locked = { .name = "core/spum" } },
      { .kind = ACTION_VERIFY_EXISTS, .exists = exe("main") },
    },
  });
}

sp_test(deps_file, editable) {
  return run_rebuild_test(t, (rebuild_test_t) {
    .project = "test/integration/fixtures/deps/file/editable",
    .copy = { "packages/*", "main.kram.c" },
    .first = {
      .args = { "build" },
      .expect.exists = { exe("editable_package") },
    },
    .rebuilds = {
      {
        .change = {
          .remove_files = { sp_str_lit("packages/spum/spum.h") },
          .moves = {
            { .from = sp_str_lit("packages/spum/kram.h"), .to = sp_str_lit("packages/spum/spum.h") },
            { .from = sp_str_lit("main.kram.c"), .to = sp_str_lit("main.c") },
          },
          .remove_dirs = { sp_str_lit("build") },
        },
        .command = {
          .args = { "build" },
          .expect = {
            .exists = { exe("editable_package") },
            .lock = true,
            .packages = { "core/spum" },
          },
        },
      },
    },
  });
}

sp_test(deps_index, basic) {
  return run_rebuild_test(t, (rebuild_test_t) {
    .project = "test/integration/fixtures/deps/index/basic",
    .first = {
      .args = { "build" },
      .expect = {
        .lock = true,
        .packages = { "core/spum" },
      },
    },
    .rebuilds = {
      {
        .change.remove_dirs = { sp_str_lit("build") },
        .command = {
          .args = { "build" },
          .expect = {
            .lock = true,
            .packages = { "core/spum" },
          },
        },
      },
    },
  });
}

sp_test(deps_index, name_mismatch) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/deps/index/name_mismatch",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .rc = 1 } },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = SPN_EVENT_ERR, .key = "requested", .value = "core/spum" } },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = SPN_EVENT_ERR, .key = "kind", .value = "pkg_mismatch" } },
    },
  });
}

sp_test(deps_index, pinned_commit) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/deps/index/pinned_commit",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_VERIFY_EXISTS, .exists = exe("index_package_pinned_commit") },
    },
  });
}

sp_test(deps_index, without_source) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/deps/index/without_source",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_VERIFY_LOCKED },
      { .kind = ACTION_VERIFY_PKG_LOCKED, .verify_locked = { .name = "core/spum" } },
      { .kind = ACTION_VERIFY_EXISTS, .exists = exe("main") },
    },
  });
}

sp_test(deps_index, binary_static) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/deps/index/binary_static",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_VERIFY_LOCKED },
      { .kind = ACTION_VERIFY_PKG_LOCKED, .verify_locked = { .name = "core/spum" } },
      { .kind = ACTION_VERIFY_EXISTS, .exists = store_file("bin/main") },
    },
  });
}

sp_test(deps_index, binary_shared) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/deps/index/binary_shared",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_VERIFY_LOCKED },
      { .kind = ACTION_VERIFY_PKG_LOCKED, .verify_locked = { .name = "core/spum" } },
      { .kind = ACTION_VERIFY_EXISTS, .exists = store_file("bin/main") },
    },
  });
}

sp_test(deps_index, split_recipe) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/deps/index/split_recipe",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_VERIFY_PKG_LOCKED, .verify_locked = { .name = "core/spum" } },
      { .kind = ACTION_VERIFY_EXISTS, .exists = exe("main") },
    },
  });
}

sp_test(deps_index, patched) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/deps/index/patched",
    .copy = { "patches/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_VERIFY_EXISTS, .exists = exe("main") },
    },
  });
}

sp_test(deps_index, fetch_fails) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/deps/index/fetch_fails",
    .actions = {
      { .kind = ACTION_REMOVE_DIR, .rm = { .dir = "remote/spum" } },
      { .kind = ACTION_RUN_CLI, .cli = { "build", .rc = 1 } },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = SPN_EVENT_SYNC_FAILED, .key = "name", .value = "core/spum" } },
    },
  });
}

sp_test(deps_index, invalid_manifest) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/deps/index/invalid_manifest",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .rc = 1 } },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = SPN_EVENT_ERR, .key = "name", .value = "core/spum" } },
    },
  });
}
