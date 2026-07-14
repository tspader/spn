#include "common.h"

typedef struct {
  const c8* dir;
  const c8* filter;
} glob_enum_t;

typedef struct {
  glob_enum_t enums [DAG_TEST_MAX_INPUTS];
  const c8* matches [DAG_TEST_MAX_INPUTS];
  const c8* absent [DAG_TEST_MAX_INPUTS];
  spn_err_t err;
} glob_expect_t;

typedef struct {
  const c8* name;
  const c8* files [DAG_TEST_MAX_INPUTS];
  const c8* dirs [DAG_TEST_MAX_INPUTS];
  const c8* pattern;
  glob_expect_t expect;
} glob_test_t;

UTEST_EMPTY_FIXTURE(glob)

static s32 glob_obs_order(const void* a, const void* b) {
  const spn_dag_obs_t* oa = (const spn_dag_obs_t*)a;
  const spn_dag_obs_t* ob = (const spn_dag_obs_t*)b;
  s32 order = sp_str_compare_alphabetical(oa->path, ob->path);
  if (order) {
    return order;
  }
  return sp_str_compare_alphabetical(oa->filter, ob->filter);
}

static s32 glob_match_order(const void* a, const void* b) {
  return sp_str_compare_alphabetical(((const spn_dag_match_t*)a)->relative, ((const spn_dag_match_t*)b)->relative);
}

static void run_test(s32* utest_result, glob_test_t t) {
  tmpfs_t fs = sp_zero;
  tmpfs_init_named(&fs, t.name);
  sp_str_t root = tmpfs_get(&fs, sp_str_lit("R"));
  sp_fs_create_dir(root);

  sp_carr_for(t.files, it) {
    if (!t.files[it]) {
      break;
    }
    tmpfs_create(&fs, sp_fmt(fs.mem, "R/{}", sp_fmt_cstr(t.files[it])).value, sp_str_lit("S"));
  }
  sp_carr_for(t.dirs, it) {
    if (!t.dirs[it]) {
      break;
    }
    sp_fs_create_dir(tmpfs_get(&fs, sp_fmt(fs.mem, "R/{}", sp_fmt_cstr(t.dirs[it])).value));
  }

  sp_da(spn_dag_obs_t) obs = sp_da_new(fs.mem, spn_dag_obs_t);
  sp_da(spn_dag_match_t) matches = sp_da_new(fs.mem, spn_dag_match_t);
  spn_err_t err = spn_dag_glob(fs.mem, root, sp_str_view(t.pattern), &obs, &matches);
  EXPECT_EQ(t.expect.err, err);
  if (err) {
    tmpfs_deinit(&fs);
    return;
  }

  sp_da(spn_dag_obs_t) enums = sp_da_new(fs.mem, spn_dag_obs_t);
  sp_da(spn_dag_obs_t) file_obs = sp_da_new(fs.mem, spn_dag_obs_t);
  sp_da(spn_dag_obs_t) absent_obs = sp_da_new(fs.mem, spn_dag_obs_t);
  sp_da_for(obs, it) {
    switch (obs[it].kind) {
      case SPN_DAG_OBS_ENUMERATION: sp_da_push(enums, obs[it]);      break;
      case SPN_DAG_OBS_FILE:        sp_da_push(file_obs, obs[it]);   break;
      case SPN_DAG_OBS_ABSENT:      sp_da_push(absent_obs, obs[it]); break;
    }
  }
  sp_da_sort(enums, glob_obs_order);
  sp_da_sort(file_obs, glob_obs_order);
  sp_da_sort(absent_obs, glob_obs_order);
  sp_da_sort(matches, glob_match_order);

  u32 expect_enums = 0;
  sp_carr_for(t.expect.enums, it) {
    if (!t.expect.enums[it].dir) {
      break;
    }
    expect_enums++;
  }
  ASSERT_EQ(expect_enums, (u32)sp_da_size(enums));
  sp_for(it, expect_enums) {
    sp_str_t dir = *t.expect.enums[it].dir
      ? tmpfs_get(&fs, sp_fmt(fs.mem, "R/{}", sp_fmt_cstr(t.expect.enums[it].dir)).value)
      : root;
    sp_str_t filter = t.expect.enums[it].filter ? sp_str_view(t.expect.enums[it].filter) : sp_str_lit("");
    EXPECT_TRUE(sp_str_equal(enums[it].path, dir));
    EXPECT_TRUE(sp_str_equal(enums[it].filter, filter));
  }

  u32 expect_matches = 0;
  sp_carr_for(t.expect.matches, it) {
    if (!t.expect.matches[it]) {
      break;
    }
    expect_matches++;
  }
  ASSERT_EQ(expect_matches, (u32)sp_da_size(matches));
  ASSERT_EQ(expect_matches, (u32)sp_da_size(file_obs));
  sp_for(it, expect_matches) {
    sp_str_t relative = sp_str_view(t.expect.matches[it]);
    sp_str_t path = tmpfs_get(&fs, sp_fmt(fs.mem, "R/{}", sp_fmt_cstr(t.expect.matches[it])).value);
    EXPECT_TRUE(sp_str_equal(matches[it].relative, relative));
    EXPECT_TRUE(sp_str_equal(matches[it].path, path));
    EXPECT_TRUE(sp_str_equal(file_obs[it].path, path));
  }

  u32 expect_absent = 0;
  sp_carr_for(t.expect.absent, it) {
    if (!t.expect.absent[it]) {
      break;
    }
    expect_absent++;
  }
  ASSERT_EQ(expect_absent, (u32)sp_da_size(absent_obs));
  sp_for(it, expect_absent) {
    EXPECT_TRUE(sp_str_equal(absent_obs[it].path, tmpfs_get(&fs, sp_fmt(fs.mem, "R/{}", sp_fmt_cstr(t.expect.absent[it])).value)));
  }

  tmpfs_deinit(&fs);
}

UTEST_F(glob, root_pattern) {
  run_test(&ur, (glob_test_t) {
    .name = "glob_root",
    .files = { "X.h", "Y.c" },
    .pattern = "*.h",
    .expect = {
      .enums = { { "", "*.h" } },
      .matches = { "X.h" },
    }
  });
}

UTEST_F(glob, subdir_pattern) {
  run_test(&ur, (glob_test_t) {
    .name = "glob_subdir",
    .files = { "A/X.h", "A/Y.c", "Z.h" },
    .pattern = "A/*.h",
    .expect = {
      .enums = { { "A", "*.h" } },
      .matches = { "A/X.h" },
    }
  });
}

UTEST_F(glob, literal_pattern_probes_file) {
  run_test(&ur, (glob_test_t) {
    .name = "glob_literal",
    .files = { "A/X.h", "A/Y.h" },
    .pattern = "A/X.h",
    .expect = {
      .matches = { "A/X.h" },
    }
  });
}

UTEST_F(glob, literal_missing_probes_absent) {
  run_test(&ur, (glob_test_t) {
    .name = "glob_literal_missing",
    .files = { "A/X.h" },
    .pattern = "A/Z.h",
    .expect = {
      .absent = { "A/Z.h" },
    }
  });
}

UTEST_F(glob, invalid_pattern_fails) {
  run_test(&ur, (glob_test_t) {
    .name = "glob_invalid",
    .pattern = "A/[",
    .expect = {
      .err = SPN_ERR_DAG_GLOB,
    }
  });
}

UTEST_F(glob, recursive_pattern) {
  run_test(&ur, (glob_test_t) {
    .name = "glob_recursive",
    .files = { "A/X.h", "A/B/Y.h", "A/B/Z.c" },
    .dirs = { "A/C" },
    .pattern = "A/**/*.h",
    .expect = {
      .enums = { { "A", "*.h" }, { "A/B", "*.h" }, { "A/C", "*.h" } },
      .matches = { "A/B/Y.h", "A/X.h" },
    }
  });
}

UTEST_F(glob, recursive_all) {
  run_test(&ur, (glob_test_t) {
    .name = "glob_recursive_all",
    .files = { "A/X.h", "A/B/Y.c" },
    .pattern = "A/**",
    .expect = {
      .enums = { { "A", "" }, { "A/B", "" } },
      .matches = { "A/B/Y.c", "A/X.h" },
    }
  });
}

UTEST_F(glob, missing_dir_still_observed) {
  run_test(&ur, (glob_test_t) {
    .name = "glob_missing_dir",
    .files = { "X.h" },
    .pattern = "B/*.h",
    .expect = {
      .enums = { { "B", "*.h" } },
    }
  });
}

typedef struct {
  const c8* path;
  const c8* content;
} glob_exec_file_t;

typedef struct {
  glob_exec_file_t files [DAG_TEST_MAX_INPUTS];
  u32 expect_runs;
} glob_exec_run_t;

typedef struct {
  const c8* name;
  const c8* pattern;
  glob_exec_run_t runs [DAG_TEST_MAX_OPS];
} glob_exec_test_t;

typedef struct {
  dag_test_env_t dag;
  sp_str_t root;
  const c8* pattern;
} glob_exec_env_t;

static spn_err_t glob_exec_discover(spn_dag_action_t* action, void* user_data, sp_mem_t mem, sp_da(spn_dag_obs_t)* out) {
  glob_exec_env_t* env = (glob_exec_env_t*)user_data;
  return spn_dag_glob(mem, env->root, sp_str_view(env->pattern), out, SP_NULLPTR);
}

static void run_exec_test(s32* utest_result, glob_exec_test_t t) {
  glob_exec_env_t env = sp_zero;
  dag_test_env_init(&env.dag, (dag_test_env_config_t) {
    .name = t.name,
    .store = SPN_DAG_STORE_MEM,
    .discovery = true
  });
  env.root = tmpfs_get(&env.dag.fs, sp_str_lit("R"));
  env.pattern = t.pattern;
  sp_fs_create_dir(env.root);

  sp_carr_for(t.runs, r) {
    glob_exec_run_t* run = &t.runs[r];
    if (!run->expect_runs) {
      break;
    }

    spn_dag_file_cache_refresh(&env.dag.files);
    sp_carr_for(run->files, it) {
      if (!run->files[it].path) {
        break;
      }
      tmpfs_create(&env.dag.fs, sp_fmt(env.dag.fs.mem, "R/{}", sp_fmt_cstr(run->files[it].path)).value, sp_str_view(run->files[it].content));
    }

    spn_dag_t* g = dag_test_env_graph(&env.dag);
    spn_dag_id_t action = spn_dag_add_action(g, (spn_dag_action_config_t) {
      .identity = dag_test_digest(t.name),
      .execute = dag_test_exec_stamp,
      .discover = glob_exec_discover,
      .user_data = &env
    });
    ASSERT_EQ(SPN_OK, spn_dag_action_add_output(g, action, spn_dag_add_output(g, sp_str_lit("O"))));

    EXPECT_EQ(SPN_OK, spn_dag_execute_discovered(g, action, &env.dag.env));
    EXPECT_EQ(run->expect_runs, env.dag.runs);
  }

  dag_test_env_deinit(&env.dag);
}

UTEST_F(glob, discovered_content_change_reruns) {
  run_exec_test(&ur, (glob_exec_test_t) {
    .name = "glob_content_change",
    .pattern = "*.h",
    .runs = {
      { .files = { { "X.h", "A" } }, .expect_runs = 1 },
      { .expect_runs = 1 },
      { .files = { { "X.h", "B" } }, .expect_runs = 2 },
      { .expect_runs = 2 },
    }
  });
}
