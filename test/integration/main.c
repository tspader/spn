#define SP_IMPLEMENTATION
#include "sp.h"

#define SP_TEST_IMPLEMENTATION
#include "test.h"
#include "utest.h"
#include "action.h"

#define TOML_IMPLEMENTATION
#include "toml.h"

#define SPN_TOML_IMPLEMENTATION
#include "stoml.h"

UTEST_MAIN()

#define uf utest_fixture

#define r(str) str "\n"


typedef struct {
  tmpfs_t fs;
  s32* result;
  struct {
    sp_str_t root;
    sp_str_t spn;
    sp_str_t storage;
    sp_str_t config;
    sp_str_t index;
  } paths;
} fixture_t;

struct spn_build {
  fixture_t fixture;
};

static bool tmpfs_top_level_initialized = false;

void init_tmpfs_top_level(void) {
  if (tmpfs_top_level_initialized) {
    return;
  }

  sp_str_t tmp = sp_os_get_env_as_path(sp_str_lit("SPN_TEST_TMP"));
  if (sp_str_empty(tmp)) {
    tmp = sp_str_lit(".tmp");
  }

  sp_tm_epoch_t now = sp_tm_now_epoch();
  sp_str_t iso = sp_tm_epoch_to_iso8601(now);
  c8* sanitized = (c8*)sp_alloc(iso.len);
  sp_for(it, iso.len) {
    sanitized[it] = iso.data[it] == ':' ? '-' : iso.data[it];
  }

  tmpfs_set_top_level(sp_fs_join_path(tmp, sp_str(sanitized, iso.len)));
  tmpfs_top_level_initialized = true;
}

UTEST_F_SETUP(spn_build) {
  init_tmpfs_top_level();

  uf->fixture.paths.root = sp_fs_get_exe_path();
  sp_for(it, 4) {
    uf->fixture.paths.root = sp_fs_parent_path(uf->fixture.paths.root);
  }

  uf->fixture.paths.spn = sp_fs_join_path(uf->fixture.paths.root, sp_str_lit("bootstrap/bin/spn"));
  ASSERT_TRUE(sp_fs_exists(uf->fixture.paths.spn));

}

UTEST_F_TEARDOWN(spn_build) {
}

void fixture_write_file(sp_str_t path, sp_str_t content) {
  sp_str_t parent = sp_fs_parent_path(path);
  if (!sp_str_empty(parent)) {
    sp_fs_create_dir(parent);
  }

  sp_io_writer_t io = sp_io_writer_from_file(path, SP_IO_WRITE_MODE_OVERWRITE);
  sp_io_write_str(&io, content);
  sp_io_writer_close(&io);
}

sp_str_t fixture_registry_manifest_from_source(toml_table_t* source, sp_str_t repo_url) {
  toml_table_t* package = toml_table_table(source, "package");
  SP_ASSERT(package);

  spn_toml_writer_t writer = spn_toml_writer_new();
  spn_toml_begin_table_cstr(&writer, "package");
  spn_toml_append_str_cstr(&writer, "name", spn_toml_str(package, "name"));
  spn_toml_append_str_cstr(&writer, "version", spn_toml_str(package, "version"));
  spn_toml_append_str_cstr(&writer, "url", repo_url);

  sp_str_t author = spn_toml_str_opt(package, "author", "");
  if (!sp_str_empty(author)) {
    spn_toml_append_str_cstr(&writer, "author", author);
  }

  sp_str_t maintainer = spn_toml_str_opt(package, "maintainer", "");
  if (!sp_str_empty(maintainer)) {
    spn_toml_append_str_cstr(&writer, "maintainer", maintainer);
  }

  sp_str_t commit = spn_toml_str_opt(package, "commit", "");
  if (!sp_str_empty(commit)) {
    spn_toml_append_str_cstr(&writer, "commit", commit);
  }

  toml_array_t* include = toml_table_array(package, "include");
  if (spn_toml_array_len(include)) {
    spn_toml_append_str_array_cstr(&writer, "include", spn_toml_arr_to_str_arr(include));
  }

  toml_array_t* define = toml_table_array(package, "define");
  if (spn_toml_array_len(define)) {
    spn_toml_append_str_array_cstr(&writer, "define", spn_toml_arr_to_str_arr(define));
  }

  toml_array_t* system_deps = toml_table_array(package, "system_deps");
  if (spn_toml_array_len(system_deps)) {
    spn_toml_append_str_array_cstr(&writer, "system_deps", spn_toml_arr_to_str_arr(system_deps));
  }
  spn_toml_end_table(&writer);

  toml_table_t* lib = toml_table_table(source, "lib");
  if (lib) {
    toml_array_t* kinds = toml_table_array(lib, "kinds");
    if (spn_toml_array_len(kinds)) {
      spn_toml_begin_table_cstr(&writer, "lib");
      spn_toml_append_str_array_cstr(&writer, "kinds", spn_toml_arr_to_str_arr(kinds));
      spn_toml_end_table(&writer);
    }
  }

  return spn_toml_writer_write(&writer);
}

void copy_project_path(s32* utest_result, fixture_t* fixture, sp_str_t project, sp_str_t relative) {
  sp_str_t from = sp_fs_join_path(project, relative);
  if (sp_fs_is_glob(from)) {
    EXPECT_TRUE(sp_fs_exists(sp_fs_parent_path(from)));
  }
  else {
    EXPECT_TRUE(sp_fs_exists(from));
  }

  sp_str_t to = fixture->fs.root;
  sp_str_t relative_parent = sp_fs_parent_path(relative);
  if (!sp_str_empty(relative_parent)) {
    to = tmpfs_get(&fixture->fs, relative_parent);
    sp_fs_create_dir(to);
  }

  sp_fs_copy(from, to);
}

void setup_fixture_index_from_remote(s32* utest_result, fixture_t* fixture, sp_str_t project) {
  sp_str_t remote = sp_fs_join_path(project, sp_str_lit("remote"));
  if (!sp_fs_exists(remote)) {
    return;
  }

  EXPECT_TRUE(sp_fs_is_dir(remote));

  sp_da(sp_os_dir_ent_t) entries = sp_fs_collect(remote);
  sp_da_for(entries, it) {
    sp_os_dir_ent_t* entry = &entries[it];
    if (!sp_fs_is_dir(entry->file_path)) {
      continue;
    }

    sp_str_t repo = tmpfs_get(&fixture->fs, sp_fs_join_path(sp_str_lit("remote"), entry->file_name));
    git_repo_create_from_dir(entry->file_path, repo);

    sp_str_t source_manifest = sp_fs_join_path(entry->file_path, sp_str_lit("spn.toml"));
    EXPECT_TRUE(sp_fs_exists(source_manifest));

    sp_str_t index_pkg = sp_fs_join_path(fixture->paths.index, entry->file_name);
    sp_fs_create_dir(index_pkg);

    toml_table_t* manifest_table = spn_toml_parse(source_manifest);
    EXPECT_TRUE(manifest_table != SP_NULLPTR);

    toml_table_t* package = toml_table_table(manifest_table, "package");
    EXPECT_TRUE(package != SP_NULLPTR);
    sp_str_t version = spn_toml_str(package, "version");

    sp_str_t manifest_with_url = fixture_registry_manifest_from_source(manifest_table, repo);
    EXPECT_FALSE(sp_str_empty(version));

    fixture_write_file(sp_fs_join_path(index_pkg, sp_str_lit("spn.toml")), manifest_with_url);

    sp_str_t source_script = sp_fs_join_path(entry->file_path, sp_str_lit("spn.c"));
    if (sp_fs_exists(source_script)) {
      sp_fs_copy(source_script, index_pkg);
    }

    spn_toml_writer_t metadata = spn_toml_writer_new();
    spn_toml_begin_array_cstr(&metadata, "versions");
    spn_toml_append_array_table(&metadata);
    spn_toml_append_str_cstr(&metadata, "version", version);
    spn_toml_append_str_cstr(&metadata, "commit", sp_str_lit("HEAD"));
    spn_toml_end_array(&metadata);

    fixture_write_file(sp_fs_join_path(index_pkg, sp_str_lit("metadata.toml")), spn_toml_writer_write(&metadata));
  }
}

void copy_project_files(s32* utest_result, fixture_t* fixture, test_t test) {
  if (!test.project) {
    return;
  }

  sp_str_t project = sp_fs_join_path(fixture->paths.root, sp_str_view(test.project));
  ASSERT_TRUE(sp_fs_exists(project));

  const c8* auto_copy [] = {
    "main.c",
    "spn.c",
    "spn.toml",
  };

  sp_carr_for(auto_copy, it) {
    sp_str_t from = sp_fs_join_path(project, sp_str_view(auto_copy[it]));
    if (sp_fs_exists(from)) {
      sp_fs_copy(from, fixture->fs.root);
    }
  }

  sp_carr_for(test.copy, it) {
    const c8* path = test.copy[it];
    if (!path) {
      break;
    }

    copy_project_path(utest_result, fixture, project, sp_str_view(path));
  }

  setup_fixture_index_from_remote(utest_result, fixture, project);
}

void run_test(s32* utest_result, fixture_t* fixture, test_t test) {
  fixture->paths.storage = tmpfs_get(&fixture->fs, sp_str_lit(".home/storage"));
  sp_fs_create_dir(fixture->paths.storage);
  sp_str_t repo = sp_fs_join_path(fixture->paths.storage, sp_str_lit("spn"));
  sp_str_t include = sp_fs_join_path(repo, sp_str_lit("include"));
  fixture->paths.index = sp_fs_join_path(repo, sp_str_lit("packages"));
  sp_fs_create_dir(include);
  sp_fs_create_dir(fixture->paths.index);
  sp_fs_copy(sp_fs_join_path(fixture->paths.root, sp_str_lit("include/spn.h")), include);


  fixture->paths.config = tmpfs_get(&fixture->fs, sp_str_lit(".home/config"));
  sp_fs_create_dir(fixture->paths.config);
  copy_project_files(utest_result, fixture, test);

  sp_for(it, SPN_TEST_MAX_ACTIONS) {
    action_t action = test.actions[it];
    if (action.kind == ACTION_NONE) {
      break;
    }

    switch (action.kind) {
      case ACTION_NONE: {
        break;
      }
      case ACTION_CREATE_FILE: {
        tmpfs_create(&fixture->fs, action.create.file, action.create.content);
        break;
      }
      case ACTION_REMOVE_FILE: {
        sp_str_t path = tmpfs_get(&fixture->fs, sp_str_view(action.rm.file));
        sp_fs_remove_file(path);
        EXPECT_FALSE(sp_fs_exists(path));
        break;
      }
      case ACTION_MOVE_FILE: {
        sp_str_t from = tmpfs_get(&fixture->fs, action.mv.from);
        sp_str_t to = tmpfs_get(&fixture->fs, action.mv.to);
        sp_str_t content = sp_io_read_file(from);

        tmpfs_create(&fixture->fs, action.mv.to, content);
        sp_fs_remove_file(from);

        EXPECT_FALSE(sp_fs_exists(from));
        EXPECT_TRUE(sp_fs_exists(to));
        break;
      }
      case ACTION_SUBPROCESS: {
        sp_ps_config_t config = action.process.config;
        if (sp_str_empty(config.cwd)) {
          config.cwd = fixture->fs.root;
        }

        sp_ps_output_t output = sp_ps_run(config);
        EXPECT_EQ(action.process.rc, output.status.exit_code);
        break;
      }
      case ACTION_RUN_BIN: {
        sp_str_t bin = tmpfs_get(&fixture->fs, sp_format(
          "build/debug/store/bin/{}",
          SP_FMT_CSTR(action.bin.name)
        ));
        EXPECT_TRUE(sp_fs_exists(bin));

        sp_ps_output_t output = sp_ps_run((sp_ps_config_t) {
          .command = bin,
          .cwd = fixture->fs.root,
        });

        EXPECT_EQ(action.bin.rc, output.status.exit_code);
        break;
      }
      case ACTION_VERIFY_EXISTS: {
        sp_str_t path = tmpfs_get(&fixture->fs, action.verify_exists.file);
        EXPECT_TRUE(sp_fs_exists(path));
        break;
      }
      case ACTION_VERIFY_CONTENT: {
        sp_str_t path = tmpfs_get(&fixture->fs, action.verify_content.file);
        sp_str_t content = sp_io_read_file(path);
        EXPECT_TRUE(sp_str_equal(content, action.verify_content.content));
        break;
      }
      case ACTION_REMOVE_DIR: {
        sp_str_t path = tmpfs_get(&fixture->fs, sp_str_view(action.rm.dir));
        sp_fs_remove_dir(path);
        EXPECT_FALSE(sp_fs_exists(path));
        break;
      }
      case ACTION_RUN_CLI: {
        sp_ps_config_t config = {
          .command = fixture->paths.spn,
          .cwd = fixture->fs.root,
          .env = {
            .extra = {
              { sp_str_lit("SPN_STORAGE_DIR"), fixture->paths.storage },
              { sp_str_lit("SPN_CONFIG_DIR"), fixture->paths.config },
            },
          },
        };

        if (action.cli.cmd) {
          sp_ps_config_add_arg(&config, sp_str_view(action.cli.cmd));
        }

        sp_carr_for(action.cli.args, arg_it) {
          const c8* arg = action.cli.args[arg_it];
          if (!arg) {
            break;
          }

          sp_ps_config_add_arg(&config, sp_str_view(arg));
        }

        sp_ps_output_t output = sp_ps_run(config);
        EXPECT_EQ(0, output.status.exit_code);
        break;
      }
      case ACTION_VERIFY_LOCKED: {
        sp_str_t path = tmpfs_get(&fixture->fs, sp_str_lit("spn.lock"));
        EXPECT_TRUE(sp_fs_exists(path));

        sp_str_t lock = sp_io_read_file(path);
        EXPECT_TRUE(sp_str_contains(lock, sp_str_lit("[[dep]]")));
        break;
      }
      case ACTION_VERIFY_PKG_LOCKED: {
        sp_str_t path = tmpfs_get(&fixture->fs, sp_str_lit("spn.lock"));
        EXPECT_TRUE(sp_fs_exists(path));

        sp_str_t lock = sp_io_read_file(path);
        sp_str_t needle = sp_format("name = \"{}\"", SP_FMT_CSTR(action.verify_locked.name));
        EXPECT_TRUE(sp_str_contains(lock, needle));
        break;
      }
    }
  }
}

UTEST_F(spn_build, tmpfs) {
  tmpfs_init_named(&uf->fixture.fs, "tmpfs");

  tmpfs_create(&uf->fixture.fs, sp_str_lit("foo/bar.txt"), sp_str_lit("hello"));

  sp_str_t path = tmpfs_get(&uf->fixture.fs, sp_str_lit("foo/bar.txt"));
  EXPECT_TRUE(sp_fs_exists(path));
  EXPECT_TRUE(sp_str_starts_with(path, uf->fixture.fs.root));

  sp_str_t content = sp_io_read_file(path);
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
      { .kind = ACTION_VERIFY_PKG_LOCKED, .verify_locked = { .name = "spum" } },
      { .kind = ACTION_REMOVE_DIR, .rm = { .dir = "build" } },
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_VERIFY_LOCKED },
      { .kind = ACTION_VERIFY_PKG_LOCKED, .verify_locked = { .name = "spum" } },
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
      { .kind = ACTION_VERIFY_PKG_LOCKED, .verify_locked = { .name = "spum" } },
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
      { .kind = ACTION_VERIFY_PKG_LOCKED, .verify_locked.name = "spum" },
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
