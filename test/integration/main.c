#define SP_IMPLEMENTATION
#include "sp.h"
#include "test.h"
#include "utest.h"
#include "action.h"
#include "harness.h"

UTEST_MAIN()

struct spn_build {
  fixture_t fixture;
};

UTEST_INITIALIZER(spn_build_init_tmpfs_top_level) {
  sp_mem_t mem = sp_mem_os_new();
  sp_str_t tmp = sp_fs_normalize_path(mem, sp_os_env_get(sp_str_lit("SPN_TEST_TMP")));
  if (sp_str_empty(tmp)) {
    tmp = sp_str_lit(".tmp");
  }

  sp_tm_epoch_t now = sp_tm_now_epoch();
  sp_str_t iso = sp_tm_epoch_to_iso8601(mem, now);
  c8* sanitized = (c8*)sp_alloc(mem, iso.len);
  sp_for(it, iso.len) {
    sanitized[it] = iso.data[it] == ':' ? '-' : iso.data[it];
  }

  tmpfs_set_top_level(sp_fs_join_path(mem, tmp, sp_str(sanitized, iso.len)));
}

UTEST_F_SETUP(spn_build) {
  // CMake passes these as compile definitions; under spn, derive them from the
  // repo root, which is where you'd run the suite from anyway
#if defined(SPN_TEST_ROOT) && defined(SPN_TEST_BIN)
  uf->fixture.paths.root = sp_str_lit(SPN_TEST_ROOT);
  uf->fixture.paths.spn = sp_str_lit(SPN_TEST_BIN);
#else
  sp_mem_t mem = sp_mem_os_new();
  uf->fixture.paths.root = sp_fs_get_cwd(mem);
  uf->fixture.paths.spn = sp_fs_join_path(mem, uf->fixture.paths.root, sp_str_lit("build/debug/store/bin/spn"));
#endif
  ASSERT_TRUE(sp_fs_exists(uf->fixture.paths.spn));
}

UTEST_F_TEARDOWN(spn_build) {
}

UTEST_F(spn_build, tmpfs) {
  tmpfs_init_named(&uf->fixture.fs, "tmpfs");

  tmpfs_create(&uf->fixture.fs, sp_str_lit("foo/bar.txt"), sp_str_lit("hello"));

  sp_str_t path = tmpfs_get(&uf->fixture.fs, sp_str_lit("foo/bar.txt"));
  EXPECT_TRUE(sp_fs_exists(path));
  EXPECT_TRUE(sp_str_starts_with(path, uf->fixture.fs.root));

  sp_str_t content = test_read_file(sp_mem_os_new(), path);
  EXPECT_TRUE(sp_str_equal(content, sp_str_lit("hello")));

  sp_str_t touched = tmpfs_touch(&uf->fixture.fs, sp_str_lit("nested/empty.txt"));
  EXPECT_TRUE(sp_fs_exists(touched));
  EXPECT_TRUE(sp_str_starts_with(touched, uf->fixture.fs.root));
}

UTEST_F(spn_build, index_package) {
  tmpfs_init_named(&uf->fixture.fs, "index_package");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/spn_build/index_package",
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

UTEST_F(spn_build, index_package_pinned_commit) {
  tmpfs_init_named(&uf->fixture.fs, "index_package_pinned_commit");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/spn_build/index_package_pinned_commit",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_RUN_BIN, .bin = { .name = "index_package_pinned_commit", .rc = 0 } },
    },
  });
}

UTEST_F(spn_build, index_package_without_source) {
  tmpfs_init_named(&uf->fixture.fs, "index_package_without_source");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/spn_build/index_package_without_source",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_VERIFY_LOCKED },
      { .kind = ACTION_VERIFY_PKG_LOCKED, .verify_locked = { .name = "core/spum" } },
      { .kind = ACTION_RUN_BIN, .bin = { .name = "main", .rc = 0 } },
    },
  });
}

UTEST_F(spn_build, index_package_binary_static) {
  tmpfs_init_named(&uf->fixture.fs, "index_package_binary_static");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/spn_build/index_package_binary_static",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_VERIFY_LOCKED },
      { .kind = ACTION_VERIFY_PKG_LOCKED, .verify_locked = { .name = "core/spum" } },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = sp_str_lit("build/debug/store/bin/main") },
    },
  });
}

UTEST_F(spn_build, index_package_binary_shared) {
  tmpfs_init_named(&uf->fixture.fs, "index_package_binary_shared");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/spn_build/index_package_binary_shared",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_VERIFY_LOCKED },
      { .kind = ACTION_VERIFY_PKG_LOCKED, .verify_locked = { .name = "core/spum" } },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = sp_str_lit("build/debug/store/bin/main") },
    },
  });
}

// UTEST_F(spn_build, codeberg) {
//   run_test(utest_result, &uf->fixture, (test_t) {
//     .project = "test/manual/spn_build/codeberg",
//     .actions = {
//       { .kind = ACTION_RUN_CLI, .cli = { "build" } },
//       { .kind = ACTION_VERIFY_LOCKED },
//       { .kind = ACTION_VERIFY_PKG_LOCKED, .verify_locked = { .name = "sp" } },
//     },
//   });
// }

UTEST_F(spn_build, file_package) {
  tmpfs_init_named(&uf->fixture.fs, "file_package");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/spn_build/file_package",
    .copy = { "packages/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .args = { "--force" } } },
      { .kind = ACTION_VERIFY_LOCKED },
      { .kind = ACTION_VERIFY_PKG_LOCKED, .verify_locked = { .name = "core/spum" } },
    },
  });
}

UTEST_F(spn_build, publish) {
  tmpfs_init_named(&uf->fixture.fs, "publish");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/spn_build/publish",
    .copy = { "packages/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_INCLUDE, .verify_include.file = sp_str_lit("kit.h") },
      { .kind = ACTION_VERIFY_INCLUDE, .verify_include.file = sp_str_lit("kit/a.h") },
      { .kind = ACTION_VERIFY_INCLUDE, .verify_include.file = sp_str_lit("kit/b.h") },
      { .kind = ACTION_RUN_BIN, .bin.name = "publish" },
    },
  });
}

UTEST_F(spn_build, path_dep_remote_source) {
  tmpfs_init_named(&uf->fixture.fs, "path_dep_remote_source");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/spn_build/path_dep_remote_source",
    .copy = { "vendor/spum/spn.toml" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_VERIFY_LOCKED },
      { .kind = ACTION_VERIFY_PKG_LOCKED, .verify_locked = { .name = "core/spum" } },
      { .kind = ACTION_RUN_BIN, .bin = { .name = "main", .rc = 0 } },
    },
  });
}

UTEST_F(spn_build, index_package_split_recipe) {
  tmpfs_init_named(&uf->fixture.fs, "index_package_split_recipe");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/spn_build/index_package_split_recipe",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_VERIFY_PKG_LOCKED, .verify_locked = { .name = "core/spum" } },
      { .kind = ACTION_RUN_BIN, .bin = { .name = "main", .rc = 0 } },
    },
  });
}

UTEST_F(spn_build, index_package_patched) {
  tmpfs_init_named(&uf->fixture.fs, "index_package_patched");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/spn_build/index_package_patched",
    .copy = { "patches/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_RUN_BIN, .bin = { .name = "main", .rc = 0 } },
    },
  });
}

UTEST_F(spn_build, file_dep_missing_manifest) {
  tmpfs_init_named(&uf->fixture.fs, "file_dep_missing_manifest");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/spn_build/file_dep_missing_manifest",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .rc = 1 } },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = "err_manifest", .key = "name", .value = "core/spum" } },
      { .kind = ACTION_VERIFY_NO_EVENT, .verify_event = { .event = "err_unknown_pkg" } },
    },
  });
}

UTEST_F(spn_build, file_dep_custom_manifest_name) {
  tmpfs_init_named(&uf->fixture.fs, "file_dep_custom_manifest_name");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/spn_build/file_dep_custom_manifest_name",
    .copy = { "vendor/spum/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_RUN_BIN, .bin = { .name = "main", .rc = 0 } },
    },
  });
}

UTEST_F(spn_build, root_source_pin) {
  tmpfs_init_named(&uf->fixture.fs, "root_source_pin");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/spn_build/root_source_pin",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_RUN_BIN, .bin = { .name = "main", .rc = 0 } },
    },
  });
}

UTEST_F(spn_build, index_package_fetch_fails) {
  tmpfs_init_named(&uf->fixture.fs, "index_package_fetch_fails");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/spn_build/index_package_fetch_fails",
    .actions = {
      { .kind = ACTION_REMOVE_DIR, .rm = { .dir = "remote/spum" } },
      { .kind = ACTION_RUN_CLI, .cli = { "build", .rc = 1 } },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = "sync_failed", .key = "name", .value = "core/spum" } },
    },
  });
}

UTEST_F(spn_build, index_package_invalid_manifest) {
  tmpfs_init_named(&uf->fixture.fs, "index_package_invalid_manifest");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/spn_build/index_package_invalid_manifest",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .rc = 1 } },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = "err_manifest", .key = "name", .value = "core/spum" } },
    },
  });
}

UTEST_F(spn_build, file_dep_invalid_manifest) {
  tmpfs_init_named(&uf->fixture.fs, "file_dep_invalid_manifest");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/spn_build/file_dep_invalid_manifest",
    .copy = { "vendor/spum/spn.toml" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .rc = 1 } },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = "err_manifest", .key = "name", .value = "core/spum" } },
      { .kind = ACTION_VERIFY_NO_EVENT, .verify_event = { .event = "err_unknown_pkg" } },
    },
  });
}

UTEST_F(spn_build, editable_package) {
  tmpfs_init_named(&uf->fixture.fs, "editable_package");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/spn_build/editable_package",
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

UTEST_F(spn_build, top_level_system_deps) {
  tmpfs_init_named(&uf->fixture.fs, "top_level_system_deps");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/spn_build/top_level_system_deps",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = sp_str_lit("spn.lock") },
      { .kind = ACTION_RUN_BIN, .bin.name = "main" },
      { .kind = ACTION_RUN_CLI, .cli = { "build", .args = { "--force" } } },
      { .kind = ACTION_RUN_BIN, .bin.name = "main" },
    },
  });
}

UTEST_F(spn_build, add_bin) {
  tmpfs_init_named(&uf->fixture.fs, "add_exe_smoke");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/spn_build/add_exe/smoke",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_RUN_BIN, .bin.name = "foo" },
    },
  });
}

UTEST_F(spn_build, source_glob) {
  tmpfs_init_named(&uf->fixture.fs, "source_glob");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/spn_build/source_glob",
    .copy = { "src" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_RUN_BIN, .bin.name = "main" },
    },
  });
}

UTEST_F(spn_build, static_lib) {
  tmpfs_init_named(&uf->fixture.fs, "static_lib");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/spn_build/static_lib",
    .copy = { "mylib.c" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .args = { "mylib" } } },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = sp_str_lit("build/debug/store/lib/libmylib.a") },
    },
  });
}

UTEST_F(spn_build, shared_lib) {
  tmpfs_init_named(&uf->fixture.fs, "shared_lib");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/spn_build/shared_lib",
    .copy = { "spum.c" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .args = { "spum" } } },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = sp_str_lit("build/debug/store/lib/libspum.so") },
    },
  });
}

UTEST_F(spn_build, consume_static_lib) {
  tmpfs_init_named(&uf->fixture.fs, "consume_static_lib");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/spn_build/consume_static_lib",
    .copy = { "packages/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .args = { "-p", "debug" } } },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = sp_str_lit("build/debug/store/lib/libspum.a") },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = sp_str_lit("build/debug/store/bin/main") },
    },
  });
}

UTEST_F(spn_build, consume_transitive_static) {
  tmpfs_init_named(&uf->fixture.fs, "consume_transitive_static");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/spn_build/consume_transitive_static",
    .copy = { "packages/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .args = { "-p", "debug" } } },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = sp_str_lit("build/debug/store/lib/libspum.a") },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = sp_str_lit("build/debug/store/lib/libspam.a") },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = sp_str_lit("build/debug/store/bin/main") },
    },
  });
}

UTEST_F(spn_build, consume_system_dep_lib) {
  tmpfs_init_named(&uf->fixture.fs, "consume_system_dep_lib");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/spn_build/consume_system_dep_lib",
    .copy = { "packages/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .args = { "-p", "static" } } },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = sp_str_lit("build/static/store/lib/libspum.a") },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = sp_str_lit("build/static/store/bin/main") },
    },
  });
}

UTEST_F(spn_build, consume_shared_lib) {
  tmpfs_init_named(&uf->fixture.fs, "consume_shared_lib");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/spn_build/consume_shared_lib",
    .copy = { "packages/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .args = { "-p", "debug" } } },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = sp_str_lit("build/debug/store/lib/libspum.so") },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = sp_str_lit("build/debug/store/bin/main") },
    },
  });
}

UTEST_F(spn_build, consume_source_lib) {
  tmpfs_init_named(&uf->fixture.fs, "consume_source_lib");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/spn_build/consume_source_lib",
    .copy = { "packages/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .args = { "-p", "debug" } } },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = sp_str_lit("build/debug/store/bin/main") },
    },
  });
}

UTEST_F(spn_build, consume_static_lib_static_profile) {
  tmpfs_init_named(&uf->fixture.fs, "consume_static_lib_static_profile");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/spn_build/consume_static_lib",
    .copy = { "packages/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .args = { "-p", "static" } } },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = sp_str_lit("build/static/store/lib/libspum.a") },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = sp_str_lit("build/static/store/bin/main") },
    },
  });
}

UTEST_F(spn_build, consume_shared_lib_static_profile) {
  tmpfs_init_named(&uf->fixture.fs, "consume_shared_lib_static_profile");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/spn_build/consume_shared_lib",
    .copy = { "packages/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .args = { "-p", "static" }, .rc = 1 } },
    },
  });
}

UTEST_F(spn_build, consume_source_lib_static_profile) {
  tmpfs_init_named(&uf->fixture.fs, "consume_source_lib_static_profile");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/spn_build/consume_source_lib",
    .copy = { "packages/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .args = { "-p", "static" } } },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = sp_str_lit("build/static/store/bin/main") },
    },
  });
}

UTEST_F(spn_build, consume_multi_kind_shared) {
  tmpfs_init_named(&uf->fixture.fs, "consume_multi_kind_shared");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/spn_build/consume_multi_kind_shared",
    .copy = { "packages/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = sp_str_lit("build/debug/store/lib/libspum.so") },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = sp_str_lit("build/debug/store/bin/main") },
    },
  });
}

UTEST_F(spn_build, consume_multi_kind_static) {
  tmpfs_init_named(&uf->fixture.fs, "consume_multi_kind_static");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/spn_build/consume_multi_kind_static",
    .copy = { "packages/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = sp_str_lit("build/debug/store/lib/libspum.a") },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = sp_str_lit("build/debug/store/bin/main") },
    },
  });
}

UTEST_F(spn_build, consume_multi_kind_source) {
  tmpfs_init_named(&uf->fixture.fs, "consume_multi_kind_source");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/spn_build/consume_multi_kind_source",
    .copy = { "packages/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = sp_str_lit("build/debug/store/bin/main") },
    },
  });
}

UTEST_F(spn_build, consume_multi_kind_default) {
  tmpfs_init_named(&uf->fixture.fs, "consume_multi_kind_default");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/spn_build/consume_multi_kind_default",
    .copy = { "packages/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = sp_str_lit("build/debug/store/bin/main") },
    },
  });
}

UTEST_F(spn_build, consume_kind_not_supported) {
  tmpfs_init_named(&uf->fixture.fs, "consume_kind_not_supported");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/spn_build/consume_kind_not_supported",
    .copy = { "packages/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .rc = 1 } },
    },
  });
}

UTEST_F(spn_build, schema_missing_required_package_version) {
  tmpfs_init_named(&uf->fixture.fs, "schema_missing_required_package_version");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/schema/missing_required_package_version",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .rc = 1 } },
    },
  });
}

UTEST_F(spn_build, schema_missing_required_package_name) {
  tmpfs_init_named(&uf->fixture.fs, "schema_missing_required_package_name");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/schema/missing_required_package_name",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .rc = 1 } },
    },
  });
}

UTEST_F(spn_build, schema_wrong_type_required_package_version) {
  tmpfs_init_named(&uf->fixture.fs, "schema_wrong_type_required_package_version");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/schema/wrong_type_required_package_version",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .rc = 1 } },
    },
  });
}

UTEST_F(spn_build, add_define) {
  tmpfs_init_named(&uf->fixture.fs, "add_define");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/spn_build/add_define",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_RUN_BIN, .bin.name = "main" },
    },
  });
}

UTEST_F(spn_build, add_include) {
  tmpfs_init_named(&uf->fixture.fs, "add_include");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/spn_build/add_include",
    .copy = { "include/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_RUN_BIN, .bin.name = "main" },
    },
  });
}

UTEST_F(spn_build, add_system_dep) {
  tmpfs_init_named(&uf->fixture.fs, "add_system_dep");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/spn_build/add_system_dep",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_RUN_BIN, .bin.name = "main" },
    },
  });
}

UTEST_F(spn_build, add_test) {
  tmpfs_init_named(&uf->fixture.fs, "add_test");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/spn_build/add_test",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_RUN_BIN, .bin.name = "test" },
    },
  });
}

UTEST_F(spn_build, run_script_manifest) {
  tmpfs_init_named(&uf->fixture.fs, "run_script_manifest");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/spn_run/manifest",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_VERIFY_NOT_EXISTS, .verify_not_exists.file = sp_str_lit("build/debug/store/bin/main") },
      { .kind = ACTION_RUN_CLI, .cli = { "build", .args = { "main" } } },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = sp_str_lit("build/debug/store/bin/main") },
      { .kind = ACTION_REMOVE_DIR, .rm = { .dir = "build" } },
      { .kind = ACTION_RUN_CLI, .cli = { "run", .args = { "main" } } },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = sp_str_lit("build/debug/store/bin/main") },
      { .kind = ACTION_VERIFY_CONTENT, .verify_content = { .file = sp_str_lit("ran.txt"), .content = sp_str_lit("script\n") } },
    },
  });
}

UTEST_F(spn_build, run_script_name_c) {
  tmpfs_init_named(&uf->fixture.fs, "run_script_name_c");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/spn_run/script_name_c",
    .copy = { "script.c" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "run", .args = { "main.c" } } },
      { .kind = ACTION_VERIFY_CONTENT, .verify_content = { .file = sp_str_lit("ran.txt"), .content = sp_str_lit("script-name-c\n") } },
    },
  });
}

UTEST_F(spn_build, api_basic_node) {
  tmpfs_init_named(&uf->fixture.fs, "api_basic_node");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/api/basic_node",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_INCLUDE, .verify_include.file = sp_str_lit("version.h") },
      { .kind = ACTION_RUN_BIN, .bin.name = "basic_node" },
    },
  });
}

UTEST_F(spn_build, api_chained_nodes) {
  tmpfs_init_named(&uf->fixture.fs, "api_chained_nodes");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/api/chained_nodes",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_RUN_BIN, .bin.name = "chained_nodes" },
    },
  });
}

UTEST_F(spn_build, api_cross_package) {
  tmpfs_init_named(&uf->fixture.fs, "api_cross_package");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/api/cross_package",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_PKG_LOCKED, .verify_locked.name = "core/spum" },
      { .kind = ACTION_RUN_BIN, .bin.name = "cross_package" },
    },
  });
}

UTEST_F(spn_build, api_diamond_deps) {
  tmpfs_init_named(&uf->fixture.fs, "api_diamond_deps");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/api/diamond_deps",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_RUN_BIN, .bin.name = "diamond_deps" },
    },
  });
}

UTEST_F(spn_build, api_fan_in) {
  tmpfs_init_named(&uf->fixture.fs, "api_fan_in");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/api/fan_in",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_RUN_BIN, .bin.name = "fan_in" },
    },
  });
}

UTEST_F(spn_build, api_multi_output) {
  tmpfs_init_named(&uf->fixture.fs, "api_multi_output");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/api/multi_output",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_RUN_BIN, .bin.name = "multi_output" },
    },
  });
}

UTEST_F(spn_build, api_object_lib) {
  tmpfs_init_named(&uf->fixture.fs, "api_object_lib");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/api/object_lib",
    .copy = { "packages/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      // object libs publish their objects to lib/, preserving source-relative paths
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = sp_str_lit("build/debug/store/lib/rt/extra.o") },
      // ditto for an object lib declared from the build script instead of the manifest
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = sp_str_lit("build/debug/store/lib/rt/extra2.o") },
      // an unlinked archive still builds and installs
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = sp_str_lit("build/debug/store/lib/libblob.a") },
      { .kind = ACTION_RUN_BIN, .bin.name = "object_lib" },
    },
  });
}

UTEST_F(spn_build, api_node_linking) {
  tmpfs_init_named(&uf->fixture.fs, "api_node_linking");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/api/node_linking",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_RUN_BIN, .bin.name = "node_linking" },
    },
  });
}

UTEST_F(spn_build, api_orphan_outputs) {
  tmpfs_init_named(&uf->fixture.fs, "api_orphan_outputs");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/api/orphan_outputs",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_RUN_BIN, .bin.name = "orphan_outputs" },
    },
  });
}

UTEST_F(spn_build, api_stamp_chain) {
  tmpfs_init_named(&uf->fixture.fs, "api_stamp_chain");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/api/stamp_chain",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_RUN_BIN, .bin.name = "stamp_chain" },
    },
  });
}

UTEST_F(spn_build, api_stamp_input) {
  tmpfs_init_named(&uf->fixture.fs, "api_stamp_input");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/api/stamp_input",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_RUN_BIN, .bin.name = "stamp_input" },
    },
  });
}

UTEST_F(spn_build, api_user_data) {
  tmpfs_init_named(&uf->fixture.fs, "api_user_data");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/api/user_data",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_RUN_BIN, .bin.name = "user_data" },
    },
  });
}

UTEST_F(spn_build, api_configure_table) {
  tmpfs_init_named(&uf->fixture.fs, "api_configure_table");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/api/configure_table",
    .copy = { "tools", "include" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_RUN_BIN, .bin.name = "configure_table" },
    },
  });
}

UTEST_F(spn_build, api_configure_error) {
  tmpfs_init_named(&uf->fixture.fs, "api_configure_error");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/api/configure_error",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .rc = 1 } },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = "err" } },
    },
  });
}

UTEST_F(spn_build, api_wrong_handle) {
  tmpfs_init_named(&uf->fixture.fs, "api_wrong_handle");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/api/wrong_handle",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_RUN_BIN, .bin.name = "wrong_handle" },
    },
  });
}

UTEST_F(spn_build, api_stale_config) {
  tmpfs_init_named(&uf->fixture.fs, "api_stale_config");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/api/stale_config",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_RUN_BIN, .bin.name = "stale_config" },
    },
  });
}

UTEST_F(spn_build, api_build_script) {
  tmpfs_init_named(&uf->fixture.fs, "api_build_script");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/api/build_script",
    .copy = { "tools", "include", "vendor" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_INCLUDE, .verify_include.file = sp_str_lit("version.h") },
      { .kind = ACTION_RUN_BIN, .bin.name = "build_script" },
      // The node fn lives in the build module; when only the node's output is
      // stale, the module compile is skipped and the fn must still resolve
      { .kind = ACTION_REMOVE_FILE, .rm.file = "build/debug/work/build_script/version.h" },
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = sp_str_lit("build/debug/work/build_script/version.h") },
    },
  });
}

UTEST_F(spn_build, api_default_script) {
  tmpfs_init_named(&uf->fixture.fs, "api_default_script");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/api/default_script",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_INCLUDE, .verify_include.file = sp_str_lit("version.h") },
      { .kind = ACTION_RUN_BIN, .bin.name = "default_script" },
    },
  });
}

UTEST_F(spn_build, api_build_deps) {
  tmpfs_init_named(&uf->fixture.fs, "api_build_deps");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/api/build_deps",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_PKG_LOCKED, .verify_locked.name = "core/spum" },
      { .kind = ACTION_RUN_BIN, .bin.name = "build_deps" },
    },
  });
}

UTEST_F(spn_build, embed) {
  tmpfs_init_named(&uf->fixture.fs, "embed");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/spn_build/embed",
    .copy = { "hello.txt" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_RUN_BIN, .bin = { .name = "main", .rc = 0 } },
    },
  });
}

struct build_log {
  fixture_t fixture;
};

UTEST_F_SETUP(build_log) {
#if defined(SPN_TEST_ROOT) && defined(SPN_TEST_BIN)
  uf->fixture.paths.root = sp_str_lit(SPN_TEST_ROOT);
  uf->fixture.paths.spn = sp_str_lit(SPN_TEST_BIN);
#else
  sp_mem_t mem = sp_mem_os_new();
  uf->fixture.paths.root = sp_fs_get_cwd(mem);
  uf->fixture.paths.spn = sp_fs_join_path(mem, uf->fixture.paths.root, sp_str_lit("build/debug/store/bin/spn"));
#endif
  ASSERT_TRUE(sp_fs_exists(uf->fixture.paths.spn));
}

UTEST_F_TEARDOWN(build_log) {
}

UTEST_F(build_log, clean) {
  tmpfs_init_named(&uf->fixture.fs, "build_log_clean");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/log/clean",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_NOT_EXISTS, .verify_not_exists.file = sp_str_lit("build/debug/work/log_clean/log_clean.build.log") },
    },
  });
}

UTEST_F(build_log, warn) {
  tmpfs_init_named(&uf->fixture.fs, "build_log_warn");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/log/warn",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_FILE_NONEMPTY, .verify_file_nonempty.file = sp_str_lit("build/debug/work/log_warn/log_warn.build.log") },
      { .kind = ACTION_VERIFY_FILE_CONTAINS, .verify_file_contains = { .file = sp_str_lit("build/debug/work/log_warn/log_warn.build.log"), .needle = sp_str_lit("spn-log-probe-warn") } },
    },
  });
}

UTEST_F(build_log, error) {
  tmpfs_init_named(&uf->fixture.fs, "build_log_error");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/log/error",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .rc = 1 } },
      { .kind = ACTION_VERIFY_FILE_NONEMPTY, .verify_file_nonempty.file = sp_str_lit("build/debug/work/log_error/log_error.build.log") },
      { .kind = ACTION_VERIFY_FILE_CONTAINS, .verify_file_contains = { .file = sp_str_lit("build/debug/work/log_error/log_error.build.log"), .needle = sp_str_lit("spn-log-probe-error") } },
    },
  });
}

UTEST_F(build_log, link_error) {
  tmpfs_init_named(&uf->fixture.fs, "build_log_link_error");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/log/link_error",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .rc = 1 } },
      { .kind = ACTION_VERIFY_FILE_NONEMPTY, .verify_file_nonempty.file = sp_str_lit("build/debug/work/log_link_error/log_link_error.build.log") },
      { .kind = ACTION_VERIFY_FILE_CONTAINS, .verify_file_contains = { .file = sp_str_lit("build/debug/work/log_link_error/log_link_error.build.log"), .needle = sp_str_lit("spn_log_missing_symbol") } },
    },
  });
}

UTEST_F(build_log, warn_multi) {
  tmpfs_init_named(&uf->fixture.fs, "build_log_warn_multi");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/log/warn_multi",
    .copy = { "a.c", "b.c" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_FILE_CONTAINS, .verify_file_contains = { .file = sp_str_lit("build/debug/work/log_warn_multi/log_warn_multi.build.log"), .needle = sp_str_lit("spn-log-probe-main") } },
      { .kind = ACTION_VERIFY_FILE_CONTAINS, .verify_file_contains = { .file = sp_str_lit("build/debug/work/log_warn_multi/log_warn_multi.build.log"), .needle = sp_str_lit("spn-log-probe-a") } },
      { .kind = ACTION_VERIFY_FILE_CONTAINS, .verify_file_contains = { .file = sp_str_lit("build/debug/work/log_warn_multi/log_warn_multi.build.log"), .needle = sp_str_lit("spn-log-probe-b") } },
    },
  });
}

UTEST_F(build_log, preserved_on_cache_hit) {
  tmpfs_init_named(&uf->fixture.fs, "build_log_preserved");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/log/warn",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_FILE_CONTAINS, .verify_file_contains = { .file = sp_str_lit("build/debug/work/log_warn/log_warn.build.log"), .needle = sp_str_lit("spn-log-probe-warn") } },
      { .kind = ACTION_SNAPSHOT_MTIME, .snapshot_mtime.file = sp_str_lit("build/debug/work/log_warn/log_warn.build.log") },
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_MTIME_UNCHANGED, .verify_mtime.file = sp_str_lit("build/debug/work/log_warn/log_warn.build.log") },
    },
  });
}

UTEST_F(build_log, script_log_hidden_normally) {
  tmpfs_init_named(&uf->fixture.fs, "build_log_script_log_hidden");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/log/script_log",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_CLI_NOT_CONTAINS, .verify_cli.needle = sp_str_lit("spn-script-probe-log") },
    },
  });
}

UTEST_F(build_log, script_log_shown_verbose) {
  tmpfs_init_named(&uf->fixture.fs, "build_log_script_log_verbose");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/log/script_log",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .args = { "--verbose" } } },
      { .kind = ACTION_VERIFY_CLI_CONTAINS, .verify_cli.needle = sp_str_lit("spn-script-probe-log") },
    },
  });
}

UTEST_F(build_log, script_log_shown_on_failure) {
  tmpfs_init_named(&uf->fixture.fs, "build_log_script_log_failure");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/log/script_log_fail",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .rc = 1 } },
      { .kind = ACTION_VERIFY_CLI_CONTAINS, .verify_cli.needle = sp_str_lit("spn-script-probe-fail") },
    },
  });
}

UTEST_F(build_log, rewritten_on_rebuild) {
  tmpfs_init_named(&uf->fixture.fs, "build_log_rewritten");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/log/warn",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_SNAPSHOT_MTIME, .snapshot_mtime.file = sp_str_lit("build/debug/work/log_warn/log_warn.build.log") },
      { .kind = ACTION_CREATE_FILE, .create = { .file = sp_str_lit("main.c"), .content = sp_str_lit("#warning \"spn-log-probe-rebuilt\"\nint main(void) { return 0; }\n") } },
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .args = { "--force" } } },
      { .kind = ACTION_VERIFY_MTIME_CHANGED, .verify_mtime.file = sp_str_lit("build/debug/work/log_warn/log_warn.build.log") },
      { .kind = ACTION_VERIFY_FILE_CONTAINS, .verify_file_contains = { .file = sp_str_lit("build/debug/work/log_warn/log_warn.build.log"), .needle = sp_str_lit("spn-log-probe-rebuilt") } },
      { .kind = ACTION_VERIFY_FILE_NOT_CONTAINS, .verify_file_not_contains = { .file = sp_str_lit("build/debug/work/log_warn/log_warn.build.log"), .needle = sp_str_lit("spn-log-probe-warn") } },
    },
  });
}
