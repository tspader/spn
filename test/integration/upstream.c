#include "harness.h"

sp_test(upstream, root_lib) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/upstream/root",
    .copy = { "example", "cfg.h" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_EXISTS, .exists = static_lib("A") },
    },
  });
}

sp_test(upstream, root_example) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/upstream/root",
    .copy = { "example", "cfg.h" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_EXISTS, .exists = example_exe("E") },
    },
  });
}

sp_test(upstream, recipe_tree_source) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/upstream/root",
    .copy = { "example", "cfg.h" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_EXISTS, .exists = example_exe("R") },
    },
  });
}

sp_test(upstream, recipe_tree_glob) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/upstream/root",
    .copy = { "example", "cfg.h" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_EXISTS, .exists = example_exe("G") },
    },
  });
}

sp_test(upstream, recipe_tree_headers) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/upstream/root",
    .copy = { "example", "cfg.h" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_INCLUDE, .verify_include.file = sp_str_lit("a.h") },
      { .kind = ACTION_VERIFY_INCLUDE, .verify_include.file = sp_str_lit("cfg.h") },
    },
  });
}

sp_test(upstream, file_dep) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/upstream/consume",
    .copy = { "vendor/A/spn.toml" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_EXISTS, .exists = exe("main") },
    },
  });
}
