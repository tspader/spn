#include "dag_test.h"

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

static const tree_store_test_t tree_store_tests [] = {
  {
    .name = "roundtrip",
    .ops = {
      { .kind = TREE_OP_PUT, .files = { { "X.h", "A" }, { "B/Y.h", "B" } } },
      { .kind = TREE_OP_HAS, .expect = { .hit = true } },
      { .kind = TREE_OP_MATERIALIZE, .expect = { .files = { { "X.h", "A" }, { "B/Y.h", "B" } } } },
    }
  },
  {
    .name = "has_missing",
    .ops = {
      { .kind = TREE_OP_HAS },
    }
  },
  {
    .name = "materialize_missing_fails",
    .ops = {
      { .kind = TREE_OP_MATERIALIZE, .expect = { .err = SPN_ERR_DAG_STORE_MISSING } },
    }
  },
  {
    .name = "stale_files_removed",
    .ops = {
      { .kind = TREE_OP_PUT, .files = { { "X.h", "A" } } },
      { .kind = TREE_OP_STALE, .stale = "Z.h" },
      { .kind = TREE_OP_MATERIALIZE, .expect = { .files = { { "X.h", "A" } }, .absent = { "Z.h" } } },
    }
  },
  {
    .name = "stale_subdir_removed",
    .ops = {
      { .kind = TREE_OP_PUT, .files = { { "X.h", "A" } } },
      { .kind = TREE_OP_STALE, .stale = "Z/W.h" },
      { .kind = TREE_OP_MATERIALIZE, .expect = { .files = { { "X.h", "A" } }, .absent = { "Z" } } },
    }
  },
  {
    .name = "empty_tree",
    .ops = {
      { .kind = TREE_OP_PUT },
      { .kind = TREE_OP_HAS, .expect = { .hit = true } },
      { .kind = TREE_OP_MATERIALIZE },
    }
  },
};

static sp_err_t tree_run_store_ops(sp_test_t* t, spn_dag_store_kind_t kind, const tree_store_test_t* test) {
  sp_test_kv_c(t, "store", dag_test_store_name(kind));

  dag_test_env_t env;
  dag_test_env_init(&env, t, (dag_test_env_config_t) {
    .sub = dag_test_store_name(kind),
    .store = kind
  });
  sp_str_t src = dag_test_env_path(&env, sp_str_lit("src"));
  sp_str_t dst = dag_test_env_path(&env, sp_str_lit("dst"));
  spn_dag_digest_t digest = sp_zero;

  sp_carr_for(test->ops, it) {
    tree_op_t op = test->ops[it];
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
          dag_test_create(sp_fs_join_path(env.mem, src, sp_cstr_as_str(op.files[fi].path)), sp_str_view(op.files[fi].content));
        }
        sp_expect_eq(t, op.expect.err, spn_dag_store_put_tree(&env.store, src, &digest));
        break;
      }
      case TREE_OP_HAS: {
        sp_expect_eq(t, op.expect.hit, spn_dag_store_has_tree(&env.store, digest));
        break;
      }
      case TREE_OP_MATERIALIZE: {
        sp_expect_eq(t, op.expect.err, spn_dag_store_materialize_tree(&env.store, digest, dst));
        if (op.expect.err) {
          break;
        }
        sp_carr_for(op.expect.files, fi) {
          if (!op.expect.files[fi].path) {
            break;
          }
          sp_err_t err = dag_test_expect_file(t, env.mem, sp_fs_join_path(env.mem, dst, sp_cstr_as_str(op.expect.files[fi].path)), op.expect.files[fi].content);
          if (err) {
            return err;
          }
        }
        sp_carr_for(op.expect.absent, fi) {
          if (!op.expect.absent[fi]) {
            break;
          }
          sp_expect(t, !sp_fs_exists(sp_fs_join_path(env.mem, dst, sp_cstr_as_str(op.expect.absent[fi]))));
        }
        break;
      }
      case TREE_OP_STALE: {
        dag_test_create(sp_fs_join_path(env.mem, dst, sp_cstr_as_str(op.stale)), sp_str_lit("stale"));
        break;
      }
    }
  }

  return SP_OK;
}

sp_test_each(dag_tree, store, tree_store_test_t, tree_store_tests) {
  sp_carr_for(dag_test_store_kinds, kind) {
    sp_err_t err = tree_run_store_ops(t, dag_test_store_kinds[kind], it);
    if (err) {
      return err;
    }
  }
  return SP_OK;
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
  const tree_exec_run_t* run;
} tree_exec_env_t;

static const tree_exec_test_t tree_exec_tests [] = {
  {
    .name = "exec_materializes_target",
    .runs = {
      { .identity = "I", .files = { { "X.h", "A" }, { "B/Y.h", "B" } }, .expect_runs = 1, .expect_files = { { "X.h", "A" }, { "B/Y.h", "B" } } },
    }
  },
  {
    .name = "exec_restores_deleted_target",
    .runs = {
      { .identity = "I", .files = { { "X.h", "A" } }, .expect_runs = 1 },
      { .identity = "I", .files = { { "X.h", "A" } }, .remove_target = true, .expect_runs = 1, .expect_files = { { "X.h", "A" } } },
    }
  },
  {
    .name = "exec_identity_change_reruns",
    .runs = {
      { .identity = "I", .files = { { "X.h", "A" } }, .expect_runs = 1 },
      { .identity = "J", .files = { { "X.h", "B" } }, .expect_runs = 2, .expect_files = { { "X.h", "B" } } },
    }
  },
  {
    .name = "exec_tampered_target_restored",
    .runs = {
      { .identity = "I", .files = { { "X.h", "A" } }, .expect_runs = 1 },
      { .identity = "I", .files = { { "X.h", "A" } }, .tamper = { "X.h", "T" }, .expect_runs = 1, .expect_files = { { "X.h", "A" } } },
    }
  },
};

static spn_err_t tree_exec_fn(spn_dag_t* g, spn_dag_action_t* action, void* user_data, spn_dag_env_t* dag_env, sp_mem_t mem, sp_da(spn_dag_obs_t)* obs) {
  tree_exec_env_t* env = (tree_exec_env_t*)user_data;
  env->dag.runs++;
  spn_dag_artifact_t* out = spn_dag_find_artifact(env->dag.g, action->produces[0]);
  sp_fs_create_dir(dag_test_render(&env->dag, out->materialized));
  sp_carr_for(env->run->files, it) {
    if (!env->run->files[it].path) {
      break;
    }
    sp_str_t path = dag_test_render(&env->dag, spn_path_join(env->dag.mem, out->materialized, sp_str_view(env->run->files[it].path)));
    sp_fs_create_dir(sp_fs_parent_path(path));
    if (sp_fs_create_file_str(path, sp_str_view(env->run->files[it].content))) {
      return SPN_ERR_DAG_ACTION;
    }
  }
  return SPN_OK;
}

sp_test_each(dag_tree, exec, tree_exec_test_t, tree_exec_tests) {
  tree_exec_env_t env = sp_zero;
  dag_test_env_init(&env.dag, t, (dag_test_env_config_t) { .store = SPN_DAG_STORE_FILESYSTEM });
  sp_str_t target = dag_test_env_path(&env.dag, sp_str_lit("install"));
  spn_path_t tree = dag_test_env_rooted(&env.dag, sp_str_lit("install"));

  sp_carr_for(it->runs, r) {
    const tree_exec_run_t* run = &it->runs[r];
    if (!run->expect_runs) {
      break;
    }

    env.run = run;
    spn_dag_file_cache_invalidate_all(&env.dag.files);
    if (run->remove_target) {
      sp_fs_remove_dir(target);
    }
    if (run->tamper.path) {
      sp_str_t path = sp_fs_join_path(env.dag.mem, target, sp_str_view(run->tamper.path));
      sp_fs_remove_file(path);
      sp_fs_create_file_str(path, sp_str_view(run->tamper.content));
    }

    spn_dag_t* g = dag_test_env_graph(&env.dag);
    spn_dag_id_t action = spn_dag_add_action(g, (spn_dag_action_config_t) {
      .identity = dag_test_digest(run->identity),
      .execute = tree_exec_fn,
      .user_data = &env
    });
    sp_must_eq(t, SPN_OK, spn_dag_action_add_output(g, action, spn_dag_add_tree(g, tree)));

    sp_expect_eq(t, SPN_OK, spn_dag_execute(g, action, &env.dag.env));
    sp_expect_eq(t, run->expect_runs, env.dag.runs);

    sp_carr_for(run->expect_files, ft) {
      if (!run->expect_files[ft].path) {
        break;
      }
      sp_err_t err = dag_test_expect_file(t, env.dag.mem, sp_fs_join_path(env.dag.mem, target, sp_str_view(run->expect_files[ft].path)), run->expect_files[ft].content);
      if (err) {
        return err;
      }
    }
  }

  return SP_OK;
}
