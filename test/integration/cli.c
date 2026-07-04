SPN_TEST_SUITE(cli)

UTEST_F(cli, missing_required_package_version) {
  tmpfs_init_named(&uf->fixture.fs, "cli_missing_required_package_version");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/cli/missing_required_package_version",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .rc = 1 } },
    },
  });
}

UTEST_F(cli, missing_required_package_name) {
  tmpfs_init_named(&uf->fixture.fs, "cli_missing_required_package_name");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/cli/missing_required_package_name",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .rc = 1 } },
    },
  });
}

UTEST_F(cli, wrong_type_required_package_version) {
  tmpfs_init_named(&uf->fixture.fs, "cli_wrong_type_required_package_version");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/cli/wrong_type_required_package_version",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .rc = 1 } },
    },
  });
}
