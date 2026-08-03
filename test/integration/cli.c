#include "harness.h"

sp_test(cli, missing_required_package_version) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/cli/missing_required_package_version",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .rc = 1 } },
    },
  });
}

sp_test(cli, missing_required_package_name) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/cli/missing_required_package_name",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .rc = 1 } },
    },
  });
}

sp_test(cli, wrong_type_required_package_version) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/cli/wrong_type_required_package_version",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .rc = 1 } },
    },
  });
}

sp_test(cli, init) {
  return run_test(t, (test_t) {
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "init" } },
      { .kind = ACTION_VERIFY_EXISTS, .exists = sp_str_lit("spn.toml") },
      { .kind = ACTION_VERIFY_EXISTS, .exists = sp_str_lit("main.c") },
      { .kind = ACTION_VERIFY_EXISTS, .exists = sp_str_lit(".gitignore") },
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "init", .rc = 1 } },
      { .kind = ACTION_VERIFY_RESULT, .verify_result = { .err = SPN_ERR_INIT_EXISTS } },
    },
  });
}

sp_test(cli, init_existing) {
  return run_test(t, (test_t) {
    .actions = {
      { .kind = ACTION_CREATE_FILE, .create = { .file = sp_str_lit(".gitignore"), .content = sp_str_lit("A") } },
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "init", .rc = 1 } },
      { .kind = ACTION_VERIFY_RESULT, .verify_result = { .err = SPN_ERR_INIT_EXISTS } },
    },
  });
}

sp_test(cli, init_path) {
  return run_test(t, (test_t) {
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "init", .args = { "sub" } } },
      { .kind = ACTION_VERIFY_EXISTS, .exists = sp_str_lit("sub/main.c") },
    },
  });
}

sp_test(cli, init_bare) {
  return run_test(t, (test_t) {
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "init", .args = { "sub", "--bare" } } },
      { .kind = ACTION_VERIFY_EXISTS, .exists = sp_str_lit("sub/spn.toml") },
      { .kind = ACTION_VERIFY_NOT_EXISTS, .exists = sp_str_lit("sub/main.c") },
      { .kind = ACTION_VERIFY_NOT_EXISTS, .exists = sp_str_lit("sub/.gitignore") },
    },
  });
}

sp_test(cli, add) {
  return run_test(t, (test_t) {
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

sp_test(cli, add_test_dep) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/cli/add",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "add", .args = { "spum", "--test" } } },
      { .kind = ACTION_VERIFY_FILE_CONTAINS, .verify_file_contains = { .file = sp_str_lit("spn.toml"), .needle = sp_str_lit("[deps.test]") } },
    },
  });
}

sp_test(cli, add_unknown_package) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/cli/add",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "add", .args = { "kram" }, .rc = 1 } },
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "add", .args = { "spum@9.0.0" }, .rc = 1 } },
    },
  });
}

sp_test(cli, clean) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/cli/add",
    .actions = {
      { .kind = ACTION_CREATE_FILE, .create = { .file = store_file("bin/main"), .content = sp_str_lit("x") } },
      { .kind = ACTION_CREATE_FILE, .create = { .file = profile_store_file("release", "bin/main"), .content = sp_str_lit("x") } },
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "clean", .args = { "-p", "debug" } } },
      { .kind = ACTION_VERIFY_NOT_EXISTS, .exists = sp_str_lit("build/debug") },
      { .kind = ACTION_VERIFY_EXISTS, .exists = sp_str_lit("build/release") },
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "clean" } },
      { .kind = ACTION_VERIFY_NOT_EXISTS, .exists = sp_str_lit("build") },
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "clean" } },
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "clean", .args = { "-p", "../escape" }, .rc = 1 } },
    },
  });
}

sp_test(cli, index_path) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/deps/index/basic",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "index", .args = { "path" } } },
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "index", .args = { "path", "nope" }, .rc = 1 } },
    },
  });
}

sp_test(cli, index_sync) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/deps/index/basic",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "index", .args = { "sync" } } },
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "index", .args = { "sync", "nope" }, .rc = 1 } },
    },
  });
}

sp_test(cli, publish_dry_run) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/deps/index/basic",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "publish", .args = { "--dry", "--source-url", "https://example.com/x.git", "--source-rev", "abc123" } } },
      { .kind = ACTION_VERIFY_NOT_EXISTS, .exists = sp_str_lit(".home/storage/spn/packages/core/index_package.jsonl") },
    },
  });
}

sp_test(cli, workspace_index) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/cli/workspace_index",
    .copy = { "index/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build" } },
      { .kind = ACTION_RUN_BIN, .bin = { .name = "main", .rc = 3 } },
    },
  });
}

sp_test(cli, workspace_dir_index) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/cli/workspace_dir_index",
    .copy = { "index/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build" } },
      { .kind = ACTION_VERIFY_EXISTS, .exists = exe("main") },
    },
  });
}

sp_test(cli, user_dir_index) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/cli/user_dir_index",
    .actions = {
      {
        .kind = ACTION_CREATE_FILE,
        .create = {
          .file = sp_str_lit(".home/config/spn/spn.toml"),
          .content = sp_str_lit("[[index]]\nname = \"local\"\npath = \"index\"\n"),
        },
      },
      {
        .kind = ACTION_CREATE_FILE,
        .create = {
          .file = sp_str_lit(".home/config/spn/index/A/spn.toml"),
          .content = sp_str_lit("[package]\nname = \"A\"\nversion = \"1.0.0\"\n\n[[lib]]\nname = \"A\"\nkinds = [\"source\"]\nheaders = [\"A.h\"]\n"),
        },
      },
      {
        .kind = ACTION_CREATE_FILE,
        .create = {
          .file = sp_str_lit(".home/config/spn/index/A/A.h"),
          .content = sp_str_lit("#pragma once\n\nstatic inline int A(void) {\n  return 0;\n}\n"),
        },
      },
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build" } },
      { .kind = ACTION_VERIFY_EXISTS, .exists = exe("main") },
    },
  });
}
