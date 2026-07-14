#include "common.h"

typedef struct {
  const c8* dir;
  const c8* filter;
} enum_obs_t;

typedef struct {
  const c8* files [DAG_TEST_MAX_INPUTS];
  const c8* dirs [DAG_TEST_MAX_INPUTS];
  const c8* removed [DAG_TEST_MAX_INPUTS];
  bool cold;
  u32 expect_runs;
} enum_run_t;

typedef struct {
  const c8* name;
  enum_obs_t obs [DAG_TEST_MAX_INPUTS];
  enum_run_t runs [DAG_TEST_MAX_OPS];
} enum_test_t;

typedef struct {
  dag_test_env_t dag;
  enum_test_t* test;
} enum_env_t;

UTEST_EMPTY_FIXTURE(enumeration)

static spn_err_t enum_discover(spn_dag_action_t* action, void* user_data, sp_mem_t mem, sp_da(spn_dag_obs_t)* out) {
  enum_env_t* env = (enum_env_t*)user_data;
  sp_carr_for(env->test->obs, it) {
    if (!env->test->obs[it].dir) {
      break;
    }
    sp_da_push(*out, ((spn_dag_obs_t) {
      .kind = SPN_DAG_OBS_ENUMERATION,
      .path = tmpfs_get(&env->dag.fs, sp_str_view(env->test->obs[it].dir)),
      .filter = env->test->obs[it].filter ? sp_str_view(env->test->obs[it].filter) : sp_str_lit("")
    }));
  }
  return SPN_OK;
}

static void enum_prepare(enum_env_t* env, enum_run_t* run) {
  sp_carr_for(run->removed, it) {
    if (!run->removed[it]) {
      break;
    }
    sp_str_t path = tmpfs_get(&env->dag.fs, sp_str_view(run->removed[it]));
    if (sp_fs_is_dir(path)) {
      sp_fs_remove_dir(path);
    } else {
      sp_fs_remove_file(path);
    }
  }
  sp_carr_for(run->files, it) {
    if (!run->files[it]) {
      break;
    }
    tmpfs_create(&env->dag.fs, sp_str_view(run->files[it]), sp_str_lit("S"));
  }
  sp_carr_for(run->dirs, it) {
    if (!run->dirs[it]) {
      break;
    }
    sp_fs_create_dir(tmpfs_get(&env->dag.fs, sp_str_view(run->dirs[it])));
  }
}

static void run_test(s32* utest_result, enum_test_t t) {
  enum_env_t env = sp_zero;
  env.test = &t;
  dag_test_env_init(&env.dag, (dag_test_env_config_t) {
    .name = t.name,
    .store = SPN_DAG_STORE_MEM,
    .discovery = true
  });

  sp_carr_for(t.runs, r) {
    enum_run_t* run = &t.runs[r];
    if (!run->expect_runs) {
      break;
    }

    if (run->cold) {
      dag_test_env_cold(&env.dag);
    }
    spn_dag_file_cache_refresh(&env.dag.files);
    enum_prepare(&env, run);

    spn_dag_t* g = dag_test_env_graph(&env.dag);
    spn_dag_id_t action = spn_dag_add_action(g, (spn_dag_action_config_t) {
      .identity = dag_test_digest(t.name),
      .execute = dag_test_exec_stamp,
      .discover = enum_discover,
      .user_data = &env
    });
    ASSERT_EQ(SPN_OK, spn_dag_action_add_output(g, action, spn_dag_add_output(g, sp_str_lit("O"))));

    EXPECT_EQ(SPN_OK, spn_dag_execute_discovered(g, action, &env.dag.env));
    EXPECT_EQ(run->expect_runs, env.dag.runs);
  }

  dag_test_env_deinit(&env.dag);
}

UTEST_F(enumeration, unchanged_membership_hits) {
  run_test(&ur, (enum_test_t) {
    .name = "enum_unchanged",
    .obs = { { "A", "*.h" } },
    .runs = {
      { .files = { "A/X.h" }, .expect_runs = 1 },
      { .expect_runs = 1 },
    }
  });
}

UTEST_F(enumeration, new_matching_file_reruns) {
  run_test(&ur, (enum_test_t) {
    .name = "enum_new_match",
    .obs = { { "A", "*.h" } },
    .runs = {
      { .files = { "A/X.h" }, .expect_runs = 1 },
      { .files = { "A/Y.h" }, .expect_runs = 2 },
    }
  });
}

UTEST_F(enumeration, unmatched_file_hits) {
  run_test(&ur, (enum_test_t) {
    .name = "enum_unmatched",
    .obs = { { "A", "*.h" } },
    .runs = {
      { .files = { "A/X.h" }, .expect_runs = 1 },
      { .files = { "A/Y.c" }, .expect_runs = 1 },
    }
  });
}

UTEST_F(enumeration, new_subdir_reruns) {
  run_test(&ur, (enum_test_t) {
    .name = "enum_new_subdir",
    .obs = { { "A", "*.h" } },
    .runs = {
      { .files = { "A/X.h" }, .expect_runs = 1 },
      { .dirs = { "A/B" }, .expect_runs = 2 },
    }
  });
}

UTEST_F(enumeration, removed_member_reruns) {
  run_test(&ur, (enum_test_t) {
    .name = "enum_removed",
    .obs = { { "A", "*.h" } },
    .runs = {
      { .files = { "A/X.h", "A/Y.h" }, .expect_runs = 1 },
      { .removed = { "A/Y.h" }, .expect_runs = 2 },
    }
  });
}

UTEST_F(enumeration, empty_filter_matches_all) {
  run_test(&ur, (enum_test_t) {
    .name = "enum_empty_filter",
    .obs = { { "A" } },
    .runs = {
      { .files = { "A/X.h" }, .expect_runs = 1 },
      { .files = { "A/Y.c" }, .expect_runs = 2 },
    }
  });
}

UTEST_F(enumeration, missing_dir_equals_empty) {
  run_test(&ur, (enum_test_t) {
    .name = "enum_missing_dir",
    .obs = { { "A", "*.h" } },
    .runs = {
      { .expect_runs = 1 },
      { .dirs = { "A" }, .expect_runs = 1 },
    }
  });
}

UTEST_F(enumeration, member_type_change_reruns) {
  run_test(&ur, (enum_test_t) {
    .name = "enum_type_change",
    .obs = { { "A", "*.h" } },
    .runs = {
      { .files = { "A/X.h" }, .expect_runs = 1 },
      { .removed = { "A/X.h" }, .dirs = { "A/X.h" }, .expect_runs = 2 },
    }
  });
}

UTEST_F(enumeration, distinct_filters_coexist) {
  run_test(&ur, (enum_test_t) {
    .name = "enum_distinct_filters",
    .obs = { { "A", "*.h" }, { "A", "*.c" } },
    .runs = {
      { .files = { "A/X.h" }, .expect_runs = 1 },
      { .files = { "A/Y.c" }, .expect_runs = 2 },
      { .files = { "A/Z.t" }, .expect_runs = 2 },
      { .cold = true, .expect_runs = 2 },
      { .cold = true, .files = { "A/W.c" }, .expect_runs = 3 },
    }
  });
}

UTEST_F(enumeration, duplicate_obs_dedup) {
  run_test(&ur, (enum_test_t) {
    .name = "enum_duplicate",
    .obs = { { "A", "*.h" }, { "A", "*.h" } },
    .runs = {
      { .files = { "A/X.h" }, .expect_runs = 1 },
      { .expect_runs = 1 },
    }
  });
}

UTEST_F(enumeration, path_is_file_hashes_empty) {
  run_test(&ur, (enum_test_t) {
    .name = "enum_path_is_file",
    .obs = { { "A", "*.h" } },
    .runs = {
      { .files = { "A" }, .expect_runs = 1 },
      { .removed = { "A" }, .expect_runs = 1 },
    }
  });
}

UTEST_F(enumeration, manifest_reload_roundtrip) {
  run_test(&ur, (enum_test_t) {
    .name = "enum_reload",
    .obs = { { "A", "*.h" } },
    .runs = {
      { .files = { "A/X.h" }, .expect_runs = 1 },
      { .cold = true, .expect_runs = 1 },
      { .cold = true, .files = { "A/Y.h" }, .expect_runs = 2 },
    }
  });
}
