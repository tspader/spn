#include "common.h"

typedef struct {
  const c8* path;
  const c8* content;
} run_source_t;

typedef struct {
  const c8* identity;
  const c8* inputs [DAG_TEST_MAX_INPUTS];
  const c8* discovers [DAG_TEST_MAX_INPUTS];
  const c8* output;
  bool tree;
  bool fails;
} run_action_t;

typedef struct {
  run_source_t sources [DAG_TEST_MAX_INPUTS];
  const c8* remove_dirs [DAG_TEST_MAX_INPUTS];
  spn_err_t expect_err;
  u32 expect_runs;
} run_build_t;

typedef struct {
  const c8* name;
  bool discovery;
  run_action_t actions [DAG_TEST_MAX_OPS];
  run_build_t builds [DAG_TEST_MAX_OPS];
} run_test_t;

typedef struct {
  dag_test_env_t* env;
  spn_dag_t* g;
  run_action_t* spec;
} run_ctx_t;

UTEST_EMPTY_FIXTURE(run)

static s32 on_exec(spn_dag_action_t* action, void* user_data) {
  run_ctx_t* ctx = (run_ctx_t*)user_data;
  if (ctx->spec->fails) {
    return 1;
  }
  sp_da_for(action->consumes, it) {
    spn_dag_artifact_t* in = spn_dag_find_artifact(ctx->g, action->consumes[it]);
    if (in->kind == SPN_DAG_ARTIFACT_KIND_FILE && !sp_fs_exists(in->path)) {
      return 1;
    }
  }
  ctx->env->runs++;
  spn_dag_artifact_t* out = spn_dag_find_artifact(ctx->g, action->produces[0]);
  sp_str_t content = sp_fmt(ctx->env->fs.mem, "{}", sp_fmt_uint(ctx->env->runs)).value;
  if (out->kind == SPN_DAG_ARTIFACT_KIND_TREE) {
    return sp_fs_create_file_str(sp_fs_join_path(ctx->env->fs.mem, out->path, sp_str_lit("H")), sp_str_lit("T")) ? 1 : 0;
  }
  return sp_fs_create_file_str(out->path, content) ? 1 : 0;
}

static spn_err_t on_discover(spn_dag_action_t* action, void* user_data, sp_mem_t mem, sp_da(spn_dag_obs_t)* out) {
  run_ctx_t* ctx = (run_ctx_t*)user_data;
  sp_carr_for(ctx->spec->discovers, it) {
    if (!ctx->spec->discovers[it]) {
      break;
    }
    sp_da_push(*out, ((spn_dag_obs_t) {
      .kind = SPN_DAG_OBS_FILE,
      .path = tmpfs_get(&ctx->env->fs, sp_str_view(ctx->spec->discovers[it]))
    }));
  }
  return SPN_OK;
}

static void run_dag(s32* utest_result, dag_test_env_t* env, spn_dag_t* g, run_test_t* t) {
  sp_carr_for(t->actions, ai) {
    run_action_t* spec = &t->actions[ai];
    if (!spec->identity) {
      break;
    }

    run_ctx_t* ctx = sp_alloc_type(env->fs.mem, run_ctx_t);
    ctx->env = env;
    ctx->g = g;
    ctx->spec = spec;

    spn_dag_id_t action = spn_dag_add_action(g, (spn_dag_action_config_t) {
      .identity = dag_test_digest(spec->identity),
      .execute = on_exec,
      .discover = spec->discovers[0] ? on_discover : SP_NULLPTR,
      .user_data = ctx
    });
    sp_carr_for(spec->inputs, ii) {
      if (!spec->inputs[ii]) {
        break;
      }
      spn_dag_action_add_input(g, action, spn_dag_add_file(g, tmpfs_get(&env->fs, sp_str_view(spec->inputs[ii]))));
    }
    sp_str_t output = tmpfs_get(&env->fs, sp_str_view(spec->output));
    spn_dag_id_t out_id = spec->tree ? spn_dag_add_tree(g, output) : spn_dag_add_file(g, output);
    ASSERT_EQ(SPN_OK, spn_dag_action_add_output(g, action, out_id));
  }
}

static void run_test(s32* utest_result, run_test_t t) {
  dag_test_env_t env = sp_zero;
  dag_test_env_init(&env, (dag_test_env_config_t) {
    .name = t.name,
    .store = SPN_DAG_STORE_MEM,
    .discovery = t.discovery
  });

  sp_carr_for(t.builds, b) {
    run_build_t* build = &t.builds[b];
    if (!build->expect_runs && !build->expect_err) {
      break;
    }

    spn_dag_file_cache_refresh(&env.files);
    sp_carr_for(build->sources, si) {
      if (!build->sources[si].path) {
        break;
      }
      sp_fs_create_file_str(tmpfs_get(&env.fs, sp_str_view(build->sources[si].path)), sp_str_view(build->sources[si].content));
    }
    sp_carr_for(build->remove_dirs, si) {
      if (!build->remove_dirs[si]) {
        break;
      }
      sp_fs_remove_dir(tmpfs_get(&env.fs, sp_str_view(build->remove_dirs[si])));
    }

    spn_dag_t* g = dag_test_env_graph(&env);
    run_dag(utest_result, &env, g, &t);

    spn_err_t err = spn_dag_run(g, &env.env);
    EXPECT_EQ(build->expect_err, err);
    EXPECT_EQ(build->expect_runs, env.runs);
  }

  dag_test_env_deinit(&env);
}

UTEST_F(run, chain_runs_in_dependency_order) {
  run_test(&ur, (run_test_t) {
    .name = "run_chain",
    .actions = {
      { .identity = "I", .inputs = { "S" }, .output = "X" },
      { .identity = "J", .inputs = { "X" }, .output = "Y" },
    },
    .builds = {
      { .sources = { { "S", "A" } }, .expect_runs = 2 },
    }
  });
}

UTEST_F(run, second_build_all_hits) {
  run_test(&ur, (run_test_t) {
    .name = "run_hits",
    .actions = {
      { .identity = "I", .inputs = { "S" }, .output = "X" },
      { .identity = "J", .inputs = { "X" }, .output = "Y" },
    },
    .builds = {
      { .sources = { { "S", "A" } }, .expect_runs = 2 },
      { .sources = { { "S", "A" } }, .expect_runs = 2 },
    }
  });
}

UTEST_F(run, source_change_reruns_chain) {
  run_test(&ur, (run_test_t) {
    .name = "run_source_change",
    .actions = {
      { .identity = "I", .inputs = { "S" }, .output = "X" },
      { .identity = "J", .inputs = { "X" }, .output = "Y" },
    },
    .builds = {
      { .sources = { { "S", "A" } }, .expect_runs = 2 },
      { .sources = { { "S", "B" } }, .expect_runs = 4 },
    }
  });
}

UTEST_F(run, independent_actions_both_run) {
  run_test(&ur, (run_test_t) {
    .name = "run_independent",
    .actions = {
      { .identity = "I", .inputs = { "S" }, .output = "X" },
      { .identity = "J", .inputs = { "T" }, .output = "Y" },
    },
    .builds = {
      { .sources = { { "S", "A" }, { "T", "B" } }, .expect_runs = 2 },
    }
  });
}

UTEST_F(run, diamond_selective_rebuild) {
  run_test(&ur, (run_test_t) {
    .name = "run_diamond",
    .actions = {
      { .identity = "I", .inputs = { "S" }, .output = "X" },
      { .identity = "J", .inputs = { "T" }, .output = "Y" },
      { .identity = "K", .inputs = { "X", "Y" }, .output = "Z" },
    },
    .builds = {
      { .sources = { { "S", "A" }, { "T", "B" } }, .expect_runs = 3 },
      { .sources = { { "S", "C" }, { "T", "B" } }, .expect_runs = 5 },
    }
  });
}

UTEST_F(run, missing_source_fails) {
  run_test(&ur, (run_test_t) {
    .name = "run_missing_source",
    .actions = {
      { .identity = "I", .inputs = { "S" }, .output = "X" },
    },
    .builds = {
      { .expect_err = SPN_ERR_DAG_MISSING_INPUT },
    }
  });
}

UTEST_F(run, cycle_fails) {
  run_test(&ur, (run_test_t) {
    .name = "run_cycle",
    .actions = {
      { .identity = "I", .inputs = { "Y" }, .output = "X" },
      { .identity = "J", .inputs = { "X" }, .output = "Y" },
    },
    .builds = {
      { .expect_err = SPN_ERR_DAG_STALLED },
    }
  });
}

UTEST_F(run, failing_action_stops_build) {
  run_test(&ur, (run_test_t) {
    .name = "run_failing_action",
    .actions = {
      { .identity = "I", .inputs = { "S" }, .output = "X", .fails = true },
      { .identity = "J", .inputs = { "X" }, .output = "Y" },
    },
    .builds = {
      { .sources = { { "S", "A" } }, .expect_err = SPN_ERR_DAG_ACTION },
    }
  });
}

UTEST_F(run, discovered_generated_header_waits_for_producer) {
  run_test(&ur, (run_test_t) {
    .name = "run_generated_header",
    .discovery = true,
    .actions = {
      { .identity = "I", .inputs = { "S" }, .output = "H" },
      { .identity = "J", .inputs = { "M" }, .discovers = { "H" }, .output = "O" },
    },
    .builds = {
      { .sources = { { "S", "A" }, { "M", "B" } }, .expect_runs = 3 },
      { .sources = { { "S", "A" }, { "M", "B" } }, .expect_runs = 3 },
      { .sources = { { "S", "C" }, { "M", "B" } }, .expect_runs = 5 },
    }
  });
}

UTEST_F(run, discovered_tree_member_waits_for_producer) {
  run_test(&ur, (run_test_t) {
    .name = "run_tree_member",
    .discovery = true,
    .actions = {
      { .identity = "I", .inputs = { "S" }, .output = "D", .tree = true },
      { .identity = "J", .inputs = { "M" }, .discovers = { "D/H" }, .output = "O" },
    },
    .builds = {
      { .sources = { { "S", "A" }, { "M", "B" } }, .expect_runs = 3 },
      { .sources = { { "S", "A" }, { "M", "B" } }, .remove_dirs = { "D" }, .expect_runs = 3 },
    }
  });
}

UTEST_F(run, discovered_source_header_no_deferral) {
  run_test(&ur, (run_test_t) {
    .name = "run_discovered_source",
    .discovery = true,
    .actions = {
      { .identity = "I", .inputs = { "M" }, .discovers = { "H" }, .output = "O" },
    },
    .builds = {
      { .sources = { { "M", "A" }, { "H", "B" } }, .expect_runs = 1 },
      { .sources = { { "M", "A" }, { "H", "B" } }, .expect_runs = 1 },
      { .sources = { { "M", "A" }, { "H", "C" } }, .expect_runs = 2 },
    }
  });
}
