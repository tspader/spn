#include "dag_test.h"

const spn_dag_store_kind_t dag_test_store_kinds [2] = {
  SPN_DAG_STORE_MEM,
  SPN_DAG_STORE_FILESYSTEM,
};

const c8* dag_test_store_name(spn_dag_store_kind_t kind) {
  switch (kind) {
    case SPN_DAG_STORE_MEM:        return "memory";
    case SPN_DAG_STORE_FILESYSTEM: return "filesystem";
  }
  return "unknown";
}

void dag_test_env_init(dag_test_env_t* env, sp_test_t* t, dag_test_env_config_t config) {
  sp_mem_zero(env, sizeof(*env));
  env->mem = sp_test_arena(t);
  env->root = config.sub
    ? sp_fs_join_path(env->mem, sp_test_dir(t), sp_str_view(config.sub))
    : sp_test_dir(t);
  if (!sp_fs_exists(env->root)) {
    sp_fs_create_dir(env->root);
  }
  env->roots.dirs[SPN_PATH_ROOT_PROJECT] = env->root;
  spn_dag_store_init(&env->store, (spn_dag_store_config_t) {
    .kind = config.store,
    .mem = env->mem,
    .roots = &env->roots,
    .dir = dag_test_env_rooted(env, sp_str_lit("store"))
  });
  spn_dag_file_cache_init(&env->files, env->mem, &env->roots);
  spn_dag_file_cache_fence(&env->files, SPN_DAG_STAMP_TRUST_ALL);
  spn_dag_file_cache_load(&env->files, dag_test_env_path(env, sp_str_lit("files")));
  spn_dag_action_cache_init(&env->cache, env->mem, sp_str_lit(""));
  spn_dag_obs_table_init(&env->discovery, env->mem, dag_test_env_path(env, sp_str_lit("manifests")));
  env->env = (spn_dag_env_t) {
    .files = &env->files,
    .cache = &env->cache,
    .store = &env->store,
    .discovery = config.discovery ? &env->discovery : SP_NULLPTR,
    .scratch = dag_test_env_rooted(env, sp_str_lit("scratch"))
  };
}

void dag_test_env_cold(dag_test_env_t* env) {
  spn_dag_file_cache_flush(&env->files, dag_test_env_path(env, sp_str_lit("files")));
  spn_dag_file_cache_init(&env->files, env->mem, &env->roots);
  spn_dag_file_cache_fence(&env->files, SPN_DAG_STAMP_TRUST_ALL);
  spn_dag_file_cache_load(&env->files, dag_test_env_path(env, sp_str_lit("files")));
  spn_dag_obs_table_init(&env->discovery, env->mem, dag_test_env_path(env, sp_str_lit("manifests")));
}

spn_dag_t* dag_test_env_graph(dag_test_env_t* env) {
  env->g = spn_dag_new(env->mem, &env->roots);
  return env->g;
}

sp_str_t dag_test_env_path(dag_test_env_t* env, sp_str_t rel) {
  return sp_fs_join_path(env->mem, env->root, rel);
}

spn_path_t dag_test_env_rooted(dag_test_env_t* env, sp_str_t rel) {
  return spn_path_join(env->mem, spn_path_from_root(SPN_PATH_ROOT_PROJECT), rel);
}

sp_str_t dag_test_render(dag_test_env_t* env, spn_path_t path) {
  return spn_path_str(&env->roots, env->mem, path);
}

void dag_test_env_create(dag_test_env_t* env, sp_str_t rel, sp_str_t content) {
  dag_test_create(dag_test_env_path(env, rel), content);
}

void dag_test_create(sp_str_t path, sp_str_t content) {
  sp_fs_create_dir(sp_fs_parent_path(path));
  sp_fs_remove_file(path);

  sp_io_file_writer_t f = sp_zero;
  sp_io_file_writer_from_path(&f, path);
  if (!sp_str_empty(content)) {
    sp_io_write(&f.base, content.data, content.len, SP_NULLPTR);
  }
  sp_io_file_writer_close(&f);
}

spn_dag_digest_t dag_test_digest(const c8* data) {
  if (!data) {
    return (spn_dag_digest_t) sp_zero;
  }
  sp_str_t str = sp_str_view(data);
  return spn_dag_digest(str.data, str.len);
}

u32 dag_test_obs_build(const dag_test_obs_t* specs, u32 cap, spn_dag_obs_t* out) {
  u32 count = 0;
  sp_for(it, cap) {
    if (!specs[it].path) {
      break;
    }
    out[count] = (spn_dag_obs_t) {
      .kind = specs[it].kind,
      .path = { .root = specs[it].root, .sub = sp_str_view(specs[it].path) },
      .filter = specs[it].filter ? sp_str_view(specs[it].filter) : sp_str_lit("")
    };
    if (specs[it].content) {
      out[count].meta.digest = dag_test_digest(specs[it].content);
    }
    count++;
  }
  return count;
}

s32 dag_test_exec_stamp(spn_dag_t* g, spn_dag_action_t* action, void* user_data) {
  dag_test_env_t* env = (dag_test_env_t*)user_data;
  env->runs++;
  spn_dag_artifact_t* out = spn_dag_find_artifact(env->g, action->produces[0]);
  sp_str_t content = sp_fmt(env->mem, "{}", sp_fmt_uint(env->runs)).value;
  return sp_fs_create_file_str(dag_test_render(env, out->materialized), content) ? 1 : 0;
}

sp_err_t dag_test_expect_file(sp_test_t* t, sp_mem_t mem, sp_str_t path, const c8* expected) {
  sp_str_t from_disk = sp_zero;
  sp_must_eq(t, SP_OK, sp_io_read_file(mem, path, &from_disk));
  sp_expect_str_eq_c(t, from_disk, expected);
  return SP_OK;
}
