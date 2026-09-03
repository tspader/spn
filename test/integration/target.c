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
    .when.msvc_todo = true,
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .args = { "main", "test" } } },
      { .kind = ACTION_VERIFY_EXISTS, .exists = static_lib("spum") },
      { .kind = ACTION_VERIFY_EXISTS, .exists = store_file("bin/main") },
      { .kind = ACTION_VERIFY_EXISTS, .exists = test_exe("test") },
    },
  });
}

sp_test(target, same_name) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/target/same_name",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_EXISTS, .exists = static_lib("A") },
      { .kind = ACTION_VERIFY_EXISTS, .exists = store_file("bin/A") },
      { .kind = ACTION_VERIFY_EXISTS, .exists = test_exe("T") },
    },
  });
}

sp_test(target, selection_default) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/target/selection",
    .copy = { "spum.c", "script.c", "x.c" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_EXISTS, .exists = static_lib("spum") },
      { .kind = ACTION_VERIFY_EXISTS, .exists = store_file("bin/main") },
      { .kind = ACTION_VERIFY_EXISTS, .exists = test_exe("test") },
      { .kind = ACTION_VERIFY_EXISTS, .exists = example_exe("x") },
      { .kind = ACTION_VERIFY_NOT_EXISTS, .exists = store_file("bin/script") },
    },
  });
}

sp_test(target, selection_example) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/target/selection",
    .copy = { "spum.c", "script.c", "x.c" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .args = { "--example" } } },
      { .kind = ACTION_VERIFY_EXISTS, .exists = example_exe("x") },
      { .kind = ACTION_VERIFY_NOT_EXISTS, .exists = store_file("bin/main") },
      { .kind = ACTION_VERIFY_NOT_EXISTS, .exists = test_exe("test") },
      { .kind = ACTION_VERIFY_NOT_EXISTS, .exists = store_file("bin/script") },
    },
  });
}

sp_test(target, selection_named_library) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/target/selection_libs",
    .when.msvc_todo = true,
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
      { .kind = ACTION_VERIFY_NOT_EXISTS, .exists = example_exe("x") },
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
    .when.msvc_todo = true,
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

sp_test(target, selection_named_script) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/target/selection",
    .when.msvc_todo = true,
    .copy = { "spum.c", "script.c" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .args = { "script" } } },
      { .kind = ACTION_VERIFY_EXISTS, .exists = store_file("bin/script") },
      { .kind = ACTION_VERIFY_NOT_EXISTS, .exists = store_file("bin/main") },
      { .kind = ACTION_VERIFY_NOT_EXISTS, .exists = test_exe("test") },
    },
  });
}

sp_test(target, example) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/target/example",
    .copy = { "a.c", "a.h", "example" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_EXISTS, .exists = static_lib("A") },
      { .kind = ACTION_VERIFY_EXISTS, .exists = example_exe("E") },
    },
  });
}

sp_test(target, root_include) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/target/root_include",
    .copy = { "a.h", "src" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_EXISTS, .exists = exe("main") },
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
      { .kind = ACTION_VERIFY_INCLUDE, .verify_include.file = sp_str_lit("kit/on.h") },
      { .kind = ACTION_VERIFY_NOT_EXISTS, .exists = store_file("include/kit/off.h") },
      { .kind = ACTION_VERIFY_EXISTS, .exists = exe("publish") },
    },
  });
}

sp_test(target, system_deps) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/target/system_deps",
    .when.msvc_todo = true,
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_VERIFY_EXISTS, .exists = sp_str_lit("spn.lock") },
      { .kind = ACTION_VERIFY_EXISTS, .exists = exe("main") },
      { .kind = ACTION_RUN_CLI, .cli = { "build", .args = { "--force" } } },
      { .kind = ACTION_VERIFY_EXISTS, .exists = exe("main") },
    },
  });
}

sp_test(target, lib_system_deps) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/target/lib_system_deps",
    .copy = { "mathy.c", "direct.c" },
    .when.msvc_todo = true,
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_VERIFY_EXISTS, .exists = exe("main") },
      { .kind = ACTION_VERIFY_EXISTS, .exists = exe("direct") },
    },
  });
}

sp_test(target, link_flags) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/target/link_flags",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .rc = 1 } },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = SPN_EVENT_LINK_FAILED } },
      { .kind = ACTION_VERIFY_NO_CC_ARG, .verify_cc_arg = { "-lmissing", "/DEFAULTLIB:missing" } },
    },
  });
}

sp_test(target, linker_script_msvc_unsupported) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/target/linker_script",
    .copy = { "main.ld" },
    .when.target = SPN_TEST_ARCH "-windows-msvc",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .rc = 1 } },
      { .kind = ACTION_VERIFY_RESULT, .verify_result.err = SPN_ERR_COMPILER_FEATURE_UNSUPPORTED },
    },
  });
}
