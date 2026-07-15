SPN_TEST_SUITE(script)

UTEST_F(script, basic_node) {
  tmpfs_init_named(&uf->fixture.fs, "script_basic_node");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/script/basic_node",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_INCLUDE, .verify_include.file = sp_str_lit("version.h") },
      { .kind = ACTION_RUN_BIN, .bin.name = "basic_node" },
    },
  });
}

UTEST_F(script, chained_nodes) {
  tmpfs_init_named(&uf->fixture.fs, "script_chained_nodes");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/script/chained_nodes",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_RUN_BIN, .bin.name = "chained_nodes" },
    },
  });
}

UTEST_F(script, cross_package) {
  tmpfs_init_named(&uf->fixture.fs, "script_cross_package");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/script/cross_package",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_PKG_LOCKED, .verify_locked.name = "core/spum" },
      { .kind = ACTION_RUN_BIN, .bin.name = "cross_package" },
    },
  });
}

UTEST_F(script, diamond_deps) {
  tmpfs_init_named(&uf->fixture.fs, "script_diamond_deps");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/script/diamond_deps",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_RUN_BIN, .bin.name = "diamond_deps" },
    },
  });
}

UTEST_F(script, fan_in) {
  tmpfs_init_named(&uf->fixture.fs, "script_fan_in");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/script/fan_in",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_RUN_BIN, .bin.name = "fan_in" },
    },
  });
}

UTEST_F(script, multi_output) {
  tmpfs_init_named(&uf->fixture.fs, "script_multi_output");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/script/multi_output",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_RUN_BIN, .bin.name = "multi_output" },
    },
  });
}

UTEST_F(script, object_lib) {
  tmpfs_init_named(&uf->fixture.fs, "script_object_lib");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/script/object_lib",
    .copy = { "packages/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      // object libs publish their objects to lib/, preserving source-relative paths
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = store_file("lib/rt/extra.c.o") },
      // ditto for an object lib declared from the build script instead of the manifest
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = store_file("lib/rt/extra2.c.o") },
      // an unlinked archive still builds and installs
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = static_lib("blob") },
      { .kind = ACTION_RUN_BIN, .bin.name = "object_lib" },
    },
  });
}

UTEST_F(script, node_linking) {
  tmpfs_init_named(&uf->fixture.fs, "script_node_linking");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/script/node_linking",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_RUN_BIN, .bin.name = "node_linking" },
    },
  });
}

UTEST_F(script, orphan_outputs) {
  tmpfs_init_named(&uf->fixture.fs, "script_orphan_outputs");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/script/orphan_outputs",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_RUN_BIN, .bin.name = "orphan_outputs" },
    },
  });
}

UTEST_F(script, stamp_chain) {
  tmpfs_init_named(&uf->fixture.fs, "script_stamp_chain");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/script/stamp_chain",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_RUN_BIN, .bin.name = "stamp_chain" },
    },
  });
}

UTEST_F(script, stamp_input) {
  tmpfs_init_named(&uf->fixture.fs, "script_stamp_input");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/script/stamp_input",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_RUN_BIN, .bin.name = "stamp_input" },
    },
  });
}

UTEST_F(script, user_data) {
  tmpfs_init_named(&uf->fixture.fs, "script_user_data");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/script/user_data",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_RUN_BIN, .bin.name = "user_data" },
    },
  });
}

UTEST_F(script, configure_table) {
  tmpfs_init_named(&uf->fixture.fs, "script_configure_table");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/script/configure_table",
    .copy = { "tools", "include" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_RUN_BIN, .bin.name = "configure_table" },
    },
  });
}

UTEST_F(script, configure_error) {
  tmpfs_init_named(&uf->fixture.fs, "script_configure_error");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/script/configure_error",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .rc = 1 } },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = "err" } },
    },
  });
}

UTEST_F(script, wrong_handle) {
  tmpfs_init_named(&uf->fixture.fs, "script_wrong_handle");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/script/wrong_handle",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_RUN_BIN, .bin.name = "wrong_handle" },
    },
  });
}

UTEST_F(script, stale_config) {
  tmpfs_init_named(&uf->fixture.fs, "script_stale_config");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/script/stale_config",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_RUN_BIN, .bin.name = "stale_config" },
    },
  });
}

UTEST_F(script, build_script) {
  tmpfs_init_named(&uf->fixture.fs, "script_build_script");

  run_rebuild_test(utest_result, &uf->fixture, (rebuild_test_t) {
    .project = "test/integration/fixtures/script/build_script",
    .copy = { "tools", "include", "vendor" },
    .first = {
      .args = { "build" },
      .expect = {
        .bin.name = "build_script",
        .files = {
          { .file = sp_str_lit("compile_commands.json"), .contains = { "tools/configure.c", "tools/build.c" } },
        },
        .exists = {
          sp_str_lit("build/wasm32-wasi/work/build_script/spn/object/build/tools/a/main.c.o"),
          sp_str_lit("build/wasm32-wasi/work/build_script/spn/object/build/tools/b/main.c.o"),
          store_file("include/version.h"),
        },
      },
    },
    .rebuilds = {
      {
        .change.remove_files = { work_file("build_script/version.h") },
        .command = {
          .args = { "build" },
          .expect.exists = { work_file("build_script/version.h") },
        },
      },
    },
  });
}

UTEST_F(script, default_script) {
  tmpfs_init_named(&uf->fixture.fs, "script_default_script");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/script/default_script",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_INCLUDE, .verify_include.file = sp_str_lit("version.h") },
      { .kind = ACTION_RUN_BIN, .bin.name = "default_script" },
    },
  });
}

UTEST_F(script, build_deps) {
  tmpfs_init_named(&uf->fixture.fs, "script_build_deps");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/script/build_deps",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_PKG_LOCKED, .verify_locked.name = "core/spum" },
      { .kind = ACTION_VERIFY_DIR_COUNT, .verify_dir_count = { .dir = ".home/storage/cache/store/core/spum", .count = 1 } },
      { .kind = ACTION_VERIFY_EVENT_COUNT, .verify_event_count = { .event = "user_log", .key = "message", .value = "spum configure", .count = 1 } },
      { .kind = ACTION_VERIFY_NOT_EXISTS, .verify_not_exists.file = sp_str_lit("build/debug/store/include/spum.h") },
      { .kind = ACTION_RUN_BIN, .bin.name = "build_deps" },
    },
  });
}

UTEST_F(script, dual_ctx) {
  tmpfs_init_named(&uf->fixture.fs, "script_dual_ctx");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/script/dual_ctx",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_EVENT_COUNT, .verify_event_count = { .event = "user_log", .key = "message", .value = "gamma configure", .count = 2 } },
      { .kind = ACTION_VERIFY_DIR_COUNT, .verify_dir_count = { .dir = ".home/storage/cache/store/core/gamma", .count = 2 } },
      { .kind = ACTION_RUN_BIN, .bin.name = "dual_ctx" },
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_EVENT_COUNT, .verify_event_count = { .event = "user_log", .key = "message", .value = "gamma configure", .count = 2 } },
    },
  });
}

UTEST_F(script, program_name_context) {
  tmpfs_init_named(&uf->fixture.fs, "script_program_name_context");

  run_command_test(utest_result, &uf->fixture, (command_test_t) {
    .project = "test/integration/fixtures/script/module_name_collision",
    .args = { "build" },
    .expect.bin.name = "configure",
  });
}

UTEST_F(script, build_dep_closure) {
  tmpfs_init_named(&uf->fixture.fs, "script_build_dep_closure");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/script/build_dep_closure",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_PKG_LOCKED, .verify_locked.name = "core/alpha" },
      { .kind = ACTION_VERIFY_PKG_LOCKED, .verify_locked.name = "core/beta" },
      { .kind = ACTION_VERIFY_DIR_COUNT, .verify_dir_count = { .dir = ".home/storage/cache/store/core/alpha", .count = 1 } },
      { .kind = ACTION_VERIFY_DIR_COUNT, .verify_dir_count = { .dir = ".home/storage/cache/store/core/beta", .count = 1 } },
      { .kind = ACTION_RUN_BIN, .bin.name = "build_dep_closure" },
    },
  });
}

UTEST_F(script, add_define) {
  tmpfs_init_named(&uf->fixture.fs, "script_add_define");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/script/add_define",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_RUN_BIN, .bin.name = "main" },
    },
  });
}

UTEST_F(script, add_include) {
  tmpfs_init_named(&uf->fixture.fs, "script_add_include");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/script/add_include",
    .copy = { "include/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_RUN_BIN, .bin.name = "main" },
    },
  });
}

UTEST_F(script, add_system_dep) {
  tmpfs_init_named(&uf->fixture.fs, "script_add_system_dep");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/script/add_system_dep",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_RUN_BIN, .bin.name = "main" },
    },
  });
}

UTEST_F(script, add_test) {
  tmpfs_init_named(&uf->fixture.fs, "script_add_test");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/script/add_test",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_RUN_TEST, .bin.name = "test" },
    },
  });
}

UTEST_F(script, add_exe) {
  tmpfs_init_named(&uf->fixture.fs, "script_add_exe");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/script/add_exe",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_RUN_BIN, .bin.name = "foo" },
    },
  });
}

UTEST_F(script, embed) {
  tmpfs_init_named(&uf->fixture.fs, "script_embed");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/script/embed",
    .copy = { "hello.txt" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_RUN_BIN, .bin = { .name = "main", .rc = 0 } },
    },
  });
}
