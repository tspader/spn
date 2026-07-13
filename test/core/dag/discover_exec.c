typedef struct {
  const c8* path;
  const c8* content;
} disco_header_t;

typedef struct {
  disco_header_t headers [DAG_TEST_MAX_INPUTS];
  const c8* probes [DAG_TEST_MAX_INPUTS];
  const c8* removed [DAG_TEST_MAX_INPUTS];
  const c8* created [DAG_TEST_MAX_INPUTS];
  bool discover_fails;
  spn_err_t expect_err;
  u32 expect_runs;
} disco_run_t;

typedef struct {
  const c8* name;
  const c8* input;
  disco_run_t runs [DAG_TEST_MAX_OPS];
} disco_exec_test_t;

typedef struct {
  tmpfs_t fs;
  spn_dag_t* g;
  spn_dag_store_t store;
  spn_dag_file_cache_t files;
  spn_dag_action_cache_t cache;
  spn_dag_discovery_t discovery;
  disco_run_t* run;
  u32 runs;
} disco_exec_env_t;

UTEST_EMPTY_FIXTURE(discover_exec)

static s32 disco_exec_fn(spn_dag_action_t* action, void* user_data) {
  disco_exec_env_t* env = (disco_exec_env_t*)user_data;
  env->runs++;
  spn_dag_artifact_t* out = spn_dag_find_artifact(env->g, action->produces[0]);
  sp_str_t content = sp_fmt(env->fs.mem, "obj-{}", sp_fmt_uint(env->runs)).value;
  return sp_fs_create_file_str(out->path, content) ? 1 : 0;
}

static spn_err_t disco_discover_fn(spn_dag_action_t* action, void* user_data, sp_mem_t mem, sp_da(spn_dag_obs_t)* out) {
  disco_exec_env_t* env = (disco_exec_env_t*)user_data;
  if (env->run->discover_fails) {
    return SPN_ERROR;
  }
  sp_carr_for(env->run->headers, it) {
    if (!env->run->headers[it].path) {
      break;
    }
    sp_da_push(*out, ((spn_dag_obs_t) {
      .kind = SPN_DAG_OBS_FILE,
      .path = tmpfs_get(&env->fs, sp_str_view(env->run->headers[it].path))
    }));
  }
  sp_carr_for(env->run->probes, it) {
    if (!env->run->probes[it]) {
      break;
    }
    sp_da_push(*out, ((spn_dag_obs_t) {
      .kind = SPN_DAG_OBS_ABSENT,
      .path = tmpfs_get(&env->fs, sp_str_view(env->run->probes[it]))
    }));
  }
  return SPN_OK;
}

static void disco_exec_prepare(disco_exec_env_t* env, disco_run_t* run) {
  sp_carr_for(run->headers, it) {
    if (!run->headers[it].path) {
      break;
    }
    tmpfs_create(&env->fs, sp_str_view(run->headers[it].path), sp_str_view(run->headers[it].content));
  }
  sp_carr_for(run->removed, it) {
    if (!run->removed[it]) {
      break;
    }
    sp_fs_remove_file(tmpfs_get(&env->fs, sp_str_view(run->removed[it])));
  }
  sp_carr_for(run->created, it) {
    if (!run->created[it]) {
      break;
    }
    tmpfs_create(&env->fs, sp_str_view(run->created[it]), sp_str_lit("SHADOW"));
  }
}

static void run_disco_exec_test(s32* utest_result, disco_exec_test_t t) {
  disco_exec_env_t env = sp_zero;
  tmpfs_init_named(&env.fs, t.name);
  spn_dag_store_init(&env.store, (spn_dag_store_config_t) {
    .kind = SPN_DAG_STORE_MEM,
    .mem = env.fs.mem
  });
  spn_dag_file_cache_init(&env.files, env.fs.mem);
  spn_dag_action_cache_init(&env.cache, env.fs.mem);
  spn_dag_discovery_init(&env.discovery, env.fs.mem);

  sp_carr_for(t.runs, r) {
    disco_run_t* run = &t.runs[r];
    if (!run->expect_runs) {
      break;
    }

    env.run = run;
    spn_dag_file_cache_refresh(&env.files);
    disco_exec_prepare(&env, run);

    spn_dag_t* g = spn_dag_new(env.fs.mem);
    env.g = g;
    spn_dag_id_t action = spn_dag_add_action(g, (spn_dag_action_config_t) {
      .identity = spn_dag_digest(t.input, sp_cstr_len(t.input)),
      .execute = disco_exec_fn,
      .discover = disco_discover_fn,
      .user_data = &env
    });
    spn_dag_action_add_input(g, action, spn_dag_add_value(g, t.input, sp_cstr_len(t.input)));
    spn_dag_id_t obj = spn_dag_add_file(g, tmpfs_get(&env.fs, sp_str_lit("main.o")));
    ASSERT_EQ(SPN_OK, spn_dag_action_add_output(g, action, obj));

    spn_err_t err = spn_dag_execute_discovered(g, action, &env.files, &env.cache, &env.store, &env.discovery);

    EXPECT_EQ(run->expect_err, err);
    EXPECT_EQ(run->expect_runs, env.runs);
  }

  tmpfs_deinit(&env.fs);
}

UTEST_F(discover_exec, unchanged_header_hits) {
  run_disco_exec_test(&ur, (disco_exec_test_t) {
    .name = "discover_unchanged",
    .input = "int main() {}",
    .runs = {
      { .headers = { { "sp.h", "SP" } }, .expect_runs = 1 },
      { .headers = { { "sp.h", "SP" } }, .expect_runs = 1 },
    }
  });
}

UTEST_F(discover_exec, changed_header_reruns) {
  run_disco_exec_test(&ur, (disco_exec_test_t) {
    .name = "discover_changed",
    .input = "int main() {}",
    .runs = {
      { .headers = { { "sp.h", "SP version one" } }, .expect_runs = 1 },
      { .headers = { { "sp.h", "SP two" } }, .expect_runs = 2 },
    }
  });
}

UTEST_F(discover_exec, removed_header_reruns) {
  run_disco_exec_test(&ur, (disco_exec_test_t) {
    .name = "discover_removed",
    .input = "int main() {}",
    .runs = {
      { .headers = { { "sp.h", "SP" } }, .expect_runs = 1 },
      { .removed = { "sp.h" }, .expect_runs = 2 },
    }
  });
}

UTEST_F(discover_exec, changed_set_refreshes_pathset) {
  run_disco_exec_test(&ur, (disco_exec_test_t) {
    .name = "discover_refresh",
    .input = "int main() {}",
    .runs = {
      { .headers = { { "sp.h", "SP" } }, .expect_runs = 1 },
      { .headers = { { "sp.h", "SP2" }, { "io.h", "IO" } }, .expect_runs = 2 },
      { .headers = { { "sp.h", "SP2" }, { "io.h", "IO" } }, .expect_runs = 2 },
    }
  });
}

UTEST_F(discover_exec, no_discovered_inputs_hits) {
  run_disco_exec_test(&ur, (disco_exec_test_t) {
    .name = "discover_none",
    .input = "int main() {}",
    .runs = {
      { .expect_runs = 1 },
      { .expect_runs = 1 },
    }
  });
}

UTEST_F(discover_exec, discover_failure_not_cached) {
  run_disco_exec_test(&ur, (disco_exec_test_t) {
    .name = "discover_fail",
    .input = "int main() {}",
    .runs = {
      { .discover_fails = true, .expect_err = SPN_ERROR, .expect_runs = 1 },
      { .headers = { { "sp.h", "SP" } }, .expect_runs = 2 },
    }
  });
}

UTEST_F(discover_exec, shadowing_header_reruns) {
  run_disco_exec_test(&ur, (disco_exec_test_t) {
    .name = "discover_shadow",
    .input = "int main() {}",
    .runs = {
      { .headers = { { "inc2/sp.h", "SP" } }, .probes = { "inc1/sp.h" }, .expect_runs = 1 },
      { .headers = { { "inc2/sp.h", "SP" } }, .probes = { "inc1/sp.h" }, .expect_runs = 1 },
      { .headers = { { "inc2/sp.h", "SP" } }, .probes = { "inc1/sp.h" }, .created = { "inc1/sp.h" }, .expect_runs = 2 },
    }
  });
}

UTEST_F(discover_exec, probe_still_absent_hits) {
  run_disco_exec_test(&ur, (disco_exec_test_t) {
    .name = "discover_probe_absent",
    .input = "int main() {}",
    .runs = {
      { .headers = { { "inc2/sp.h", "SP" } }, .probes = { "inc1/sp.h" }, .expect_runs = 1 },
      { .headers = { { "inc2/sp.h", "SP" } }, .probes = { "inc1/sp.h" }, .expect_runs = 1 },
    }
  });
}
