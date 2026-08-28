#include "harness.h"

sp_test(script, basic_node) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/script/basic_node",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_INCLUDE, .verify_include.file = sp_str_lit("version.h") },
      { .kind = ACTION_RUN_BIN, .bin.name = "basic_node" },
    },
  });
}

sp_test(script, package_discovery) {
  return sp_test_skip(t, "I disabled WASI hooks until I figure out how to cleanly patch WAMR");

  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/script/package_discovery",
    .copy = { "data.txt" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_FILE_CONTAINS, .verify_file_contains = { .file = store_file("misc/data"), .needle = sp_str_lit("A") } },
      { .kind = ACTION_VERIFY_FILE_CONTAINS, .verify_file_contains = { .file = store_file("misc/witness"), .needle = sp_str_lit("R") } },
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_FILE_NOT_CONTAINS, .verify_file_not_contains = { .file = store_file("misc/witness"), .needle = sp_str_lit("RR") } },
      { .kind = ACTION_CREATE_FILE, .create = { .file = sp_str_lit("data.txt"), .content = sp_str_lit("B") } },
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_FILE_CONTAINS, .verify_file_contains = { .file = store_file("misc/data"), .needle = sp_str_lit("B") } },
      { .kind = ACTION_VERIFY_FILE_CONTAINS, .verify_file_contains = { .file = store_file("misc/witness"), .needle = sp_str_lit("RR") } },
    },
  });
}

sp_test(script, abi_discovery) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/script/abi_discovery",
    .copy = { "data" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_FILE_CONTAINS, .verify_file_contains = { .file = store_file("misc/data/D.txt"), .needle = sp_str_lit("A") } },
      { .kind = ACTION_VERIFY_FILE_CONTAINS, .verify_file_contains = { .file = store_file("misc/witness"), .needle = sp_str_lit("R") } },
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_FILE_NOT_CONTAINS, .verify_file_not_contains = { .file = store_file("misc/witness"), .needle = sp_str_lit("RR") } },
      { .kind = ACTION_CREATE_FILE, .create = { .file = sp_str_lit("data/D.txt"), .content = sp_str_lit("B") } },
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_FILE_CONTAINS, .verify_file_contains = { .file = store_file("misc/data/D.txt"), .needle = sp_str_lit("B") } },
      { .kind = ACTION_VERIFY_FILE_CONTAINS, .verify_file_contains = { .file = store_file("misc/witness"), .needle = sp_str_lit("RR") } },
      { .kind = ACTION_CREATE_FILE, .create = { .file = sp_str_lit("data/E.txt"), .content = sp_str_lit("C") } },
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_FILE_CONTAINS, .verify_file_contains = { .file = store_file("misc/data/E.txt"), .needle = sp_str_lit("C") } },
      { .kind = ACTION_VERIFY_FILE_CONTAINS, .verify_file_contains = { .file = store_file("misc/witness"), .needle = sp_str_lit("RRR") } },
    },
  });
}

sp_test(script, relative_path_rejected) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/script/relative_path",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .rc = 1 } },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = SPN_EVENT_ERR, .key = "kind", .value = "wasm_module_call_failed" } },
    },
  });
}

sp_test(script, chained_nodes) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/script/chained_nodes",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_RUN_BIN, .bin.name = "chained_nodes" },
    },
  });
}

sp_test(script, cross_package) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/script/cross_package",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_PKG_LOCKED, .verify_locked.name = "core/spum" },
      { .kind = ACTION_RUN_BIN, .bin.name = "cross_package" },
    },
  });
}

sp_test(script, diamond_deps) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/script/diamond_deps",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_RUN_BIN, .bin.name = "diamond_deps" },
    },
  });
}

sp_test(script, fan_in) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/script/fan_in",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_RUN_BIN, .bin.name = "fan_in" },
    },
  });
}

sp_test(script, multi_output) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/script/multi_output",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_RUN_BIN, .bin.name = "multi_output" },
    },
  });
}

sp_test(script, object_lib) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/script/object_lib",
    .copy = { "packages/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      // object libs publish their objects to lib/, preserving source-relative paths
      { .kind = ACTION_VERIFY_EXISTS, .exists = store_file("lib/manifest/rt/extra.c.o") },
      // ditto for an object lib declared from the build script instead of the manifest
      { .kind = ACTION_VERIFY_EXISTS, .exists = store_file("lib/manifest/rt/extra2.c.o") },
      // an unlinked archive still builds and installs
      { .kind = ACTION_VERIFY_EXISTS, .exists = static_lib("blob") },
      { .kind = ACTION_RUN_BIN, .bin.name = "object_lib" },
    },
  });
}

sp_test(script, node_linking) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/script/node_linking",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_RUN_BIN, .bin.name = "node_linking" },
    },
  });
}

sp_test(script, orphan_outputs) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/script/orphan_outputs",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_RUN_BIN, .bin.name = "orphan_outputs" },
    },
  });
}

sp_test(script, stamp_chain) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/script/stamp_chain",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_RUN_BIN, .bin.name = "stamp_chain" },
    },
  });
}

sp_test(script, stamp_input) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/script/stamp_input",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_RUN_BIN, .bin.name = "stamp_input" },
    },
  });
}

sp_test(script, configure_table) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/script/configure_table",
    .copy = { "tools", "include" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_RUN_BIN, .bin.name = "configure_table" },
    },
  });
}

sp_test(script, configure_glob) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/script/configure_glob",
    .copy = { "tools" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_EVENT_COUNT, .verify_event_count = { .event = SPN_EVENT_USER_LOG, .key = "message", .value = "G", .count = 1 } },
    },
  });
}

sp_test(script, configure_dead_glob) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/script/configure_dead_glob",
    .copy = { "tools" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .rc = 1 } },
      { .kind = ACTION_VERIFY_RESULT, .verify_result = { .err = SPN_ERR_CONFIGURE_SOURCE_GLOB } },
    },
  });
}

sp_test(script, configure_missing_source) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/script/configure_missing_source",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .rc = 1 } },
      { .kind = ACTION_VERIFY_RESULT, .verify_result = { .err = SPN_ERR_CONFIGURE_SOURCE_MISSING } },
    },
  });
}

sp_test(script, configure_error) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/script/configure_error",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .rc = 1 } },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = SPN_EVENT_ERR } },
    },
  });
}

sp_test(script, wrong_handle) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/script/wrong_handle",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_RUN_BIN, .bin.name = "wrong_handle" },
    },
  });
}

sp_test(script, stale_config) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/script/stale_config",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_RUN_BIN, .bin.name = "stale_config" },
    },
  });
}

sp_test(script, build_script) {
  return run_rebuild_test(t, (rebuild_test_t) {
    .project = "test/integration/fixtures/script/build_script",
    .copy = { "tools", "include", "vendor" },
    .first = {
      .args = { "build" },
      .expect = {
        .bin.name = "build_script",
        .cc = {
          { .args = { "tools/configure.c" } },
          { .args = { "tools/build.c" } },
        },
        .exists = {
          sp_str_lit("build/wasm32-wasi-musl/.spn/build_script/object/build/build/manifest/tools/a/main.c.o"),
          sp_str_lit("build/wasm32-wasi-musl/.spn/build_script/object/build/build/manifest/tools/b/main.c.o"),
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

sp_test(script, default_script) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/script/default_script",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_INCLUDE, .verify_include.file = sp_str_lit("version.h") },
      { .kind = ACTION_RUN_BIN, .bin.name = "default_script" },
    },
  });
}

sp_test(script, build_deps) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/script/build_deps",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_PKG_LOCKED, .verify_locked.name = "core/spum" },
      { .kind = ACTION_VERIFY_DIR_COUNT, .verify_dir_count = { .dir = ".home/storage/cache/store/core/spum", .count = 1 } },
      { .kind = ACTION_VERIFY_EVENT_COUNT, .verify_event_count = { .event = SPN_EVENT_USER_LOG, .key = "message", .value = "spum configure", .count = 1 } },
      { .kind = ACTION_VERIFY_NOT_EXISTS, .exists = sp_str_lit("build/debug/store/include/spum.h") },
      { .kind = ACTION_RUN_BIN, .bin.name = "build_deps" },
    },
  });
}

sp_test(script, dual_ctx) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/script/dual_ctx",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_EVENT_COUNT, .verify_event_count = { .event = SPN_EVENT_USER_LOG, .key = "message", .value = "gamma configure", .count = 2 } },
      { .kind = ACTION_VERIFY_DIR_COUNT, .verify_dir_count = { .dir = ".home/storage/cache/store/core/gamma", .count = 2 } },
      { .kind = ACTION_RUN_BIN, .bin.name = "dual_ctx" },
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_EVENT_COUNT, .verify_event_count = { .event = SPN_EVENT_USER_LOG, .key = "message", .value = "gamma configure", .count = 2 } },
    },
  });
}

sp_test(script, program_name_context) {
  return run_command_test(t, (command_test_t) {
    .project = "test/integration/fixtures/script/module_name_collision",
    .args = { "build" },
    .expect.bin.name = "configure",
  });
}

sp_test(script, build_dep_closure) {
  return run_test(t, (test_t) {
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

sp_test(script, build_dep_static) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/script/build_dep_static",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_PKG_LOCKED, .verify_locked.name = "core/spum" },
      { .kind = ACTION_VERIFY_DIR_COUNT, .verify_dir_count = { .dir = ".home/storage/cache/store/core/spum", .count = 1 } },
      { .kind = ACTION_VERIFY_NOT_EXISTS, .exists = sp_str_lit("build/debug/store/include/spum.h") },
    },
  });
}

sp_test(script, build_dep_profiles) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/script/build_dep_static",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .args = { "-m", "release" } } },
      { .kind = ACTION_VERIFY_DIR_COUNT, .verify_dir_count = { .dir = ".home/storage/cache/store/core/spum", .count = 1 } },
      { .kind = ACTION_RUN_BIN, .bin.name = "build_dep_static" },
    },
  });
}

sp_test(script, add_define) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/script/add_define",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_RUN_BIN, .bin.name = "main" },
    },
  });
}

sp_test(script, add_include) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/script/add_include",
    .copy = { "include/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_RUN_BIN, .bin.name = "main" },
    },
  });
}

sp_test(script, add_system_dep) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/script/add_system_dep",
    .when.msvc_todo = true,
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_RUN_BIN, .bin.name = "main" },
    },
  });
}

sp_test(script, add_test) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/script/add_test",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_RUN_TEST, .bin.name = "test" },
    },
  });
}

sp_test(script, add_exe) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/script/add_exe",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_RUN_BIN, .bin.name = "foo" },
    },
  });
}

sp_test(script, embed) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/script/embed",
    .copy = { "hello.txt" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_RUN_BIN, .bin = { .name = "main", .rc = 0 } },
    },
  });
}

sp_test(script, input_order) {
  return sp_test_skip(t, "pending: canonicalize declared node input/output order in DAG construction");

  return run_rebuild_test(t, (rebuild_test_t) {
    .project = "test/integration/fixtures/script/input_order",
    .copy = { "a.txt", "b.txt", "c.txt", "inputs.txt", "inputs.reordered.txt" },
    .first = {
      .args = { "build" },
      .expect = {
        .events = { { .event = SPN_EVENT_SCRIPT_USER_FN } },
        .exists = { work_file("input_order/gen.h") },
      },
    },
    .rebuilds = {
      {
        .change.moves = {
          { .from = sp_str_lit("inputs.reordered.txt"), .to = sp_str_lit("inputs.txt") },
        },
        .command = {
          .args = { "build" },
          .expect.events = { { .event = SPN_EVENT_SCRIPT_USER_FN, .absent = true } },
        },
      },
    },
    .watches = {
      { .file = work_file("input_order/gen.h"), .mtime = REBUILD_MTIME_UNCHANGED },
    },
  });
}

sp_test(script, generated_source) {
  return sp_test_skip(t, "pending: union declared node outputs into source-glob expansion");

  return run_command_test(t, (command_test_t) {
    .project = "test/integration/fixtures/script/generated_source",
    .args = { "build" },
    .expect = {
      .exists = { exe("generated_source") },
    },
  });
}
