#include "common.h"

typedef struct {
  const c8* path;
  const c8* content;
} tree_file_t;

typedef enum {
  TREE_OP_DONE,
  TREE_OP_PUT,
  TREE_OP_HAS,
  TREE_OP_MATERIALIZE,
  TREE_OP_STALE,
} tree_op_kind_t;

typedef struct {
  bool hit;
  spn_err_t err;
  tree_file_t files [DAG_TEST_MAX_OUTPUTS];
  const c8* absent [DAG_TEST_MAX_OUTPUTS];
} tree_expect_t;

typedef struct {
  tree_op_kind_t kind;
  tree_file_t files [DAG_TEST_MAX_OUTPUTS];
  const c8* stale;
  tree_expect_t expect;
} tree_op_t;

typedef struct {
  const c8* name;
  tree_op_t ops [DAG_TEST_MAX_OPS];
} tree_store_test_t;

UTEST_EMPTY_FIXTURE(tree)

static void run_store_test(s32* utest_result, tree_store_test_t t) {
  sp_carr_for(dag_test_store_kinds, kind) {
    dag_test_env_t env = sp_zero;
    dag_test_env_init(&env, (dag_test_env_config_t) { .name = t.name, .store = dag_test_store_kinds[kind] });
    tmpfs_t* fs = &env.fs;
    sp_str_t src = tmpfs_get(fs, sp_str_lit("src"));
    sp_str_t dst = tmpfs_get(fs, sp_str_lit("dst"));
    spn_dag_digest_t digest = sp_zero;

    sp_carr_for(t.ops, it) {
      tree_op_t op = t.ops[it];
      if (op.kind == TREE_OP_DONE) {
        break;
      }

      switch (op.kind) {
        case TREE_OP_DONE: {
          break;
        }
        case TREE_OP_PUT: {
          sp_fs_create_dir(src);
          sp_carr_for(op.files, fi) {
            if (!op.files[fi].path) {
              break;
            }
            tmpfs_create(fs, sp_fmt(fs->mem, "src/{}", sp_fmt_cstr(op.files[fi].path)).value, sp_str_view(op.files[fi].content));
          }
          EXPECT_EQ(op.expect.err, spn_dag_store_put_tree(&env.store, src, &digest));
          break;
        }
        case TREE_OP_HAS: {
          EXPECT_EQ(op.expect.hit, spn_dag_store_has_tree(&env.store, digest));
          break;
        }
        case TREE_OP_MATERIALIZE: {
          EXPECT_EQ(op.expect.err, spn_dag_store_materialize_tree(&env.store, digest, dst));
          if (op.expect.err) {
            break;
          }
          sp_carr_for(op.expect.files, fi) {
            if (!op.expect.files[fi].path) {
              break;
            }
            dag_test_expect_file(utest_result, fs->mem, tmpfs_get(fs, sp_fmt(fs->mem, "dst/{}", sp_fmt_cstr(op.expect.files[fi].path)).value), op.expect.files[fi].content);
          }
          sp_carr_for(op.expect.absent, fi) {
            if (!op.expect.absent[fi]) {
              break;
            }
            EXPECT_FALSE(sp_fs_exists(tmpfs_get(fs, sp_fmt(fs->mem, "dst/{}", sp_fmt_cstr(op.expect.absent[fi])).value)));
          }
          break;
        }
        case TREE_OP_STALE: {
          tmpfs_create(fs, sp_fmt(fs->mem, "dst/{}", sp_fmt_cstr(op.stale)).value, sp_str_lit("stale"));
          break;
        }
      }
    }

    dag_test_env_deinit(&env);
  }
}

UTEST_F(tree, roundtrip) {
  run_store_test(&ur, (tree_store_test_t) {
    .name = "tree_roundtrip",
    .ops = {
      { .kind = TREE_OP_PUT, .files = { { "X.h", "A" }, { "B/Y.h", "B" } } },
      { .kind = TREE_OP_HAS, .expect = { .hit = true } },
      { .kind = TREE_OP_MATERIALIZE, .expect = { .files = { { "X.h", "A" }, { "B/Y.h", "B" } } } },
    }
  });
}

UTEST_F(tree, has_missing) {
  run_store_test(&ur, (tree_store_test_t) {
    .name = "tree_has_missing",
    .ops = {
      { .kind = TREE_OP_HAS },
    }
  });
}

UTEST_F(tree, materialize_missing_fails) {
  run_store_test(&ur, (tree_store_test_t) {
    .name = "tree_materialize_missing",
    .ops = {
      { .kind = TREE_OP_MATERIALIZE, .expect = { .err = SPN_ERROR } },
    }
  });
}

UTEST_F(tree, stale_files_removed) {
  run_store_test(&ur, (tree_store_test_t) {
    .name = "tree_stale",
    .ops = {
      { .kind = TREE_OP_PUT, .files = { { "X.h", "A" } } },
      { .kind = TREE_OP_STALE, .stale = "Z.h" },
      { .kind = TREE_OP_MATERIALIZE, .expect = { .files = { { "X.h", "A" } }, .absent = { "Z.h" } } },
    }
  });
}

UTEST_F(tree, stale_subdir_removed) {
  run_store_test(&ur, (tree_store_test_t) {
    .name = "tree_stale_subdir",
    .ops = {
      { .kind = TREE_OP_PUT, .files = { { "X.h", "A" } } },
      { .kind = TREE_OP_STALE, .stale = "Z/W.h" },
      { .kind = TREE_OP_MATERIALIZE, .expect = { .files = { { "X.h", "A" } }, .absent = { "Z" } } },
    }
  });
}

UTEST_F(tree, empty_tree) {
  run_store_test(&ur, (tree_store_test_t) {
    .name = "tree_empty",
    .ops = {
      { .kind = TREE_OP_PUT },
      { .kind = TREE_OP_HAS, .expect = { .hit = true } },
      { .kind = TREE_OP_MATERIALIZE },
    }
  });
}

typedef struct {
  const c8* identity;
  tree_file_t files [DAG_TEST_MAX_OUTPUTS];
  bool remove_target;
  tree_file_t tamper;
  u32 expect_runs;
  tree_file_t expect_files [DAG_TEST_MAX_OUTPUTS];
} tree_exec_run_t;

typedef struct {
  const c8* name;
  tree_exec_run_t runs [DAG_TEST_MAX_OPS];
} tree_exec_test_t;

typedef struct {
  dag_test_env_t dag;
  tree_exec_run_t* run;
} tree_exec_env_t;

static s32 tree_exec_fn(spn_dag_action_t* action, void* user_data) {
  tree_exec_env_t* env = (tree_exec_env_t*)user_data;
  env->dag.runs++;
  spn_dag_artifact_t* out = spn_dag_find_artifact(env->dag.g, action->produces[0]);
  sp_fs_create_dir(out->path);
  sp_carr_for(env->run->files, it) {
    if (!env->run->files[it].path) {
      break;
    }
    sp_str_t path = sp_fs_join_path(env->dag.fs.mem, out->path, sp_str_view(env->run->files[it].path));
    sp_fs_create_dir(sp_fs_parent_path(path));
    if (sp_fs_create_file_str(path, sp_str_view(env->run->files[it].content))) {
      return 1;
    }
  }
  return 0;
}

static void run_exec_test(s32* utest_result, tree_exec_test_t t) {
  tree_exec_env_t env = sp_zero;
  dag_test_env_init(&env.dag, (dag_test_env_config_t) { .name = t.name, .store = SPN_DAG_STORE_FILESYSTEM });
  sp_str_t target = tmpfs_get(&env.dag.fs, sp_str_lit("install"));

  sp_carr_for(t.runs, r) {
    tree_exec_run_t* run = &t.runs[r];
    if (!run->expect_runs) {
      break;
    }

    env.run = run;
    spn_dag_file_cache_refresh(&env.dag.files);
    if (run->remove_target) {
      sp_fs_remove_dir(target);
    }
    if (run->tamper.path) {
      sp_str_t path = sp_fs_join_path(env.dag.fs.mem, target, sp_str_view(run->tamper.path));
      sp_fs_remove_file(path);
      sp_fs_create_file_str(path, sp_str_view(run->tamper.content));
    }

    spn_dag_t* g = dag_test_env_graph(&env.dag);
    spn_dag_id_t action = spn_dag_add_action(g, (spn_dag_action_config_t) {
      .identity = dag_test_digest(run->identity),
      .execute = tree_exec_fn,
      .user_data = &env
    });
    ASSERT_EQ(SPN_OK, spn_dag_action_add_output(g, action, spn_dag_add_tree(g, target)));

    EXPECT_EQ(SPN_OK, spn_dag_execute(g, action, &env.dag.env));
    EXPECT_EQ(run->expect_runs, env.dag.runs);

    sp_carr_for(run->expect_files, it) {
      if (!run->expect_files[it].path) {
        break;
      }
      dag_test_expect_file(utest_result, env.dag.fs.mem, sp_fs_join_path(env.dag.fs.mem, target, sp_str_view(run->expect_files[it].path)), run->expect_files[it].content);
    }
  }

  dag_test_env_deinit(&env.dag);
}

UTEST_F(tree, exec_materializes_target) {
  run_exec_test(&ur, (tree_exec_test_t) {
    .name = "tree_exec",
    .runs = {
      { .identity = "I", .files = { { "X.h", "A" }, { "B/Y.h", "B" } }, .expect_runs = 1, .expect_files = { { "X.h", "A" }, { "B/Y.h", "B" } } },
    }
  });
}

UTEST_F(tree, exec_restores_deleted_target) {
  run_exec_test(&ur, (tree_exec_test_t) {
    .name = "tree_exec_restore",
    .runs = {
      { .identity = "I", .files = { { "X.h", "A" } }, .expect_runs = 1 },
      { .identity = "I", .files = { { "X.h", "A" } }, .remove_target = true, .expect_runs = 1, .expect_files = { { "X.h", "A" } } },
    }
  });
}

UTEST_F(tree, exec_identity_change_reruns) {
  run_exec_test(&ur, (tree_exec_test_t) {
    .name = "tree_exec_identity",
    .runs = {
      { .identity = "I", .files = { { "X.h", "A" } }, .expect_runs = 1 },
      { .identity = "J", .files = { { "X.h", "B" } }, .expect_runs = 2, .expect_files = { { "X.h", "B" } } },
    }
  });
}

UTEST_F(tree, exec_tampered_target_restored) {
  run_exec_test(&ur, (tree_exec_test_t) {
    .name = "tree_exec_tamper",
    .runs = {
      { .identity = "I", .files = { { "X.h", "A" } }, .expect_runs = 1 },
      { .identity = "I", .files = { { "X.h", "A" } }, .tamper = { "X.h", "T" }, .expect_runs = 1, .expect_files = { { "X.h", "A" } } },
    }
  });
}
