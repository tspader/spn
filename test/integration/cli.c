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

UTEST_F(cli, init) {
  tmpfs_init_named(&uf->fixture.fs, "cli_init");

  run_test(utest_result, &uf->fixture, (test_t) {
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "init" } },
      { .kind = ACTION_VERIFY_FILE_CONTAINS, .verify_file_contains = { .file = sp_str_lit("spn.toml"), .needle = sp_str_lit("name = \"cli_init\"") } },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = sp_str_lit("main.c") },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = sp_str_lit(".gitignore") },
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "init", .rc = 1 } },
    },
  });
}

UTEST_F(cli, init_path) {
  tmpfs_init_named(&uf->fixture.fs, "cli_init_path");

  run_test(utest_result, &uf->fixture, (test_t) {
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "init", .args = { "sub" } } },
      { .kind = ACTION_VERIFY_FILE_CONTAINS, .verify_file_contains = { .file = sp_str_lit("sub/spn.toml"), .needle = sp_str_lit("name = \"sub\"") } },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = sp_str_lit("sub/main.c") },
    },
  });
}

UTEST_F(cli, init_bare) {
  tmpfs_init_named(&uf->fixture.fs, "cli_init_bare");

  run_test(utest_result, &uf->fixture, (test_t) {
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "init", .args = { "sub", "--bare" } } },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = sp_str_lit("sub/spn.toml") },
      { .kind = ACTION_VERIFY_NOT_EXISTS, .verify_not_exists.file = sp_str_lit("sub/main.c") },
      { .kind = ACTION_VERIFY_NOT_EXISTS, .verify_not_exists.file = sp_str_lit("sub/.gitignore") },
    },
  });
}

UTEST_F(cli, add) {
  tmpfs_init_named(&uf->fixture.fs, "cli_add");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/cli/add",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "add", .args = { "spum" } } },
      { .kind = ACTION_VERIFY_FILE_CONTAINS, .verify_file_contains = { .file = sp_str_lit("spn.toml"), .needle = sp_str_lit("spum = \"1.1.0\"") } },
      { .kind = ACTION_VERIFY_FILE_CONTAINS, .verify_file_contains = { .file = sp_str_lit("spn.toml"), .needle = sp_str_lit("# keep") } },
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "add", .args = { "spum@1.0.0" } } },
      { .kind = ACTION_VERIFY_FILE_CONTAINS, .verify_file_contains = { .file = sp_str_lit("spn.toml"), .needle = sp_str_lit("spum = \"1.0.0\"") } },
      { .kind = ACTION_VERIFY_FILE_NOT_CONTAINS, .verify_file_not_contains = { .file = sp_str_lit("spn.toml"), .needle = sp_str_lit("1.1.0") } },
    },
  });
}

UTEST_F(cli, add_test_dep) {
  tmpfs_init_named(&uf->fixture.fs, "cli_add_test_dep");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/cli/add",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "add", .args = { "spum", "--test" } } },
      { .kind = ACTION_VERIFY_FILE_CONTAINS, .verify_file_contains = { .file = sp_str_lit("spn.toml"), .needle = sp_str_lit("[deps.test]") } },
    },
  });
}

UTEST_F(cli, add_unknown_package) {
  tmpfs_init_named(&uf->fixture.fs, "cli_add_unknown_package");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/cli/add",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "add", .args = { "kram" }, .rc = 1 } },
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "add", .args = { "spum@9.0.0" }, .rc = 1 } },
    },
  });
}

UTEST_F(cli, update) {
  tmpfs_init_named(&uf->fixture.fs, "cli_update");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/cli/update",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "update" } },
      { .kind = ACTION_VERIFY_PKG_LOCKED, .verify_locked = { .name = "core/spum" } },
      { .kind = ACTION_VERIFY_FILE_CONTAINS, .verify_file_contains = { .file = sp_str_lit("spn.lock"), .needle = sp_str_lit("version = \"1.1.0\"") } },
      { .kind = ACTION_VERIFY_FILE_NOT_CONTAINS, .verify_file_not_contains = { .file = sp_str_lit("spn.lock"), .needle = sp_str_lit("version = \"2.0.0\"") } },
      { .kind = ACTION_VERIFY_CLI_CONTAINS, .verify_cli = { .needle = sp_str_lit("semver incompatible") } },
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "update" } },
      { .kind = ACTION_VERIFY_CLI_CONTAINS, .verify_cli = { .needle = sp_str_lit("up to date") } },
    },
  });
}

UTEST_F(cli, clean) {
  tmpfs_init_named(&uf->fixture.fs, "cli_clean");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/cli/add",
    .actions = {
      { .kind = ACTION_CREATE_FILE, .create = { .file = store_file("bin/main"), .content = sp_str_lit("x") } },
      { .kind = ACTION_CREATE_FILE, .create = { .file = profile_store_file("release", "bin/main"), .content = sp_str_lit("x") } },
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "clean", .args = { "-p", "debug" } } },
      { .kind = ACTION_VERIFY_NOT_EXISTS, .verify_not_exists.file = sp_str_lit("build/debug") },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = sp_str_lit("build/release") },
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "clean" } },
      { .kind = ACTION_VERIFY_NOT_EXISTS, .verify_not_exists.file = sp_str_lit("build") },
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "clean" } },
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "clean", .args = { "-p", "../escape" }, .rc = 1 } },
    },
  });
}

UTEST_F(cli, index_list) {
  tmpfs_init_named(&uf->fixture.fs, "cli_index_list");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/deps/index/basic",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "index", .args = { "list" } } },
      { .kind = ACTION_VERIFY_CLI_CONTAINS, .verify_cli = { .needle = sp_str_lit("core") } },
      { .kind = ACTION_VERIFY_CLI_CONTAINS, .verify_cli = { .needle = sp_str_lit("filesystem") } },
    },
  });
}

UTEST_F(cli, index_path) {
  tmpfs_init_named(&uf->fixture.fs, "cli_index_path");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/deps/index/basic",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "index", .args = { "path" } } },
      { .kind = ACTION_VERIFY_CLI_CONTAINS, .verify_cli = { .needle = sp_str_lit("spn/packages") } },
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "index", .args = { "path", "nope" }, .rc = 1 } },
      { .kind = ACTION_VERIFY_CLI_CONTAINS, .verify_cli = { .needle = sp_str_lit("unknown index") } },
    },
  });
}

UTEST_F(cli, index_sync) {
  tmpfs_init_named(&uf->fixture.fs, "cli_index_sync");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/deps/index/basic",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "index", .args = { "sync" } } },
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "index", .args = { "sync", "nope" }, .rc = 1 } },
    },
  });
}

UTEST_F(cli, publish_dry_run) {
  tmpfs_init_named(&uf->fixture.fs, "cli_publish_dry_run");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/deps/index/basic",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "publish", .args = { "--dry", "--source-url", "https://example.com/x.git", "--source-rev", "abc123" } } },
      { .kind = ACTION_VERIFY_CLI_CONTAINS, .verify_cli = { .needle = sp_str_lit("\"name\":\"index_package\"") } },
      { .kind = ACTION_VERIFY_CLI_CONTAINS, .verify_cli = { .needle = sp_str_lit("dry run") } },
      { .kind = ACTION_VERIFY_NOT_EXISTS, .verify_not_exists.file = sp_str_lit(".home/storage/spn/packages/core/index_package.jsonl") },
    },
  });
}

UTEST_F(cli, complete) {
  tmpfs_init_named(&uf->fixture.fs, "cli_complete");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/cli/complete",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "__complete", .args = { "--", "spn", "bu" } } },
      { .kind = ACTION_VERIFY_CLI_CONTAINS, .verify_cli = { .needle = sp_str_lit("build") } },
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "__complete", .args = { "--", "spn", "build", "ma" } } },
      { .kind = ACTION_VERIFY_CLI_CONTAINS, .verify_cli = { .needle = sp_str_lit("main") } },
      { .kind = ACTION_VERIFY_CLI_NOT_CONTAINS, .verify_cli = { .needle = sp_str_lit("A") } },
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "__complete", .args = { "--", "spn", "build", "-p", "de" } } },
      { .kind = ACTION_VERIFY_CLI_CONTAINS, .verify_cli = { .needle = sp_str_lit("default") } },
      { .kind = ACTION_VERIFY_CLI_CONTAINS, .verify_cli = { .needle = sp_str_lit("debug") } },
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "__complete", .args = { "--", "spn", "test", "A" } } },
      { .kind = ACTION_VERIFY_CLI_CONTAINS, .verify_cli = { .needle = sp_str_lit("A") } },
      { .kind = ACTION_VERIFY_CLI_NOT_CONTAINS, .verify_cli = { .needle = sp_str_lit("main") } },
    },
  });
}

UTEST_F(cli, complete_no_manifest) {
  tmpfs_init_named(&uf->fixture.fs, "cli_complete_no_manifest");

  run_test(utest_result, &uf->fixture, (test_t) {
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "__complete", .args = { "--", "spn", "build", "ma" } } },
      { .kind = ACTION_VERIFY_CLI_NOT_CONTAINS, .verify_cli = { .needle = sp_str_lit("main") } },
      { .kind = ACTION_VERIFY_CLI_NOT_CONTAINS, .verify_cli = { .needle = sp_str_lit("error") } },
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "__complete", .args = { "--", "spn", "bu" } } },
      { .kind = ACTION_VERIFY_CLI_CONTAINS, .verify_cli = { .needle = sp_str_lit("build") } },
    },
  });
}

UTEST_F(cli, complete_broken_manifest) {
  tmpfs_init_named(&uf->fixture.fs, "cli_complete_broken_manifest");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/cli/complete_broken",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "__complete", .args = { "--", "spn", "build", "ma" } } },
      { .kind = ACTION_VERIFY_CLI_NOT_CONTAINS, .verify_cli = { .needle = sp_str_lit("main") } },
      { .kind = ACTION_VERIFY_CLI_NOT_CONTAINS, .verify_cli = { .needle = sp_str_lit("error") } },
    },
  });
}

UTEST_F(cli, completions) {
  tmpfs_init_named(&uf->fixture.fs, "cli_completions");

  run_test(utest_result, &uf->fixture, (test_t) {
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "completions", .args = { "bash" } } },
      { .kind = ACTION_VERIFY_CLI_CONTAINS, .verify_cli = { .needle = sp_str_lit("complete -o default -F _spn spn") } },
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "completions", .args = { "zsh" } } },
      { .kind = ACTION_VERIFY_CLI_CONTAINS, .verify_cli = { .needle = sp_str_lit("compdef _spn spn") } },
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "completions", .args = { "fish" } } },
      { .kind = ACTION_VERIFY_CLI_CONTAINS, .verify_cli = { .needle = sp_str_lit("complete -c spn") } },
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "completions", .args = { "powershell" } } },
      { .kind = ACTION_VERIFY_CLI_CONTAINS, .verify_cli = { .needle = sp_str_lit("Register-ArgumentCompleter") } },
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "completions", .args = { "elvish" }, .rc = 1 } },
    },
  });
}

UTEST_F(cli, workspace_index) {
  tmpfs_init_named(&uf->fixture.fs, "cli_workspace_index");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/cli/workspace_index",
    .copy = { "index/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "index", .args = { "list" } } },
      { .kind = ACTION_VERIFY_CLI_CONTAINS, .verify_cli = { .needle = sp_str_lit("local") } },
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build" } },
      { .kind = ACTION_RUN_BIN, .bin = { .name = "main", .rc = 3 } },
    },
  });
}
