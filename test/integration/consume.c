#include "common.h"

SPN_TEST_SUITE(consume)

UTEST_F(consume, static_lib) {
  tmpfs_init_named(&uf->fixture.fs, "consume_static");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/consume/static",
    .copy = { "packages/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .args = { "-p", "debug" } } },
      { .kind = ACTION_VERIFY_EXISTS, .exists = static_lib("spum") },
      { .kind = ACTION_VERIFY_EXISTS, .exists = store_file("bin/main") },
    },
  });
}

UTEST_F(consume, shared_lib) {
  tmpfs_init_named(&uf->fixture.fs, "consume_shared");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/consume/shared",
    .copy = { "packages/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .args = { "-p", "debug" } } },
      { .kind = ACTION_VERIFY_EXISTS, .exists = shared_lib("spum") },
      { .kind = ACTION_VERIFY_EXISTS, .exists = store_file("bin/main") },
    },
  });
}

UTEST_F(consume, shared_lib_static_profile) {
  tmpfs_init_named(&uf->fixture.fs, "consume_shared_static_profile");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/consume/shared",
    .copy = { "packages/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .args = { "-p", "static" }, .rc = 1 } },
      { .kind = ACTION_VERIFY_CLI_CONTAINS, .verify_cli.needle = sp_str_lit("doesn't support") },
      { .kind = ACTION_VERIFY_CLI_CONTAINS, .verify_cli.needle = sp_str_lit("the profile requested it") },
    },
  });
}

UTEST_F(consume, source_lib) {
  tmpfs_init_named(&uf->fixture.fs, "consume_source");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/consume/source",
    .copy = { "packages/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .args = { "-p", "debug" } } },
      { .kind = ACTION_VERIFY_EXISTS, .exists = store_file("bin/main") },
    },
  });
}

UTEST_F(consume, system_dep) {
  tmpfs_init_named(&uf->fixture.fs, "consume_system_dep");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/consume/system_dep",
    .copy = { "packages/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .args = { "-p", "static" } } },
      { .kind = ACTION_VERIFY_EXISTS, .exists = profile_static_lib("static", "spum") },
      { .kind = ACTION_VERIFY_EXISTS, .exists = profile_store_file("static", "bin/main") },
    },
  });
}

UTEST_F(consume, transitive) {
  tmpfs_init_named(&uf->fixture.fs, "consume_transitive");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/consume/transitive",
    .copy = { "packages/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .args = { "-p", "debug" } } },
      { .kind = ACTION_VERIFY_EXISTS, .exists = static_lib("spum") },
      { .kind = ACTION_VERIFY_EXISTS, .exists = static_lib("spam") },
      { .kind = ACTION_VERIFY_EXISTS, .exists = store_file("bin/main") },
    },
  });
}

UTEST_F(consume, explicit_root_with_package_dep) {
  tmpfs_init_named(&uf->fixture.fs, "consume_explicit_root_with_package_dep");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/consume/root_only",
    .copy = { "packages/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .args = { "main" } } },
      { .kind = ACTION_VERIFY_EXISTS, .exists = static_lib("dependency") },
      { .kind = ACTION_VERIFY_EXISTS, .exists = store_file("bin/main") },
      { .kind = ACTION_VERIFY_NOT_EXISTS, .exists = store_file("bin/dependency-bin") },
      { .kind = ACTION_VERIFY_NOT_EXISTS, .exists = store_file("bin/dependency-script") },
      { .kind = ACTION_VERIFY_NOT_EXISTS, .exists = test_exe("dependency-test") },
    },
  });
}

UTEST_F(consume, dependency_package_is_not_a_root_target) {
  tmpfs_init_named(&uf->fixture.fs, "consume_dependency_package_is_not_a_root_target");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/consume/root_only",
    .copy = { "packages/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .args = { "dependency" }, .rc = 1 } },
      { .kind = ACTION_VERIFY_CLI_CONTAINS, .verify_cli.needle = sp_str_lit("is not defined for the selected target kinds") },
      { .kind = ACTION_VERIFY_NOT_EXISTS, .exists = static_lib("dependency") },
    },
  });
}

UTEST_F(consume, multi_kind_default) {
  tmpfs_init_named(&uf->fixture.fs, "consume_multi_kind_default");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/consume/multi_kind/default",
    .copy = { "packages/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_VERIFY_EXISTS, .exists = store_file("bin/main") },
    },
  });
}

UTEST_F(consume, multi_kind_static) {
  tmpfs_init_named(&uf->fixture.fs, "consume_multi_kind_static");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/consume/multi_kind/static",
    .copy = { "packages/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_VERIFY_EXISTS, .exists = static_lib("spum") },
      { .kind = ACTION_VERIFY_EXISTS, .exists = store_file("bin/main") },
    },
  });
}

UTEST_F(consume, multi_kind_shared) {
  tmpfs_init_named(&uf->fixture.fs, "consume_multi_kind_shared");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/consume/multi_kind/shared",
    .copy = { "packages/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_VERIFY_EXISTS, .exists = shared_lib("spum") },
      { .kind = ACTION_VERIFY_EXISTS, .exists = store_file("bin/main") },
    },
  });
}

UTEST_F(consume, multi_kind_source) {
  tmpfs_init_named(&uf->fixture.fs, "consume_multi_kind_source");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/consume/multi_kind/source",
    .copy = { "packages/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_VERIFY_EXISTS, .exists = store_file("bin/main") },
    },
  });
}

UTEST_F(consume, kind_not_supported) {
  tmpfs_init_named(&uf->fixture.fs, "consume_kind_not_supported");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/consume/kind_not_supported",
    .copy = { "packages/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .rc = 1 } },
      { .kind = ACTION_VERIFY_CLI_CONTAINS, .verify_cli.needle = sp_str_lit("doesn't support") },
      { .kind = ACTION_VERIFY_CLI_CONTAINS, .verify_cli.needle = sp_str_lit("the root manifest requested it") },
    },
  });
}
