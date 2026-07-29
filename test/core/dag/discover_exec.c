#include "dag_test.h"

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
} discover_run_t;

typedef struct {
  const c8* name;
  const c8* input;
  discover_run_t runs [DAG_TEST_MAX_OPS];
} discover_test_t;

typedef struct {
  dag_test_env_t dag;
  const discover_run_t* run;
} discover_env_t;

static const discover_test_t discover_tests [] = {
  {
    .name = "unchanged_header_hits",
    .input = "A",
    .runs = {
      { .headers = { { "H", "A" } }, .expect_runs = 1 },
      { .headers = { { "H", "A" } }, .expect_runs = 1 },
    }
  },
  {
    .name = "changed_header_reruns",
    .input = "A",
    .runs = {
      { .headers = { { "H", "A" } }, .expect_runs = 1 },
      { .headers = { { "H", "B" } }, .expect_runs = 2 },
    }
  },
  {
    .name = "removed_header_reruns",
    .input = "A",
    .runs = {
      { .headers = { { "H", "A" } }, .expect_runs = 1 },
      { .removed = { "H" }, .expect_runs = 2 },
    }
  },
  {
    .name = "changed_set_refreshes_pathset",
    .input = "A",
    .runs = {
      { .headers = { { "H", "A" } }, .expect_runs = 1 },
      { .headers = { { "H", "B" }, { "I", "B" } }, .expect_runs = 2 },
      { .headers = { { "H", "B" }, { "I", "B" } }, .expect_runs = 2 },
    }
  },
  {
    .name = "no_discovered_inputs_hits",
    .input = "A",
    .runs = {
      { .expect_runs = 1 },
      { .expect_runs = 1 },
    }
  },
  {
    .name = "discover_failure_not_cached",
    .input = "A",
    .runs = {
      { .discover_fails = true, .expect_err = SPN_ERROR, .expect_runs = 1 },
      { .headers = { { "H", "A" } }, .expect_runs = 2 },
    }
  },
  {
    .name = "shadowing_header_reruns",
    .input = "A",
    .runs = {
      { .headers = { { "X/H", "A" } }, .probes = { "Y/H" }, .expect_runs = 1 },
      { .headers = { { "X/H", "A" } }, .probes = { "Y/H" }, .expect_runs = 1 },
      { .headers = { { "X/H", "A" } }, .probes = { "Y/H" }, .created = { "Y/H" }, .expect_runs = 2 },
    }
  },
  {
    .name = "probe_still_absent_hits",
    .input = "A",
    .runs = {
      { .headers = { { "X/H", "A" } }, .probes = { "Y/H" }, .expect_runs = 1 },
      { .headers = { { "X/H", "A" } }, .probes = { "Y/H" }, .expect_runs = 1 },
    }
  },
  {
    .name = "reverted_header_hits_prior_entry",
    .input = "A",
    .runs = {
      { .headers = { { "H", "A" } }, .expect_runs = 1 },
      { .headers = { { "H", "B" } }, .expect_runs = 2 },
      { .headers = { { "H", "A" } }, .expect_runs = 2 },
    }
  },
  {
    .name = "discover_order_canonicalized",
    .input = "A",
    .runs = {
      { .headers = { { "I", "B" }, { "H", "A" } }, .expect_runs = 1 },
      { .headers = { { "H", "C" }, { "I", "B" } }, .expect_runs = 2 },
      { .headers = { { "H", "A" }, { "I", "B" } }, .expect_runs = 2 },
    }
  },
  {
    .name = "missing_discovered_input_reruns_until_present",
    .input = "A",
    .runs = {
      { .missing = { "H" }, .expect_runs = 1, .output = "1" },
      { .missing = { "H" }, .expect_runs = 2, .output = "2" },
      { .headers = { { "H", "A" } }, .expect_runs = 3, .output = "3" },
      { .headers = { { "H", "A" } }, .expect_runs = 3, .output = "3" },
    }
  },
  {
    .name = "hit_restores_deleted_output",
    .input = "A",
    .runs = {
      { .headers = { { "H", "A" } }, .expect_runs = 1 },
      { .removed = { "O" }, .expect_runs = 1, .output = "1" },
    }
  },
  {
    .name = "manifest_reloaded_across_cold_start",
    .input = "A",
    .runs = {
      { .headers = { { "H", "A" } }, .expect_runs = 1 },
      { .headers = { { "H", "A" } }, .cold = true, .expect_runs = 1 },
    }
  },
  {
    .name = "manifest_rewritten_on_hit",
    .input = "A",
    .runs = {
      { .headers = { { "H", "A" } }, .expect_runs = 1 },
      { .headers = { { "H", "A" } }, .cold = true, .expect_runs = 1 },
      { .headers = { { "H", "A" } }, .cold = true, .expect_runs = 1, .manifest_fresh = "H" },
    }
  },
  {
    .name = "manifest_flush_skipped_when_unchanged",
    .input = "A",
    .runs = {
      { .headers = { { "H", "A" } }, .expect_runs = 1 },
      { .expect_runs = 1, .manifest_stable = true },
      { .cold = true, .expect_runs = 1, .manifest_stable = true },
    }
  },
};

static spn_err_t discover_exec_on_discover(spn_dag_t* g, spn_dag_action_t* action, void* user_data, sp_mem_t mem, sp_da(spn_dag_obs_t)* out) {
  discover_env_t* env = (discover_env_t*)user_data;
  if (env->run->discover_fails) {
    return SPN_ERROR;
  }
  sp_carr_for(env->run->headers, it) {
    if (!env->run->headers[it].path) {
      break;
    }
    sp_da_push(*out, ((spn_dag_obs_t) {
      .kind = SPN_DAG_OBS_FILE,
      .path = dag_test_env_path(&env->dag, sp_str_view(env->run->headers[it].path))
    }));
  }
  sp_carr_for(env->run->missing, it) {
    if (!env->run->missing[it]) {
      break;
    }
    sp_da_push(*out, ((spn_dag_obs_t) {
      .kind = SPN_DAG_OBS_FILE,
      .path = dag_test_env_path(&env->dag, sp_str_view(env->run->missing[it]))
    }));
  }
  sp_carr_for(env->run->probes, it) {
    if (!env->run->probes[it]) {
      break;
    }
    sp_da_push(*out, ((spn_dag_obs_t) {
      .kind = SPN_DAG_OBS_ABSENT,
      .path = dag_test_env_path(&env->dag, sp_str_view(env->run->probes[it]))
    }));
  }
  return SPN_OK;
}

static void discover_exec_prepare(discover_env_t* env, const discover_run_t* run) {
  sp_carr_for(run->headers, it) {
    if (!run->headers[it].path) {
      break;
    }
    dag_test_env_create(&env->dag, sp_str_view(run->headers[it].path), sp_str_view(run->headers[it].content));
  }
  sp_carr_for(run->removed, it) {
    if (!run->removed[it]) {
      break;
    }
    sp_fs_remove_file(dag_test_env_path(&env->dag, sp_str_view(run->removed[it])));
  }
  sp_carr_for(run->created, it) {
    if (!run->created[it]) {
      break;
    }
    dag_test_env_create(&env->dag, sp_str_view(run->created[it]), sp_str_lit("S"));
  }
}

static sp_sys_file_meta_t manifest_meta(discover_env_t* env) {
  sp_sys_file_meta_t meta = sp_zero;
  sp_str_t dir = dag_test_env_path(&env->dag, sp_str_lit("manifests"));
  sp_da(sp_fs_entry_t) entries = sp_fs_collect(env->dag.mem, dir);
  if (sp_da_size(entries) == 1) {
    sp_sys_get_path_metadata_s(sp_sys_get_root(0), entries[0].path, &meta);
  }
  return meta;
}

static sp_str_t manifest_read(discover_env_t* env) {
  sp_str_t dir = dag_test_env_path(&env->dag, sp_str_lit("manifests"));
  sp_da(sp_fs_entry_t) entries = sp_fs_collect(env->dag.mem, dir);
  if (sp_da_size(entries) != 1) {
    return sp_str_lit("");
  }
  sp_str_t content = sp_zero;
  sp_io_read_file(env->dag.mem, entries[0].path, &content);
  return content;
}

sp_test_each(dag_discover_exec, runs, discover_test_t, discover_tests) {
  discover_env_t env = sp_zero;
  dag_test_env_init(&env.dag, t, (dag_test_env_config_t) {
    .store = SPN_DAG_STORE_MEM,
    .discovery = true
  });

  sp_carr_for(it->runs, r) {
    const discover_run_t* run = &it->runs[r];
    if (!run->expect_runs) {
      break;
    }

    env.run = run;
    if (run->cold) {
      dag_test_env_cold(&env.dag);
    }
    spn_dag_file_cache_invalidate_all(&env.dag.files);
    discover_exec_prepare(&env, run);

    spn_dag_t* g = dag_test_env_graph(&env.dag);
    spn_dag_id_t action = spn_dag_add_action(g, (spn_dag_action_config_t) {
      .identity = dag_test_digest(it->input),
      .execute = dag_test_exec_stamp,
      .discover = discover_exec_on_discover,
      .user_data = &env
    });
    spn_dag_action_add_input(g, action, spn_dag_add_value(g, it->input, sp_cstr_len(it->input)));
    spn_dag_id_t obj = spn_dag_add_file(g, dag_test_env_path(&env.dag, sp_str_lit("O")));
    sp_must_eq(t, SPN_OK, spn_dag_action_add_output(g, action, obj));

    sp_sys_file_meta_t before = sp_zero;
    if (run->manifest_stable) {
      before = manifest_meta(&env);
    }

    spn_err_t err = spn_dag_execute_discovered(g, action, &env.dag.env);

    if (run->manifest_stable) {
      sp_sys_file_meta_t after = manifest_meta(&env);
      sp_expect_eq(t, before.device, after.device);
      sp_expect_eq(t, before.id, after.id);
      sp_expect_eq(t, before.mtime.tv_sec, after.mtime.tv_sec);
      sp_expect_eq(t, before.mtime.tv_nsec, after.mtime.tv_nsec);
    }

    sp_expect_eq(t, run->expect_err, err);
    sp_expect_eq(t, run->expect_runs, env.dag.runs);

    if (run->output) {
      sp_err_t file_err = dag_test_expect_file(t, env.dag.mem, dag_test_env_path(&env.dag, sp_str_lit("O")), run->output);
      if (file_err) {
        return file_err;
      }
    }

    if (run->manifest_fresh) {
      sp_str_t manifest = manifest_read(&env);
      sp_expect(t, !sp_str_empty(manifest));
      sp_sys_file_meta_t sys = sp_zero;
      sp_must_eq(t, SPN_OK, spn_dag_file_cache_stat(&env.dag.files, dag_test_env_path(&env.dag, sp_str_view(run->manifest_fresh)), &sys));
      sp_str_t mtime = sp_fmt(env.dag.mem, " {} {} ", sp_fmt_int((s64)sys.mtime.tv_sec), sp_fmt_int((s64)sys.mtime.tv_nsec)).value;
      sp_expect(t, sp_str_contains(manifest, mtime));
    }
  }

  return SP_OK;
}
