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
} fixture_t;

struct spn_build {
  fixture_t fixture;
};

UTEST_F_SETUP(spn_build) {
  tmpfs_init(&uf->fixture.fs);
}

UTEST_F_TEARDOWN(spn_build) {
  tmpfs_deinit(&uf->fixture.fs);
}

void copy_project_file_if_exists(fixture_t* fixture, sp_str_t project_root, const c8* file_name) {
  sp_str_t relative = sp_str_view(file_name);
  sp_str_t source = sp_fs_join_path(project_root, relative);
  if (!sp_fs_exists(source)) {
    return;
  }

  tmpfs_create(&fixture->fs, relative, sp_io_read_file(source));
}

void copy_project_files(s32* utest_result, fixture_t* fixture, const c8* path) {
  if (!path) return;

  sp_str_t exe = sp_fs_get_exe_path();
  sp_for(it, 4) {
    exe = sp_fs_parent_path(exe);
  }

  sp_str_t project = sp_fs_join_path(exe, sp_str_view(path));
  ASSERT_TRUE(sp_fs_exists(project));

  copy_project_file_if_exists(fixture, project, "main.c");
  copy_project_file_if_exists(fixture, project, "spn.c");
  copy_project_file_if_exists(fixture, project, "spn.toml");
}

void run_test(s32* utest_result, fixture_t* fixture, test_t test) {
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
        sp_str_t path = tmpfs_get(&fixture->fs, sp_str_view(action.remove_dir.path));
        sp_fs_remove_dir(path);
        EXPECT_FALSE(sp_fs_exists(path));
        break;
      }
      case ACTION_RUN_CLI: {
        sp_ps_config_t config = {
          .command = sp_str_lit("tspn"),
          .cwd = fixture->fs.root,
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

UTEST_F(spn_build, smoke) {
  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/manual/spn_build/smoke",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_VERIFY_LOCKED },
      { .kind = ACTION_VERIFY_PKG_LOCKED, .verify_locked = { .name = "sp" } },
      { .kind = ACTION_REMOVE_DIR, .remove_dir = { .path = "build" } },
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_VERIFY_LOCKED },
      { .kind = ACTION_VERIFY_PKG_LOCKED, .verify_locked = { .name = "sp" } },
    },
  });
}
