#include "harness.h"

sp_test(target, static_lib) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/target/static_lib",
    .copy = { "mylib.c" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_EXISTS, .exists = static_lib("mylib") },
    },
  });
}

sp_test(target, shared_lib) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/target/shared_lib",
    .copy = { "spum.c" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_EXISTS, .exists = shared_lib("spum") },
    },
  });
}

sp_test(target, source_glob) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/target/source_glob",
    .copy = { "src" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_EXISTS, .exists = exe("main") },
    },
  });
}

sp_test(target, shared_source) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/target/shared_source",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_EXISTS, .exists = exe("main") },
    },
  });
}

sp_test(target, multiple_roots) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/target/shared_source",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .args = { "main", "test" } } },
      { .kind = ACTION_VERIFY_EXISTS, .exists = static_lib("spum") },
      { .kind = ACTION_VERIFY_EXISTS, .exists = store_file("bin/main") },
      { .kind = ACTION_VERIFY_EXISTS, .exists = test_exe("test") },
    },
  });
}

sp_test(target, selection_default) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/target/selection",
    .copy = { "spum.c", "script.c" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_EXISTS, .exists = static_lib("spum") },
      { .kind = ACTION_VERIFY_EXISTS, .exists = store_file("bin/main") },
      { .kind = ACTION_VERIFY_EXISTS, .exists = test_exe("test") },
      { .kind = ACTION_VERIFY_NOT_EXISTS, .exists = store_file("bin/script") },
    },
  });
}

sp_test(target, selection_named_library) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/target/selection_libs",
    .copy = { "one.c", "two.c" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .args = { "one" } } },
      { .kind = ACTION_VERIFY_EXISTS, .exists = static_lib("one") },
      { .kind = ACTION_VERIFY_NOT_EXISTS, .exists = static_lib("two") },
    },
  });
}

sp_test(target, selection_multiple_kinds) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/target/selection",
    .copy = { "spum.c", "script.c" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .args = { "--bin", "--test" } } },
      { .kind = ACTION_VERIFY_EXISTS, .exists = exe("main") },
      { .kind = ACTION_VERIFY_EXISTS, .exists = static_lib("spum") },
      { .kind = ACTION_VERIFY_EXISTS, .exists = test_exe("test") },
      { .kind = ACTION_VERIFY_NOT_EXISTS, .exists = store_file("bin/script") },
    },
  });
}

sp_test(target, selection_name_respects_kind) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/target/selection",
    .copy = { "spum.c", "script.c" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .args = { "--lib", "main" }, .rc = 1 } },
      { .kind = ACTION_VERIFY_RESULT, .verify_result = { .err = SPN_ERR_TARGET_SELECTION } },
      { .kind = ACTION_VERIFY_NOT_EXISTS, .exists = store_file("bin/main") },
    },
  });
}

sp_test(target, selection_test_command) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/target/selection",
    .copy = { "spum.c", "script.c" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "test", .args = { "test" } } },
      { .kind = ACTION_VERIFY_EXISTS, .exists = test_exe("test") },
      { .kind = ACTION_VERIFY_EXISTS, .exists = static_lib("spum") },
      { .kind = ACTION_VERIFY_NOT_EXISTS, .exists = store_file("bin/main") },
      { .kind = ACTION_VERIFY_NOT_EXISTS, .exists = store_file("bin/script") },
    },
  });
}

sp_test(target, selection_run_command) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/target/selection",
    .copy = { "spum.c", "script.c" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "run", .args = { "script" } } },
      { .kind = ACTION_VERIFY_EXISTS, .exists = store_file("bin/script") },
      { .kind = ACTION_VERIFY_NOT_EXISTS, .exists = store_file("bin/main") },
      { .kind = ACTION_VERIFY_NOT_EXISTS, .exists = test_exe("test") },
    },
  });
}

sp_test(target, publish) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/target/publish",
    .copy = { "packages/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_INCLUDE, .verify_include.file = sp_str_lit("kit.h") },
      { .kind = ACTION_VERIFY_INCLUDE, .verify_include.file = sp_str_lit("kit/a.h") },
      { .kind = ACTION_VERIFY_INCLUDE, .verify_include.file = sp_str_lit("kit/b.h") },
      { .kind = ACTION_VERIFY_EXISTS, .exists = exe("publish") },
    },
  });
}

sp_test(target, system_deps) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/target/system_deps",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_VERIFY_EXISTS, .exists = sp_str_lit("spn.lock") },
      { .kind = ACTION_VERIFY_EXISTS, .exists = exe("main") },
      { .kind = ACTION_RUN_CLI, .cli = { "build", .args = { "--force" } } },
      { .kind = ACTION_VERIFY_EXISTS, .exists = exe("main") },
    },
  });
}

sp_test(target, source_pin) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/target/source_pin",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_VERIFY_EXISTS, .exists = exe("main") },
    },
  });
}
