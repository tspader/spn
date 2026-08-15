#include "dag_test.h"

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
  const c8* nested_root;
  const c8* pattern;
  glob_expect_t expect;
} glob_test_t;

static const glob_test_t glob_tests [] = {
  {
    .name = "root_pattern",
    .files = { "X.h", "Y.c" },
    .pattern = "*.h",
    .expect = {
      .enums = { { "", "*.h" } },
      .matches = { "X.h" },
    }
  },
  {
    .name = "subdir_pattern",
    .files = { "A/X.h", "A/Y.c", "Z.h" },
    .pattern = "A/*.h",
    .expect = {
      .enums = { { "A", "*.h" } },
      .matches = { "A/X.h" },
    }
  },
  {
    .name = "literal_pattern_probes_file",
    .files = { "A/X.h", "A/Y.h" },
    .pattern = "A/X.h",
    .expect = {
      .matches = { "A/X.h" },
    }
  },
  {
    .name = "literal_missing_probes_absent",
    .files = { "A/X.h" },
    .pattern = "A/Z.h",
    .expect = {
      .absent = { "A/Z.h" },
    }
  },
  {
    .name = "invalid_pattern_fails",
    .pattern = "A/[",
    .expect = {
      .err = SPN_ERR_DAG_GLOB,
    }
  },
  {
    .name = "recursive_pattern",
    .files = { "A/X.h", "A/B/Y.h", "A/B/Z.c" },
    .dirs = { "A/C" },
    .pattern = "A/**/*.h",
    .expect = {
      .enums = { { "A", "*.h" }, { "A/B", "*.h" }, { "A/C", "*.h" } },
      .matches = { "A/B/Y.h", "A/X.h" },
    }
  },
  {
    .name = "recursive_all",
    .files = { "A/X.h", "A/B/Y.c" },
    .pattern = "A/**",
    .expect = {
      .enums = { { "A", "" }, { "A/B", "" } },
      .matches = { "A/B/Y.c", "A/X.h" },
    }
  },
  {
    .name = "missing_dir_still_observed",
    .files = { "X.h" },
    .pattern = "B/*.h",
    .expect = {
      .enums = { { "B", "*.h" } },
    }
  },
  {
    .name = "obs_keep_dir_root_over_nested_root",
    .files = { "A/X.h" },
    .nested_root = "A",
    .pattern = "A/*.h",
    .expect = {
      .enums = { { "A", "*.h" } },
      .matches = { "A/X.h" },
    }
  },
};

typedef struct {
  sp_str_t path;
  sp_str_t filter;
  spn_dag_obs_kind_t kind;
  spn_path_root_t root;
} glob_seen_t;

static s32 glob_obs_order(const void* a, const void* b) {
  const glob_seen_t* oa = (const glob_seen_t*)a;
  const glob_seen_t* ob = (const glob_seen_t*)b;
  s32 order = sp_str_compare_alphabetical(oa->path, ob->path);
  if (order) {
    return order;
  }
  return sp_str_compare_alphabetical(oa->filter, ob->filter);
}

sp_test_each(dag_glob, observe, glob_test_t, glob_tests) {
  sp_mem_t mem = sp_test_arena(t);
  sp_str_t root = sp_fs_join_path(mem, sp_test_dir(t), sp_str_lit("R"));
  sp_fs_create_dir(root);

  spn_path_roots_t storage = sp_zero;
  storage.dirs[SPN_PATH_ROOT_PROJECT] = root;
  const spn_path_roots_t* roots = &storage;

  sp_carr_for(it->files, ft) {
    if (!it->files[ft]) {
      break;
    }
    dag_test_create(sp_fs_join_path(mem, root, sp_cstr_as_str(it->files[ft])), sp_str_lit("S"));
  }
  sp_carr_for(it->dirs, dt) {
    if (!it->dirs[dt]) {
      break;
    }
    sp_fs_create_dir(sp_fs_join_path(mem, root, sp_cstr_as_str(it->dirs[dt])));
  }
  if (it->nested_root) {
    storage.dirs[SPN_PATH_ROOT_STORE] = sp_fs_join_path(mem, root, sp_cstr_as_str(it->nested_root));
  }

  sp_da(spn_dag_obs_t) obs = sp_da_new(mem, spn_dag_obs_t);
  sp_da(spn_path_t) matches = sp_da_new(mem, spn_path_t);
  spn_path_t pattern = { .root = SPN_PATH_ROOT_PROJECT, .sub = sp_str_view(it->pattern) };
  spn_err_t err = spn_dag_glob(mem, roots, pattern, &obs, &matches);
  sp_expect_eq(t, it->expect.err, err);
  if (err) {
    return SP_OK;
  }

  sp_da(glob_seen_t) enums = sp_da_new(mem, glob_seen_t);
  sp_da(glob_seen_t) file_obs = sp_da_new(mem, glob_seen_t);
  sp_da(glob_seen_t) absent_obs = sp_da_new(mem, glob_seen_t);
  sp_da_for(obs, ot) {
    glob_seen_t seen = {
      .path = spn_path_str(roots, mem, obs[ot].path),
      .filter = obs[ot].filter,
      .kind = obs[ot].kind,
      .root = obs[ot].path.root,
    };
    switch (obs[ot].kind) {
      case SPN_DAG_OBS_ENUMERATION: sp_da_push(enums, seen);      break;
      case SPN_DAG_OBS_FILE:        sp_da_push(file_obs, seen);   break;
      case SPN_DAG_OBS_ABSENT:      sp_da_push(absent_obs, seen); break;
    }
  }
  sp_da_sort(enums, glob_obs_order);
  sp_da_sort(file_obs, glob_obs_order);
  sp_da_sort(absent_obs, glob_obs_order);

  u32 expect_enums = 0;
  sp_carr_for(it->expect.enums, et) {
    if (!it->expect.enums[et].dir) {
      break;
    }
    expect_enums++;
  }
  sp_must_eq(t, expect_enums, (u32)sp_da_size(enums));
  sp_for(et, expect_enums) {
    sp_str_t dir = *it->expect.enums[et].dir
      ? sp_fs_join_path(mem, root, sp_cstr_as_str(it->expect.enums[et].dir))
      : root;
    sp_str_t filter = it->expect.enums[et].filter ? sp_str_view(it->expect.enums[et].filter) : sp_str_lit("");
    sp_expect_str_eq(t, enums[et].path, dir);
    sp_expect_str_eq(t, enums[et].filter, filter);
    sp_expect_eq(t, SPN_PATH_ROOT_PROJECT, enums[et].root);
  }

  u32 expect_matches = 0;
  sp_carr_for(it->expect.matches, mt) {
    if (!it->expect.matches[mt]) {
      break;
    }
    expect_matches++;
  }
  sp_must_eq(t, expect_matches, (u32)sp_da_size(matches));
  sp_must_eq(t, expect_matches, (u32)sp_da_size(file_obs));
  sp_for(mt, expect_matches) {
    sp_str_t sub = sp_str_view(it->expect.matches[mt]);
    sp_expect_str_eq(t, matches[mt].sub, sub);
    sp_expect_eq(t, SPN_PATH_ROOT_PROJECT, matches[mt].root);
    sp_expect_str_eq(t, file_obs[mt].path, sp_fs_join_path(mem, root, sub));
    sp_expect_eq(t, SPN_PATH_ROOT_PROJECT, file_obs[mt].root);
  }

  u32 expect_absent = 0;
  sp_carr_for(it->expect.absent, at) {
    if (!it->expect.absent[at]) {
      break;
    }
    expect_absent++;
  }
  sp_must_eq(t, expect_absent, (u32)sp_da_size(absent_obs));
  sp_for(at, expect_absent) {
    sp_expect_str_eq(t, absent_obs[at].path, sp_fs_join_path(mem, root, sp_cstr_as_str(it->expect.absent[at])));
    sp_expect_eq(t, SPN_PATH_ROOT_PROJECT, absent_obs[at].root);
  }

  return SP_OK;
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
  spn_path_t pattern;
} glob_exec_env_t;

static const glob_exec_test_t glob_exec_tests [] = {
  {
    .name = "discovered_content_change_reruns",
    .pattern = "*.h",
    .runs = {
      { .files = { { "X.h", "A" } }, .expect_runs = 1 },
      { .expect_runs = 1 },
      { .files = { { "X.h", "B" } }, .expect_runs = 2 },
      { .expect_runs = 2 },
    }
  },
};

static spn_err_t glob_exec_discover(spn_dag_t* g, spn_dag_action_t* action, void* user_data, spn_dag_env_t* dag_env, sp_mem_t mem, sp_da(spn_dag_obs_t)* out) {
  glob_exec_env_t* env = (glob_exec_env_t*)user_data;
  return spn_dag_glob(mem, g->roots, env->pattern, out, SP_NULLPTR);
}

sp_test_each(dag_glob, exec, glob_exec_test_t, glob_exec_tests) {
  glob_exec_env_t env = sp_zero;
  dag_test_env_init(&env.dag, t, (dag_test_env_config_t) {
    .store = SPN_DAG_STORE_MEM,
    .discovery = true
  });
  env.root = dag_test_env_path(&env.dag, sp_str_lit("R"));
  env.pattern = spn_path_join(env.dag.mem, dag_test_env_rooted(&env.dag, sp_str_lit("R")), sp_str_view(it->pattern));
  sp_fs_create_dir(env.root);

  sp_carr_for(it->runs, r) {
    const glob_exec_run_t* run = &it->runs[r];
    if (!run->expect_runs) {
      break;
    }

    spn_dag_file_cache_invalidate_all(&env.dag.files);
    sp_carr_for(run->files, ft) {
      if (!run->files[ft].path) {
        break;
      }
      dag_test_create(sp_fs_join_path(env.dag.mem, env.root, sp_cstr_as_str(run->files[ft].path)), sp_str_view(run->files[ft].content));
    }

    spn_dag_t* g = dag_test_env_graph(&env.dag);
    spn_dag_id_t action = spn_dag_add_action(g, (spn_dag_action_config_t) {
      .identity = dag_test_digest(it->name),
      .execute = dag_test_exec_stamp,
      .discover = glob_exec_discover,
      .user_data = &env
    });
    sp_must_eq(t, SPN_OK, spn_dag_action_add_output(g, action, spn_dag_add_output(g, sp_str_lit("O"))));

    sp_expect_eq(t, SPN_OK, spn_dag_execute_discovered(g, action, &env.dag.env));
    sp_expect_eq(t, run->expect_runs, env.dag.runs);
  }

  return SP_OK;
}
