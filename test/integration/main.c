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
  struct {
    sp_str_t root;
    sp_str_t spn;
    sp_str_t storage;
    sp_str_t config;
    sp_str_t index;
    sp_str_t include;
  } paths;
} fixture_t;

struct spn_build {
  fixture_t fixture;
};

UTEST_INITIALIZER(spn_build_init_tmpfs_top_level) {
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
}

UTEST_F_SETUP(spn_build) {
  //  $repo/build/$profile/store/bin/$test
  //  │     │     │        │     │
  //  4     3     2        1     0
  uf->fixture.paths.root = sp_fs_get_exe_path();
  sp_for(it, 4) {
    uf->fixture.paths.root = sp_fs_parent_path(uf->fixture.paths.root);
  }

  uf->fixture.paths.spn = sp_fs_join_path(uf->fixture.paths.root, sp_str_lit("bootstrap/bin/spn"));
  ASSERT_TRUE(sp_fs_exists(uf->fixture.paths.spn));

}

UTEST_F_TEARDOWN(spn_build) {
}

void run_test(s32* utest_result, fixture_t* fixture, test_t test) {
  fixture->paths.config = tmpfs_get(&fixture->fs, sp_str_lit(".home/config"));
  fixture->paths.storage = tmpfs_get(&fixture->fs, sp_str_lit(".home/storage"));
  fixture->paths.include = sp_fs_join_path(fixture->paths.storage, sp_str_lit("spn/include"));
  fixture->paths.index = sp_fs_join_path(fixture->paths.storage, sp_str_lit("spn/packages"));
  sp_fs_create_dir(fixture->paths.config);
  sp_fs_create_dir(fixture->paths.storage);
  sp_fs_create_dir(fixture->paths.include);
  sp_fs_create_dir(fixture->paths.index);

  sp_fs_copy(sp_fs_join_path(fixture->paths.root, sp_str_lit("include/spn.h")), fixture->paths.include);

  //
  if (test.project) {
    sp_str_t project = sp_fs_join_path(fixture->paths.root, sp_str_view(test.project));
    ASSERT_TRUE(sp_fs_exists(project));

    // copy the files that nearly always exist automatically, for ergonomics
    const c8* copy [] = {
      "main.c",
      "spn.c",
      "spn.toml",
    };

    sp_carr_for(copy, it) {
      sp_str_t from = sp_fs_join_path(project, sp_str_view(copy[it]));
      if (sp_fs_exists(from)) {
        sp_fs_copy(from, fixture->fs.root);
      }
    }

    sp_carr_for(test.copy, it) {
      if (!test.copy[it]) {
        break;
      }

      copy_project_path(utest_result, &fixture->fs, project, sp_str_view(test.copy[it]));
    }

    setup_fixture_index_from_remote(utest_result, &fixture->fs, fixture->paths.index, project);
  }

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
        ASSERT_TRUE(sp_fs_exists(bin));

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
      case ACTION_VERIFY_INCLUDE: {
        sp_str_t path = tmpfs_get(
          &fixture->fs,
          sp_fs_join_path(sp_str_lit("build/debug/store/include"), action.verify_include.file)
        );
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

UTEST_F(spn_build, index_package_pinned_commit) {
  tmpfs_init_named(&uf->fixture.fs, "index_package_pinned_commit");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/spn_build/index_package_pinned_commit",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_RUN_BIN, .bin = { .name = "index_package_pinned_commit", .rc = 0 } },
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

UTEST_F(spn_build, api_basic_node) {
  tmpfs_init_named(&uf->fixture.fs, "api_basic_node");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/api/basic_node",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_INCLUDE, .verify_include.file = sp_str_lit("version.h") },
      { .kind = ACTION_RUN_BIN, .bin.name = "basic_node" },
    },
  });
}

UTEST_F(spn_build, api_chained_nodes) {
  tmpfs_init_named(&uf->fixture.fs, "api_chained_nodes");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/api/chained_nodes",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = sp_str_lit("build/debug/work/chained_nodes/final.h") },
      { .kind = ACTION_RUN_BIN, .bin.name = "chained_nodes" },
    },
  });
}

UTEST_F(spn_build, api_cross_package) {
  tmpfs_init_named(&uf->fixture.fs, "api_cross_package");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/api/cross_package",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = sp_str_lit("build/debug/work/cross_package/dep_info.h") },
      { .kind = ACTION_VERIFY_PKG_LOCKED, .verify_locked.name = "spn_log" },
      { .kind = ACTION_RUN_BIN, .bin.name = "cross_package" },
    },
  });
}

UTEST_F(spn_build, api_diamond_deps) {
  tmpfs_init_named(&uf->fixture.fs, "api_diamond_deps");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/api/diamond_deps",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = sp_str_lit("build/debug/work/diamond_deps/final.h") },
      { .kind = ACTION_RUN_BIN, .bin.name = "diamond_deps" },
    },
  });
}

UTEST_F(spn_build, api_fan_in) {
  tmpfs_init_named(&uf->fixture.fs, "api_fan_in");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/api/fan_in",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = sp_str_lit("build/debug/work/fan_in/combined.h") },
      { .kind = ACTION_RUN_BIN, .bin.name = "fan_in" },
    },
  });
}

UTEST_F(spn_build, api_multi_output) {
  tmpfs_init_named(&uf->fixture.fs, "api_multi_output");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/api/multi_output",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = sp_str_lit("build/debug/work/multi_output/all.h") },
      { .kind = ACTION_RUN_BIN, .bin.name = "multi_output" },
    },
  });
}

UTEST_F(spn_build, api_node_linking) {
  tmpfs_init_named(&uf->fixture.fs, "api_node_linking");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/api/node_linking",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = sp_str_lit("build/debug/work/node_linking/generated.h") },
      { .kind = ACTION_RUN_BIN, .bin.name = "node_linking" },
    },
  });
}

UTEST_F(spn_build, api_orphan_outputs) {
  tmpfs_init_named(&uf->fixture.fs, "api_orphan_outputs");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/api/orphan_outputs",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = sp_str_lit("build/debug/work/orphan_outputs/orphan_header.h") },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = sp_str_lit("build/debug/work/orphan_outputs/consumed_header.h") },
      { .kind = ACTION_RUN_BIN, .bin.name = "orphan_outputs" },
    },
  });
}

UTEST_F(spn_build, api_stamp_chain) {
  tmpfs_init_named(&uf->fixture.fs, "api_stamp_chain");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/api/stamp_chain",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = sp_str_lit("build/debug/work/stamp_chain/result.h") },
      { .kind = ACTION_RUN_BIN, .bin.name = "stamp_chain" },
    },
  });
}

UTEST_F(spn_build, api_stamp_input) {
  tmpfs_init_named(&uf->fixture.fs, "api_stamp_input");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/api/stamp_input",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = sp_str_lit("build/debug/work/stamp_input/validated.h") },
      { .kind = ACTION_RUN_BIN, .bin.name = "stamp_input" },
    },
  });
}

UTEST_F(spn_build, api_user_data) {
  tmpfs_init_named(&uf->fixture.fs, "api_user_data");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/fixtures/api/user_data",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_EXISTS, .verify_exists.file = sp_str_lit("build/debug/work/user_data/config.h") },
      { .kind = ACTION_RUN_BIN, .bin.name = "user_data" },
    },
  });
}
