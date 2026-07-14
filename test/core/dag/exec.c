#include "common.h"

typedef enum {
  EXEC_BEHAVIOR_WRITE,
  EXEC_BEHAVIOR_FAIL,
  EXEC_BEHAVIOR_SKIP_LAST_OUTPUT,
} exec_behavior_t;

typedef enum {
  EXEC_OP_DONE,
  EXEC_OP_RUN,
  EXEC_OP_REMOVE_OUTPUTS,
} exec_op_kind_t;

typedef struct {
  const c8* identity;
  const c8* inputs [DAG_TEST_MAX_INPUTS];
  const c8* outputs [DAG_TEST_MAX_OUTPUTS];
  const c8* write [DAG_TEST_MAX_OUTPUTS];
} exec_action_t;

typedef struct {
  const c8* identity;
  const c8* inputs [DAG_TEST_MAX_INPUTS];
  const c8* outputs [DAG_TEST_MAX_OUTPUTS];
} exec_change_t;

typedef struct {
  spn_err_t err;
  u32 runs;
  const c8* contents [DAG_TEST_MAX_OUTPUTS];
} exec_expect_t;

typedef struct {
  exec_op_kind_t kind;
  exec_behavior_t behavior;
  exec_change_t change;
  const c8* unavailable [DAG_TEST_MAX_OUTPUTS];
  exec_expect_t expect;
} exec_op_t;

typedef struct {
  const c8* name;
  exec_action_t action;
  exec_op_t ops [DAG_TEST_MAX_OPS];
} exec_test_t;

typedef struct {
  tmpfs_t fs;
  sp_mem_t mem;
  spn_dag_store_t store;
  spn_dag_file_cache_t files;
  spn_dag_action_cache_t cache;
  spn_dag_env_t env;
  spn_err_t err;
  u32 runs;
} exec_env_t;

typedef struct {
  spn_dag_t* g;
  exec_action_t* spec;
  exec_behavior_t behavior;
  exec_env_t* env;
} exec_fn_ctx_t;

static const spn_dag_store_kind_t exec_store_kinds [] = {
  SPN_DAG_STORE_MEM,
  SPN_DAG_STORE_FILESYSTEM,
};

UTEST_EMPTY_FIXTURE(exec)

static s32 exec_test_fn(spn_dag_action_t* action, void* user_data) {
  exec_fn_ctx_t* ctx = (exec_fn_ctx_t*)user_data;
  if (ctx->behavior == EXEC_BEHAVIOR_FAIL) {
    return 1;
  }

  ctx->env->runs++;
  u64 count = sp_da_size(action->produces);
  sp_da_for(action->produces, it) {
    if (ctx->behavior == EXEC_BEHAVIOR_SKIP_LAST_OUTPUT && it + 1 == count) {
      continue;
    }
    spn_dag_artifact_t* artifact = spn_dag_find_artifact(ctx->g, action->produces[it]);
    sp_str_t content = sp_fmt(ctx->env->mem, "{}{}", sp_fmt_cstr(ctx->spec->write[it]), sp_fmt_uint(ctx->env->runs)).value;
    if (sp_fs_create_file_str(artifact->path, content)) {
      return 1;
    }
  }
  return 0;
}

static void exec_env_init(exec_env_t* env, const c8* name, spn_dag_store_kind_t kind) {
  tmpfs_init_named(&env->fs, name);
  env->mem = env->fs.mem;
  spn_dag_store_init(&env->store, (spn_dag_store_config_t) {
    .kind = kind,
    .mem = env->mem,
    .dir = tmpfs_get(&env->fs, sp_str_lit("store"))
  });
  spn_dag_file_cache_init(&env->files, env->mem);
  spn_dag_action_cache_init(&env->cache, env->mem, sp_str_lit(""));
  env->env = (spn_dag_env_t) {
    .files = &env->files,
    .cache = &env->cache,
    .store = &env->store,
    .scratch = tmpfs_get(&env->fs, sp_str_lit("scratch"))
  };
}

static void exec_store_context(exec_env_t* env) {
  switch (env->store.kind) {
    case SPN_DAG_STORE_MEM: {
      utest_kv("store", sp_str_lit("memory"));
      break;
    }
    case SPN_DAG_STORE_FILESYSTEM: {
      utest_kv("store", sp_str_lit("filesystem"));
      break;
    }
  }
}

static void exec_action_change(exec_action_t* action, exec_change_t change) {
  if (change.identity) {
    action->identity = change.identity;
  }
  if (change.inputs[0]) {
    sp_carr_for(action->inputs, it) {
      action->inputs[it] = change.inputs[it];
    }
  }
  if (change.outputs[0]) {
    sp_carr_for(action->outputs, it) {
      action->outputs[it] = change.outputs[it];
    }
  }
}

static void exec_action_run(s32* utest_result, exec_env_t* env, exec_action_t spec, exec_op_t op) {
  spn_dag_t* g = spn_dag_new(env->mem);
  exec_fn_ctx_t ctx = {
    .g = g,
    .spec = &spec,
    .behavior = op.behavior,
    .env = env
  };

  spn_dag_digest_t identity = sp_zero;
  if (spec.identity) {
    sp_str_t str = sp_str_view(spec.identity);
    identity = spn_dag_digest(str.data, str.len);
  }

  spn_dag_id_t action = spn_dag_add_action(g, (spn_dag_action_config_t) {
    .identity = identity,
    .execute = exec_test_fn,
    .user_data = &ctx
  });

  sp_carr_for(spec.inputs, it) {
    if (!spec.inputs[it]) {
      break;
    }
    sp_str_t str = sp_str_view(spec.inputs[it]);
    spn_dag_action_add_input(g, action, spn_dag_add_value(g, str.data, str.len));
  }

  sp_carr_for(spec.outputs, it) {
    if (!spec.outputs[it]) {
      break;
    }
    spn_dag_id_t file = spn_dag_add_file(g, tmpfs_get(&env->fs, sp_str_view(spec.outputs[it])));
    exec_store_context(env);
    ASSERT_EQ(SPN_OK, spn_dag_action_add_output(g, action, file));
  }

  if (op.unavailable[0]) {
    spn_dag_action_output_t outputs [DAG_TEST_MAX_OUTPUTS] = sp_zero;
    spn_dag_action_t* a = spn_dag_find_action(g, action);
    u32 count = 0;
    sp_carr_for(op.unavailable, it) {
      if (!op.unavailable[it]) {
        break;
      }
      sp_str_t content = sp_str_view(op.unavailable[it]);
      spn_dag_artifact_t* artifact = spn_dag_find_artifact(g, a->produces[it]);
      outputs[it] = (spn_dag_action_output_t) {
        .name = artifact->name,
        .digest = spn_dag_digest(content.data, content.len)
      };
      count++;
    }
    spn_dag_action_cache_put(&env->cache, spn_dag_action_key(g, action), outputs, count);
  }

  env->err = spn_dag_execute(g, action, &env->env);
  exec_store_context(env);
  EXPECT_EQ(op.expect.err, env->err);
  if (env->err != op.expect.err) {
    return;
  }

  exec_store_context(env);
  EXPECT_EQ(op.expect.runs, env->runs);
  if (env->err) {
    return;
  }

  sp_carr_for(op.expect.contents, it) {
    if (!op.expect.contents[it]) {
      break;
    }
    sp_str_t content = sp_str_view(op.expect.contents[it]);
    sp_str_t from_disk = sp_zero;
    exec_store_context(env);
    ASSERT_EQ(SP_OK, sp_io_read_file(env->mem, tmpfs_get(&env->fs, sp_str_view(spec.outputs[it])), &from_disk));
    exec_store_context(env);
    EXPECT_STR(from_disk, op.expect.contents[it]);

    spn_dag_action_t* a = spn_dag_find_action(g, action);
    spn_dag_artifact_t* artifact = spn_dag_find_artifact(g, a->produces[it]);
    exec_store_context(env);
    ASSERT_TRUE(spn_dag_digest_valid(artifact->digest));
    EXPECT_TRUE(spn_dag_digest_equal(artifact->digest, spn_dag_digest(content.data, content.len)));
  }
}

static void exec_remove_outputs(s32* utest_result, exec_env_t* env, exec_action_t action) {
  sp_carr_for(action.outputs, it) {
    if (!action.outputs[it]) {
      break;
    }
    sp_str_t path = tmpfs_get(&env->fs, sp_str_view(action.outputs[it]));
    env->err = sp_fs_remove_file(path) ? SPN_ERROR : SPN_OK;
    exec_store_context(env);
    EXPECT_EQ(SPN_OK, env->err);
    if (env->err) {
      return;
    }
    exec_store_context(env);
    EXPECT_FALSE(sp_fs_exists(path));
  }
}

static void run_exec_test(s32* utest_result, exec_test_t t) {
  sp_carr_for(exec_store_kinds, kind) {
    exec_env_t env = sp_zero;
    exec_env_init(&env, t.name, exec_store_kinds[kind]);
    exec_action_t action = t.action;

    sp_carr_for(t.ops, it) {
      exec_op_t op = t.ops[it];
      if (op.kind == EXEC_OP_DONE) {
        break;
      }

      switch (op.kind) {
        case EXEC_OP_DONE: {
          break;
        }
        case EXEC_OP_RUN: {
          exec_action_change(&action, op.change);
          exec_action_run(utest_result, &env, action, op);
          break;
        }
        case EXEC_OP_REMOVE_OUTPUTS: {
          exec_remove_outputs(utest_result, &env, action);
          break;
        }
      }

      if (env.err != op.expect.err) {
        break;
      }
    }

    tmpfs_deinit(&env.fs);
  }
}

UTEST_F(exec, miss_executes_action) {
  run_exec_test(&ur, (exec_test_t) {
    .name = "exec_miss",
    .action = { .identity = "I", .inputs = { "A" }, .outputs = { "O" }, .write = { "V" } },
    .ops = {
      { .kind = EXEC_OP_RUN, .expect = { .runs = 1, .contents = { "V1" } } },
    }
  });
}

UTEST_F(exec, hit_restores_deleted_output) {
  run_exec_test(&ur, (exec_test_t) {
    .name = "exec_hit",
    .action = { .identity = "I", .inputs = { "A" }, .outputs = { "O" }, .write = { "V" } },
    .ops = {
      { .kind = EXEC_OP_RUN, .expect = { .runs = 1 } },
      { .kind = EXEC_OP_REMOVE_OUTPUTS },
      { .kind = EXEC_OP_RUN, .expect = { .runs = 1, .contents = { "V1" } } },
    }
  });
}

UTEST_F(exec, input_change_reruns) {
  run_exec_test(&ur, (exec_test_t) {
    .name = "exec_input_change",
    .action = { .identity = "I", .inputs = { "A" }, .outputs = { "O" }, .write = { "V" } },
    .ops = {
      { .kind = EXEC_OP_RUN, .expect = { .runs = 1 } },
      { .kind = EXEC_OP_RUN, .change = { .inputs = { "B" } }, .expect = { .runs = 2, .contents = { "V2" } } },
    }
  });
}

UTEST_F(exec, identity_change_reruns) {
  run_exec_test(&ur, (exec_test_t) {
    .name = "exec_identity_change",
    .action = { .identity = "I", .inputs = { "A" }, .outputs = { "O" }, .write = { "V" } },
    .ops = {
      { .kind = EXEC_OP_RUN, .expect = { .runs = 1 } },
      { .kind = EXEC_OP_RUN, .change = { .identity = "J" }, .expect = { .runs = 2, .contents = { "V2" } } },
    }
  });
}

UTEST_F(exec, output_path_change_reruns) {
  run_exec_test(&ur, (exec_test_t) {
    .name = "exec_output_change",
    .action = { .identity = "I", .inputs = { "A" }, .outputs = { "O" }, .write = { "V" } },
    .ops = {
      { .kind = EXEC_OP_RUN, .expect = { .runs = 1 } },
      { .kind = EXEC_OP_RUN, .change = { .outputs = { "P" } }, .expect = { .runs = 2, .contents = { "V2" } } },
    }
  });
}

UTEST_F(exec, reverted_input_hits_prior_entry) {
  run_exec_test(&ur, (exec_test_t) {
    .name = "exec_revert",
    .action = { .identity = "I", .inputs = { "A" }, .outputs = { "O" }, .write = { "V" } },
    .ops = {
      { .kind = EXEC_OP_RUN, .expect = { .runs = 1 } },
      { .kind = EXEC_OP_RUN, .change = { .inputs = { "B" } }, .expect = { .runs = 2 } },
      { .kind = EXEC_OP_RUN, .change = { .inputs = { "A" } }, .expect = { .runs = 2, .contents = { "V1" } } },
    }
  });
}

UTEST_F(exec, multiple_outputs_restored) {
  run_exec_test(&ur, (exec_test_t) {
    .name = "exec_multi_output",
    .action = { .identity = "I", .inputs = { "A" }, .outputs = { "O", "P" }, .write = { "V", "W" } },
    .ops = {
      { .kind = EXEC_OP_RUN, .expect = { .runs = 1 } },
      { .kind = EXEC_OP_REMOVE_OUTPUTS },
      { .kind = EXEC_OP_RUN, .expect = { .runs = 1, .contents = { "V1", "W1" } } },
    }
  });
}

UTEST_F(exec, failed_action_not_cached) {
  run_exec_test(&ur, (exec_test_t) {
    .name = "exec_fail",
    .action = { .identity = "I", .inputs = { "A" }, .outputs = { "O" }, .write = { "V" } },
    .ops = {
      { .kind = EXEC_OP_RUN, .behavior = EXEC_BEHAVIOR_FAIL, .expect = { .err = SPN_ERROR } },
      { .kind = EXEC_OP_RUN, .expect = { .runs = 1, .contents = { "V1" } } },
    }
  });
}

UTEST_F(exec, missing_output_not_cached) {
  run_exec_test(&ur, (exec_test_t) {
    .name = "exec_missing_output",
    .action = { .identity = "I", .inputs = { "A" }, .outputs = { "O", "P" }, .write = { "V", "W" } },
    .ops = {
      { .kind = EXEC_OP_RUN, .behavior = EXEC_BEHAVIOR_SKIP_LAST_OUTPUT, .expect = { .err = SPN_ERROR, .runs = 1 } },
      { .kind = EXEC_OP_RUN, .expect = { .runs = 2, .contents = { "V2", "W2" } } },
    }
  });
}

UTEST_F(exec, unavailable_cached_output_reruns_then_hits) {
  run_exec_test(&ur, (exec_test_t) {
    .name = "exec_unavailable_output",
    .action = { .identity = "I", .inputs = { "A" }, .outputs = { "O" }, .write = { "V" } },
    .ops = {
      { .kind = EXEC_OP_RUN, .unavailable = { "U" }, .expect = { .runs = 1, .contents = { "V1" } } },
      { .kind = EXEC_OP_RUN, .expect = { .runs = 1, .contents = { "V1" } } },
    }
  });
}
