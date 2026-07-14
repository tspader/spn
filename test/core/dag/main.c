#define SP_IMPLEMENTATION
#define UTEST_IMPLEMENTATION
#include "common.h"
#include "sp_sim.h"

UTEST_STATE();

const spn_dag_store_kind_t dag_test_store_kinds [2] = {
  SPN_DAG_STORE_MEM,
  SPN_DAG_STORE_FILESYSTEM,
};

void dag_test_env_init(dag_test_env_t* env, dag_test_env_config_t config) {
  tmpfs_init_named(&env->fs, config.name);
  spn_dag_store_init(&env->store, (spn_dag_store_config_t) {
    .kind = config.store,
    .mem = env->fs.mem,
    .dir = tmpfs_get(&env->fs, sp_str_lit("store"))
  });
  spn_dag_file_cache_init(&env->files, env->fs.mem);
  spn_dag_action_cache_init(&env->cache, env->fs.mem, sp_str_lit(""));
  spn_dag_discovery_init(&env->discovery, env->fs.mem, tmpfs_get(&env->fs, sp_str_lit("manifests")));
  env->env = (spn_dag_env_t) {
    .files = &env->files,
    .cache = &env->cache,
    .store = &env->store,
    .discovery = config.discovery ? &env->discovery : SP_NULLPTR,
    .scratch = tmpfs_get(&env->fs, sp_str_lit("scratch"))
  };
}

void dag_test_env_cold(dag_test_env_t* env) {
  spn_dag_file_cache_init(&env->files, env->fs.mem);
  spn_dag_discovery_init(&env->discovery, env->fs.mem, tmpfs_get(&env->fs, sp_str_lit("manifests")));
}

spn_dag_t* dag_test_env_graph(dag_test_env_t* env) {
  env->g = spn_dag_new(env->fs.mem);
  return env->g;
}

void dag_test_env_deinit(dag_test_env_t* env) {
  tmpfs_deinit(&env->fs);
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
      .path = sp_str_view(specs[it].path),
      .filter = specs[it].filter ? sp_str_view(specs[it].filter) : sp_str_lit("")
    };
    if (specs[it].content) {
      out[count].meta.digest = dag_test_digest(specs[it].content);
    }
    count++;
  }
  return count;
}

s32 dag_test_exec_stamp(spn_dag_action_t* action, void* user_data) {
  dag_test_env_t* env = (dag_test_env_t*)user_data;
  env->runs++;
  spn_dag_artifact_t* out = spn_dag_find_artifact(env->g, action->produces[0]);
  sp_str_t content = sp_fmt(env->fs.mem, "{}", sp_fmt_uint(env->runs)).value;
  return sp_fs_create_file_str(out->path, content) ? 1 : 0;
}

void dag_test_expect_file(s32* utest_result, sp_mem_t mem, sp_str_t path, const c8* expected) {
  sp_str_t from_disk = sp_zero;
  ASSERT_EQ(SP_OK, sp_io_read_file(mem, path, &from_disk));
  EXPECT_STR(from_disk, expected);
}

static sp_sim_t dag_sim;

s32 dag_test_entry(s32 argc, const c8** argv) {
  if (!sp_str_empty(sp_os_env_get(sp_str_lit("SPN_TEST_SIM")))) {
    sp_sim_init(&dag_sim, sp_mem_os_new());
    sp_sim_install(&dag_sim);
  }
  return utest_main(argc, argv);
}
SP_MAIN(dag_test_entry)
