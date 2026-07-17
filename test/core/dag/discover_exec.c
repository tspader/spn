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
  bool manifest_stable;
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
  dag_test_env_t dag;
  run_t* run;
} env_t;

UTEST_EMPTY_FIXTURE(discover_exec)

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
      .path = tmpfs_get(&env->dag.fs, sp_str_view(env->run->headers[it].path))
    }));
  }
  sp_carr_for(env->run->missing, it) {
    if (!env->run->missing[it]) {
      break;
    }
    sp_da_push(*out, ((spn_dag_obs_t) {
      .kind = SPN_DAG_OBS_FILE,
      .path = tmpfs_get(&env->dag.fs, sp_str_view(env->run->missing[it]))
    }));
  }
  sp_carr_for(env->run->probes, it) {
    if (!env->run->probes[it]) {
      break;
    }
    sp_da_push(*out, ((spn_dag_obs_t) {
      .kind = SPN_DAG_OBS_ABSENT,
      .path = tmpfs_get(&env->dag.fs, sp_str_view(env->run->probes[it]))
    }));
  }
  return SPN_OK;
}

static void prepare(env_t* env, run_t* run) {
  sp_carr_for(run->headers, it) {
    if (!run->headers[it].path) {
      break;
    }
    tmpfs_create(&env->dag.fs, sp_str_view(run->headers[it].path), sp_str_view(run->headers[it].content));
  }
  sp_carr_for(run->removed, it) {
    if (!run->removed[it]) {
      break;
    }
    sp_fs_remove_file(tmpfs_get(&env->dag.fs, sp_str_view(run->removed[it])));
  }
  sp_carr_for(run->created, it) {
    if (!run->created[it]) {
      break;
    }
    tmpfs_create(&env->dag.fs, sp_str_view(run->created[it]), sp_str_lit("S"));
  }
}

static sp_sys_file_meta_t manifest_meta(env_t* env) {
  sp_sys_file_meta_t meta = sp_zero;
  sp_str_t dir = tmpfs_get(&env->dag.fs, sp_str_lit("manifests"));
  sp_da(sp_fs_entry_t) entries = sp_fs_collect(env->dag.fs.mem, dir);
  if (sp_da_size(entries) == 1) {
    sp_sys_get_path_metadata_s(sp_sys_get_root(0), entries[0].path, &meta);
  }
  return meta;
}

static sp_str_t manifest_read(env_t* env) {
  sp_str_t dir = tmpfs_get(&env->dag.fs, sp_str_lit("manifests"));
  sp_da(sp_fs_entry_t) entries = sp_fs_collect(env->dag.fs.mem, dir);
  if (sp_da_size(entries) != 1) {
    return sp_str_lit("");
  }
  sp_str_t content = sp_zero;
  sp_io_read_file(env->dag.fs.mem, entries[0].path, &content);
  return content;
}

static void run_test(s32* utest_result, test_t t) {
  env_t env = sp_zero;
  dag_test_env_init(&env.dag, (dag_test_env_config_t) {
    .name = t.name,
    .store = SPN_DAG_STORE_MEM,
    .discovery = true
  });

  sp_carr_for(t.runs, r) {
    run_t* run = &t.runs[r];
    if (!run->expect_runs) {
      break;
    }

    env.run = run;
    if (run->cold) {
      dag_test_env_cold(&env.dag);
    }
    spn_dag_file_cache_invalidate_all(&env.dag.files);
    prepare(&env, run);

    spn_dag_t* g = dag_test_env_graph(&env.dag);
    spn_dag_id_t action = spn_dag_add_action(g, (spn_dag_action_config_t) {
      .identity = dag_test_digest(t.input),
      .execute = dag_test_exec_stamp,
      .discover = on_discover,
      .user_data = &env
    });
    spn_dag_action_add_input(g, action, spn_dag_add_value(g, t.input, sp_cstr_len(t.input)));
    spn_dag_id_t obj = spn_dag_add_file(g, tmpfs_get(&env.dag.fs, sp_str_lit("O")));
    ASSERT_EQ(SPN_OK, spn_dag_action_add_output(g, action, obj));

    sp_sys_file_meta_t before = sp_zero;
    if (run->manifest_stable) {
      before = manifest_meta(&env);
    }

    spn_err_t err = spn_dag_execute_discovered(g, action, &env.dag.env);

    if (run->manifest_stable) {
      sp_sys_file_meta_t after = manifest_meta(&env);
      EXPECT_EQ(before.device, after.device);
      EXPECT_EQ(before.id, after.id);
      EXPECT_EQ(before.mtime.tv_sec, after.mtime.tv_sec);
      EXPECT_EQ(before.mtime.tv_nsec, after.mtime.tv_nsec);
    }

    EXPECT_EQ(run->expect_err, err);
    EXPECT_EQ(run->expect_runs, env.dag.runs);

    if (run->output) {
      dag_test_expect_file(utest_result, env.dag.fs.mem, tmpfs_get(&env.dag.fs, sp_str_lit("O")), run->output);
    }

    if (run->manifest_fresh) {
      sp_str_t manifest = manifest_read(&env);
      EXPECT_TRUE(!sp_str_empty(manifest));
      sp_sys_file_meta_t sys = sp_zero;
      ASSERT_EQ(SPN_OK, spn_dag_file_cache_stat(&env.dag.files, tmpfs_get(&env.dag.fs, sp_str_view(run->manifest_fresh)), &sys));
      sp_str_t mtime = sp_fmt(env.dag.fs.mem, "\"mtime_ns\":\"{}\"", sp_fmt_int(sys.mtime.tv_nsec)).value;
      EXPECT_TRUE(sp_str_contains(manifest, mtime));
    }
  }

  dag_test_env_deinit(&env.dag);
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
      { .missing = { "H" }, .expect_runs = 1, .output = "1" },
      { .missing = { "H" }, .expect_runs = 2, .output = "2" },
      { .headers = { { "H", "A" } }, .expect_runs = 3, .output = "3" },
      { .headers = { { "H", "A" } }, .expect_runs = 3, .output = "3" },
    }
  });
}

UTEST_F(discover_exec, hit_restores_deleted_output) {
  run_test(&ur, (test_t) {
    .name = "discover_restore",
    .input = "A",
    .runs = {
      { .headers = { { "H", "A" } }, .expect_runs = 1 },
      { .removed = { "O" }, .expect_runs = 1, .output = "1" },
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

UTEST_F(discover_exec, manifest_flush_skipped_when_unchanged) {
  run_test(&ur, (test_t) {
    .name = "discover_manifest_skip",
    .input = "A",
    .runs = {
      { .headers = { { "H", "A" } }, .expect_runs = 1 },
      { .expect_runs = 1, .manifest_stable = true },
      { .cold = true, .expect_runs = 1, .manifest_stable = true },
    }
  });
}
