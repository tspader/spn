#define SP_IMPLEMENTATION
#include "sp.h"

#define SP_TEST_IMPLEMENTATION
#include "test.h"
#include "utest.h"
#include "action.h"

UTEST_MAIN()

#define uf utest_fixture

#define r(str) str "\n"


typedef struct {
  tmpfs_t fs;
  s32* result;
  struct {
    sp_str_t root;
    sp_str_t spn;
    sp_str_t cache;
  } paths;
} fixture_t;

struct spn_build {
  fixture_t fixture;
};

UTEST_F_SETUP(spn_build) {
  uf->fixture.paths.root = sp_fs_get_exe_path();
  sp_for(it, 4) {
    uf->fixture.paths.root = sp_fs_parent_path(uf->fixture.paths.root);
  }

  uf->fixture.paths.spn = sp_fs_join_path(uf->fixture.paths.root, sp_str_lit("bootstrap/bin/spn"));
  ASSERT_TRUE(sp_fs_exists(uf->fixture.paths.spn));

}

UTEST_F_TEARDOWN(spn_build) {
  sp_log(uf->fixture.fs.root);
}

void copy_project_files(s32* utest_result, fixture_t* fixture, const c8* path) {
  if (!path) {
    return;
  }

  sp_str_t project = sp_fs_join_path(fixture->paths.root, sp_str_view(path));
  ASSERT_TRUE(sp_fs_exists(project));

  sp_da(sp_os_dir_ent_t) entries = sp_fs_collect_recursive(project);
  sp_dyn_array_for(entries, it) {
    sp_os_dir_ent_t* entry = &entries[it];
    if (sp_fs_is_dir(entry->file_path)) {
      continue;
    }

    sp_str_t relative = sp_str_strip_left(entry->file_path, project);
    relative = sp_str_strip_left(relative, sp_str_lit("/"));
    tmpfs_create(&fixture->fs, relative, sp_io_read_file(entry->file_path));
  }
}

void run_test(s32* utest_result, fixture_t* fixture, test_t test) {
  fixture->paths.cache = tmpfs_get(&fixture->fs, sp_str_lit("cache"));
  copy_project_files(utest_result, fixture, test.project);

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
        sp_str_t path = tmpfs_get(&fixture->fs, sp_str_view(action.remove.file));
        sp_fs_remove_file(path);
        EXPECT_FALSE(sp_fs_exists(path));
        break;
      }
      case ACTION_MOVE_FILE: {
        sp_str_t from = tmpfs_get(&fixture->fs, action.move_file.from);
        sp_str_t to = tmpfs_get(&fixture->fs, action.move_file.to);
        sp_str_t content = sp_io_read_file(from);

        tmpfs_create(&fixture->fs, action.move_file.to, content);
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
        sp_str_t path = tmpfs_get(&fixture->fs, sp_str_view(action.remove.dir));
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
              {
                .key = sp_str_lit("SPN_STORAGE_DIR"),
                .value = fixture->paths.cache,
              },
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
      { .kind = ACTION_VERIFY_PKG_LOCKED, .verify_locked = { .name = "sp" } },
      { .kind = ACTION_REMOVE_DIR, .remove = { .dir = "build" } },
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_VERIFY_LOCKED },
      { .kind = ACTION_VERIFY_PKG_LOCKED, .verify_locked = { .name = "sp" } },
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
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      {
        .kind = ACTION_SUBPROCESS,
        .process = {
          .config = {
            .command = sp_str_lit("./build/debug/store/bin/editable_package"),
          },
          .rc = 69,
        },
      },
      {
        .kind = ACTION_REMOVE_FILE,
        .remove.file = "packages/spum/spum.h",
      },
      {
        .kind = ACTION_MOVE_FILE,
        .move_file = {
          .from = sp_str_lit("packages/spum/kram.h"),
          .to = sp_str_lit("packages/spum/spum.h"),
        },
      },
      { .kind = ACTION_REMOVE_DIR, .remove.dir = "build" },
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      {
        .kind = ACTION_SUBPROCESS,
        .process = {
          .config = {
            .command = sp_str_lit("./build/debug/store/bin/editable_package"),
          },
          .rc = 42,
        },
      },
      { .kind = ACTION_VERIFY_LOCKED },
      { .kind = ACTION_VERIFY_PKG_LOCKED, .verify_locked = { .name = "spum" } },
    },
  });
}

UTEST_F(spn_build, top_level_system_deps) {
  tmpfs_init_named(&uf->fixture.fs, "top_level_system_deps");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/spn_build/top_level_system_deps",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_VERIFY_LOCKED },
      { .kind = ACTION_VERIFY_PKG_LOCKED, .verify_locked = { .name = "sp" } },
      {
        .kind = ACTION_SUBPROCESS,
        .process = {
          .config = {
            .command = sp_str_lit("./build/debug/store/bin/main"),
          },
          .rc = 0,
        },
      },
      { .kind = ACTION_RUN_CLI, .cli = { "build", .args = { "--force" } } },
      {
        .kind = ACTION_SUBPROCESS,
        .process = {
          .config = {
            .command = sp_str_lit("./build/debug/store/bin/main"),
          },
          .rc = 0,
        },
      },
    },
  });
}
