#include "common.h"

typedef struct {
  const c8* path;
  const c8* content;
} header_t;

typedef struct {
  header_t headers [DAG_TEST_MAX_INPUTS];
  const c8* probes [DAG_TEST_MAX_INPUTS];
  const c8* missing [DAG_TEST_MAX_INPUTS];
  const c8* removed [DAG_TEST_MAX_INPUTS];
  const c8* created [DAG_TEST_MAX_INPUTS];
  bool discover_fails;
  bool cold;
  const c8* output;
  const c8* manifest_fresh;
  spn_err_t expect_err;
  u32 expect_runs;
} run_t;

typedef struct {
  const c8* name;
  const c8* input;
  run_t runs [DAG_TEST_MAX_OPS];
} test_t;

typedef struct {
  tmpfs_t fs;
  spn_dag_t* g;
  spn_dag_store_t store;
  spn_dag_file_cache_t files;
  spn_dag_action_cache_t cache;
  spn_dag_discovery_t discovery;
  run_t* run;
  u32 runs;
} env_t;

UTEST_EMPTY_FIXTURE(discover_exec)

static s32 on_exec(spn_dag_action_t* action, void* user_data) {
  env_t* env = (env_t*)user_data;
  env->runs++;
  spn_dag_artifact_t* out = spn_dag_find_artifact(env->g, action->produces[0]);
  sp_str_t content = sp_fmt(env->fs.mem, "obj-{}", sp_fmt_uint(env->runs)).value;
  return sp_fs_create_file_str(out->path, content) ? 1 : 0;
}

static spn_err_t on_discover(spn_dag_action_t* action, void* user_data, sp_mem_t mem, sp_da(spn_dag_obs_t)* out) {
  env_t* env = (env_t*)user_data;
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
  sp_carr_for(env->run->missing, it) {
    if (!env->run->missing[it]) {
      break;
    }
    sp_da_push(*out, ((spn_dag_obs_t) {
      .kind = SPN_DAG_OBS_FILE,
      .path = tmpfs_get(&env->fs, sp_str_view(env->run->missing[it]))
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

static void prepare(env_t* env, run_t* run) {
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
    tmpfs_create(&env->fs, sp_str_view(run->created[it]), sp_str_lit("S"));
  }
}

static sp_str_t manifest_read(env_t* env) {
  sp_str_t dir = tmpfs_get(&env->fs, sp_str_lit("manifests"));
  sp_da(sp_fs_entry_t) entries = sp_fs_collect(env->fs.mem, dir);
  if (sp_da_size(entries) != 1) {
    return sp_str_lit("");
  }
  sp_str_t content = sp_zero;
  sp_io_read_file(env->fs.mem, entries[0].path, &content);
  return content;
}

static void run_test(s32* utest_result, test_t t) {
  env_t env = sp_zero;
  tmpfs_init_named(&env.fs, t.name);
  spn_dag_store_init(&env.store, (spn_dag_store_config_t) {
    .kind = SPN_DAG_STORE_MEM,
    .mem = env.fs.mem
  });
  spn_dag_file_cache_init(&env.files, env.fs.mem);
  spn_dag_action_cache_init(&env.cache, env.fs.mem, sp_str_lit(""));
  spn_dag_discovery_init(&env.discovery, env.fs.mem, tmpfs_get(&env.fs, sp_str_lit("manifests")));

  sp_carr_for(t.runs, r) {
    run_t* run = &t.runs[r];
    if (!run->expect_runs) {
      break;
    }

    env.run = run;
    if (run->cold) {
      spn_dag_file_cache_init(&env.files, env.fs.mem);
      spn_dag_discovery_init(&env.discovery, env.fs.mem, tmpfs_get(&env.fs, sp_str_lit("manifests")));
    }
    spn_dag_file_cache_refresh(&env.files);
    prepare(&env, run);

    spn_dag_t* g = spn_dag_new(env.fs.mem);
    env.g = g;
    spn_dag_id_t action = spn_dag_add_action(g, (spn_dag_action_config_t) {
      .identity = spn_dag_digest(t.input, sp_cstr_len(t.input)),
      .execute = on_exec,
      .discover = on_discover,
      .user_data = &env
    });
    spn_dag_action_add_input(g, action, spn_dag_add_value(g, t.input, sp_cstr_len(t.input)));
    spn_dag_id_t obj = spn_dag_add_file(g, tmpfs_get(&env.fs, sp_str_lit("O")));
    ASSERT_EQ(SPN_OK, spn_dag_action_add_output(g, action, obj));

    spn_err_t err = spn_dag_execute_discovered(g, action, &env.files, &env.cache, &env.store, &env.discovery);

    EXPECT_EQ(run->expect_err, err);
    EXPECT_EQ(run->expect_runs, env.runs);

    if (run->output) {
      sp_str_t from_disk = sp_zero;
      ASSERT_EQ(SP_OK, sp_io_read_file(env.fs.mem, tmpfs_get(&env.fs, sp_str_lit("O")), &from_disk));
      EXPECT_STR(from_disk, run->output);
    }

    if (run->manifest_fresh) {
      sp_str_t manifest = manifest_read(&env);
      EXPECT_TRUE(!sp_str_empty(manifest));
      sp_sys_file_meta_t sys = sp_zero;
      ASSERT_EQ(SPN_OK, spn_dag_get_file_meta(&env.files, tmpfs_get(&env.fs, sp_str_view(run->manifest_fresh)), &sys));
      sp_str_t mtime = sp_fmt(env.fs.mem, "\"mtime_ns\":\"{}\"", sp_fmt_int(sys.mtime.tv_nsec)).value;
      EXPECT_TRUE(sp_str_contains(manifest, mtime));
    }
  }

  tmpfs_deinit(&env.fs);
}

UTEST_F(discover_exec, unchanged_header_hits) {
  run_test(&ur, (test_t) {
    .name = "discover_unchanged",
    .input = "A",
    .runs = {
      { .headers = { { "H", "A" } }, .expect_runs = 1 },
      { .headers = { { "H", "A" } }, .expect_runs = 1 },
    }
  });
}

UTEST_F(discover_exec, changed_header_reruns) {
  run_test(&ur, (test_t) {
    .name = "discover_changed",
    .input = "A",
    .runs = {
      { .headers = { { "H", "A" } }, .expect_runs = 1 },
      { .headers = { { "H", "B" } }, .expect_runs = 2 },
    }
  });
}

UTEST_F(discover_exec, removed_header_reruns) {
  run_test(&ur, (test_t) {
    .name = "discover_removed",
    .input = "A",
    .runs = {
      { .headers = { { "H", "A" } }, .expect_runs = 1 },
      { .removed = { "H" }, .expect_runs = 2 },
    }
  });
}

UTEST_F(discover_exec, changed_set_refreshes_pathset) {
  run_test(&ur, (test_t) {
    .name = "discover_refresh",
    .input = "A",
    .runs = {
      { .headers = { { "H", "A" } }, .expect_runs = 1 },
      { .headers = { { "H", "B" }, { "I", "B" } }, .expect_runs = 2 },
      { .headers = { { "H", "B" }, { "I", "B" } }, .expect_runs = 2 },
    }
  });
}

UTEST_F(discover_exec, no_discovered_inputs_hits) {
  run_test(&ur, (test_t) {
    .name = "discover_none",
    .input = "A",
    .runs = {
      { .expect_runs = 1 },
      { .expect_runs = 1 },
    }
  });
}

UTEST_F(discover_exec, discover_failure_not_cached) {
  run_test(&ur, (test_t) {
    .name = "discover_fail",
    .input = "A",
    .runs = {
      { .discover_fails = true, .expect_err = SPN_ERROR, .expect_runs = 1 },
      { .headers = { { "H", "A" } }, .expect_runs = 2 },
    }
  });
}

UTEST_F(discover_exec, shadowing_header_reruns) {
  run_test(&ur, (test_t) {
    .name = "discover_shadow",
    .input = "A",
    .runs = {
      { .headers = { { "X/H", "A" } }, .probes = { "Y/H" }, .expect_runs = 1 },
      { .headers = { { "X/H", "A" } }, .probes = { "Y/H" }, .expect_runs = 1 },
      { .headers = { { "X/H", "A" } }, .probes = { "Y/H" }, .created = { "Y/H" }, .expect_runs = 2 },
    }
  });
}

UTEST_F(discover_exec, probe_still_absent_hits) {
  run_test(&ur, (test_t) {
    .name = "discover_probe_absent",
    .input = "A",
    .runs = {
      { .headers = { { "X/H", "A" } }, .probes = { "Y/H" }, .expect_runs = 1 },
      { .headers = { { "X/H", "A" } }, .probes = { "Y/H" }, .expect_runs = 1 },
    }
  });
}

UTEST_F(discover_exec, reverted_header_hits_prior_entry) {
  run_test(&ur, (test_t) {
    .name = "discover_revert",
    .input = "A",
    .runs = {
      { .headers = { { "H", "A" } }, .expect_runs = 1 },
      { .headers = { { "H", "B" } }, .expect_runs = 2 },
      { .headers = { { "H", "A" } }, .expect_runs = 2 },
    }
  });
}

UTEST_F(discover_exec, discover_order_canonicalized) {
  run_test(&ur, (test_t) {
    .name = "discover_order",
    .input = "A",
    .runs = {
      { .headers = { { "I", "B" }, { "H", "A" } }, .expect_runs = 1 },
      { .headers = { { "H", "C" }, { "I", "B" } }, .expect_runs = 2 },
      { .headers = { { "H", "A" }, { "I", "B" } }, .expect_runs = 2 },
    }
  });
}

UTEST_F(discover_exec, missing_discovered_input_reruns_until_present) {
  run_test(&ur, (test_t) {
    .name = "discover_missing",
    .input = "A",
    .runs = {
      { .missing = { "H" }, .expect_runs = 1 },
      { .missing = { "H" }, .expect_runs = 2 },
      { .headers = { { "H", "A" } }, .expect_runs = 3 },
      { .headers = { { "H", "A" } }, .expect_runs = 3 },
    }
  });
}

UTEST_F(discover_exec, hit_restores_deleted_output) {
  run_test(&ur, (test_t) {
    .name = "discover_restore",
    .input = "A",
    .runs = {
      { .headers = { { "H", "A" } }, .expect_runs = 1 },
      { .removed = { "O" }, .expect_runs = 1, .output = "obj-1" },
    }
  });
}

UTEST_F(discover_exec, manifest_reloaded_across_cold_start) {
  run_test(&ur, (test_t) {
    .name = "discover_manifest_reload",
    .input = "A",
    .runs = {
      { .headers = { { "H", "A" } }, .expect_runs = 1 },
      { .headers = { { "H", "A" } }, .cold = true, .expect_runs = 1 },
    }
  });
}

UTEST_F(discover_exec, manifest_rewritten_on_hit) {
  run_test(&ur, (test_t) {
    .name = "discover_manifest_rewrite",
    .input = "A",
    .runs = {
      { .headers = { { "H", "A" } }, .expect_runs = 1 },
      { .headers = { { "H", "A" } }, .cold = true, .expect_runs = 1 },
      { .headers = { { "H", "A" } }, .cold = true, .expect_runs = 1, .manifest_fresh = "H" },
    }
  });
}
