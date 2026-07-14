#include "common.h"

typedef struct {
  const c8* path;
  const c8* content;
} par_source_t;

typedef struct {
  const c8* identity;
  const c8* inputs [DAG_TEST_MAX_INPUTS];
  const c8* discovers [DAG_TEST_MAX_INPUTS];
  const c8* output;
  bool tree;
  bool fails;
} par_action_t;

typedef struct {
  par_source_t sources [DAG_TEST_MAX_INPUTS];
  const c8* remove_dirs [DAG_TEST_MAX_INPUTS];
  spn_err_t expect_err;
  u32 expect_runs;
} par_build_t;

typedef struct {
  const c8* name;
  u32 workers;
  bool discovery;
  par_action_t actions [DAG_TEST_MAX_OPS];
  par_build_t builds [DAG_TEST_MAX_OPS];
} par_test_t;

typedef struct {
  dag_test_env_t dag;
  sp_atomic_s32_t runs;
} par_env_t;

typedef struct {
  par_env_t* env;
  spn_dag_t* g;
  par_action_t* spec;
} par_ctx_t;

UTEST_EMPTY_FIXTURE(parallel)

static s32 par_exec(spn_dag_action_t* action, void* user_data) {
  par_ctx_t* ctx = (par_ctx_t*)user_data;
  if (ctx->spec->fails) {
    return 1;
  }
  sp_da_for(action->consumes, it) {
    spn_dag_artifact_t* in = spn_dag_find_artifact(ctx->g, action->consumes[it]);
    if (in->kind == SPN_DAG_ARTIFACT_KIND_FILE && !sp_fs_exists(in->path)) {
      return 1;
    }
  }

  sp_atomic_s32_add(&ctx->env->runs, 1);
  spn_dag_artifact_t* out = spn_dag_find_artifact(ctx->g, action->produces[0]);
  sp_str_t content = sp_str_view(ctx->spec->identity);
  if (out->kind == SPN_DAG_ARTIFACT_KIND_TREE) {
    sp_mem_arena_marker_t s = sp_mem_begin_scratch();
    sp_err_t err = sp_fs_create_file_str(sp_fs_join_path(s.mem, out->path, sp_str_lit("H")), content);
    sp_mem_end_scratch(s);
    return err ? 1 : 0;
  }
  return sp_fs_create_file_str(out->path, content) ? 1 : 0;
}

static spn_err_t par_discover(spn_dag_action_t* action, void* user_data, sp_mem_t mem, sp_da(spn_dag_obs_t)* out) {
  par_ctx_t* ctx = (par_ctx_t*)user_data;
  sp_carr_for(ctx->spec->discovers, it) {
    if (!ctx->spec->discovers[it]) {
      break;
    }
    sp_da_push(*out, ((spn_dag_obs_t) {
      .kind = SPN_DAG_OBS_FILE,
      .path = sp_fs_join_path(mem, ctx->env->dag.fs.root, sp_str_view(ctx->spec->discovers[it]))
    }));
  }
  return SPN_OK;
}

static void par_build_graph(s32* utest_result, par_env_t* env, spn_dag_t* g, par_test_t* t) {
  sp_carr_for(t->actions, ai) {
    par_action_t* spec = &t->actions[ai];
    if (!spec->identity) {
      break;
    }

    par_ctx_t* ctx = sp_alloc_type(env->dag.fs.mem, par_ctx_t);
    ctx->env = env;
    ctx->g = g;
    ctx->spec = spec;

    spn_dag_id_t action = spn_dag_add_action(g, (spn_dag_action_config_t) {
      .identity = dag_test_digest(spec->identity),
      .execute = par_exec,
      .discover = spec->discovers[0] ? par_discover : SP_NULLPTR,
      .user_data = ctx
    });
    sp_carr_for(spec->inputs, ii) {
      if (!spec->inputs[ii]) {
        break;
      }
      spn_dag_action_add_input(g, action, spn_dag_add_file(g, tmpfs_get(&env->dag.fs, sp_str_view(spec->inputs[ii]))));
    }
    sp_str_t output = tmpfs_get(&env->dag.fs, sp_str_view(spec->output));
    spn_dag_id_t out = spec->tree ? spn_dag_add_tree(g, output) : spn_dag_add_file(g, output);
    ASSERT_EQ(SPN_OK, spn_dag_action_add_output(g, action, out));
  }
}

static void par_expect_outputs(s32* utest_result, par_env_t* env, par_test_t* t) {
  sp_carr_for(t->actions, ai) {
    par_action_t* spec = &t->actions[ai];
    if (!spec->identity) {
      break;
    }
    sp_str_t path = tmpfs_get(&env->dag.fs, sp_str_view(spec->output));
    if (spec->tree) {
      path = sp_fs_join_path(env->dag.fs.mem, path, sp_str_lit("H"));
    }
    dag_test_expect_file(utest_result, env->dag.fs.mem, path, spec->identity);
  }
}

static void run_test(s32* utest_result, par_test_t t) {
  if (!sp_str_empty(sp_os_env_get(sp_str_lit("SPN_TEST_SIM")))) {
    UTEST_SKIP("threaded executor is incompatible with the single-threaded sim");
  }

  sp_carr_for(dag_test_store_kinds, kind) {
    par_env_t env = sp_zero;
    dag_test_env_init(&env.dag, (dag_test_env_config_t) {
      .name = t.name,
      .store = dag_test_store_kinds[kind],
      .discovery = t.discovery
    });

    spn_dag_pool_t pool = sp_zero;
    spn_dag_pool_init(&pool, env.dag.fs.mem, t.workers ? t.workers : 4);

    sp_carr_for(t.builds, b) {
      par_build_t* build = &t.builds[b];
      if (!build->expect_runs && !build->expect_err) {
        break;
      }

      spn_dag_file_cache_refresh(&env.dag.files);
      sp_carr_for(build->sources, si) {
        if (!build->sources[si].path) {
          break;
        }
        tmpfs_create(&env.dag.fs, sp_str_view(build->sources[si].path), sp_str_view(build->sources[si].content));
      }
      sp_carr_for(build->remove_dirs, si) {
        if (!build->remove_dirs[si]) {
          break;
        }
        sp_fs_remove_dir(tmpfs_get(&env.dag.fs, sp_str_view(build->remove_dirs[si])));
      }

      spn_dag_t* g = dag_test_env_graph(&env.dag);
      par_build_graph(utest_result, &env, g, &t);

      spn_err_t err = spn_dag_run_ex(g, &env.dag.env, &pool.executor);
      EXPECT_EQ(build->expect_err, err);
      EXPECT_EQ((s32)build->expect_runs, sp_atomic_s32_get(&env.runs));

      if (!err && !build->expect_err) {
        par_expect_outputs(utest_result, &env, &t);
      }
    }

    spn_dag_pool_deinit(&pool);
    dag_test_env_deinit(&env.dag);
  }
}

UTEST_F(parallel, independent_actions_all_run) {
  run_test(&ur, (par_test_t) {
    .name = "parallel_independent",
    .actions = {
      { .identity = "A", .inputs = { "S" }, .output = "OA" },
      { .identity = "B", .inputs = { "S" }, .output = "OB" },
      { .identity = "C", .inputs = { "S" }, .output = "OC" },
      { .identity = "D", .inputs = { "S" }, .output = "OD" },
      { .identity = "E", .inputs = { "S" }, .output = "OE" },
      { .identity = "F", .inputs = { "S" }, .output = "OF" },
      { .identity = "G", .inputs = { "S" }, .output = "OG" },
    },
    .builds = {
      { .sources = { { "S", "1" } }, .expect_runs = 7 },
      { .sources = { { "S", "1" } }, .expect_runs = 7 },
    }
  });
}

UTEST_F(parallel, chain_runs_in_dependency_order) {
  run_test(&ur, (par_test_t) {
    .name = "parallel_chain",
    .actions = {
      { .identity = "A", .inputs = { "S" }, .output = "X" },
      { .identity = "B", .inputs = { "X" }, .output = "Y" },
      { .identity = "C", .inputs = { "Y" }, .output = "Z" },
    },
    .builds = {
      { .sources = { { "S", "1" } }, .expect_runs = 3 },
      { .sources = { { "S", "1" } }, .expect_runs = 3 },
    }
  });
}

UTEST_F(parallel, diamond_selective_rebuild) {
  run_test(&ur, (par_test_t) {
    .name = "parallel_diamond",
    .actions = {
      { .identity = "A", .inputs = { "S" }, .output = "X" },
      { .identity = "B", .inputs = { "T" }, .output = "Y" },
      { .identity = "C", .inputs = { "X", "Y" }, .output = "Z" },
    },
    .builds = {
      { .sources = { { "S", "1" }, { "T", "1" } }, .expect_runs = 3 },
      { .sources = { { "S", "2" }, { "T", "1" } }, .expect_runs = 4 },
    }
  });
}

UTEST_F(parallel, single_worker_completes) {
  run_test(&ur, (par_test_t) {
    .name = "parallel_single_worker",
    .workers = 1,
    .actions = {
      { .identity = "A", .inputs = { "S" }, .output = "X" },
      { .identity = "B", .inputs = { "X" }, .output = "Y" },
      { .identity = "C", .inputs = { "S" }, .output = "Z" },
    },
    .builds = {
      { .sources = { { "S", "1" } }, .expect_runs = 3 },
    }
  });
}

UTEST_F(parallel, failing_action_stops_build) {
  run_test(&ur, (par_test_t) {
    .name = "parallel_fail",
    .actions = {
      { .identity = "A", .inputs = { "S" }, .output = "X", .fails = true },
      { .identity = "B", .inputs = { "X" }, .output = "Y" },
    },
    .builds = {
      { .sources = { { "S", "1" } }, .expect_err = SPN_ERR_DAG_ACTION },
    }
  });
}

UTEST_F(parallel, cycle_fails) {
  run_test(&ur, (par_test_t) {
    .name = "parallel_cycle",
    .actions = {
      { .identity = "A", .inputs = { "Y" }, .output = "X" },
      { .identity = "B", .inputs = { "X" }, .output = "Y" },
    },
    .builds = {
      { .expect_err = SPN_ERR_DAG_STALLED },
    }
  });
}

UTEST_F(parallel, discovered_generated_header_waits_for_producer) {
  run_test(&ur, (par_test_t) {
    .name = "parallel_generated_header",
    .discovery = true,
    .actions = {
      { .identity = "A", .inputs = { "S" }, .output = "H" },
      { .identity = "B", .inputs = { "M" }, .discovers = { "H" }, .output = "O" },
    },
    .builds = {
      { .sources = { { "S", "1" }, { "M", "1" } }, .expect_runs = 3 },
      { .sources = { { "S", "1" }, { "M", "1" } }, .expect_runs = 3 },
      { .sources = { { "S", "2" }, { "M", "1" } }, .expect_runs = 4 },
    }
  });
}

UTEST_F(parallel, discovered_tree_member_waits_for_producer) {
  run_test(&ur, (par_test_t) {
    .name = "parallel_tree_member",
    .discovery = true,
    .actions = {
      { .identity = "A", .inputs = { "S" }, .output = "D", .tree = true },
      { .identity = "B", .inputs = { "M" }, .discovers = { "D/H" }, .output = "O" },
    },
    .builds = {
      { .sources = { { "S", "1" }, { "M", "1" } }, .expect_runs = 3 },
      { .sources = { { "S", "1" }, { "M", "1" } }, .remove_dirs = { "D" }, .expect_runs = 3 },
    }
  });
}

UTEST_F(parallel, tree_restored_after_delete) {
  run_test(&ur, (par_test_t) {
    .name = "parallel_tree_restore",
    .actions = {
      { .identity = "A", .inputs = { "S" }, .output = "D", .tree = true },
      { .identity = "B", .inputs = { "S" }, .output = "O" },
    },
    .builds = {
      { .sources = { { "S", "1" } }, .expect_runs = 2 },
      { .sources = { { "S", "1" } }, .remove_dirs = { "D" }, .expect_runs = 2 },
    }
  });
}
