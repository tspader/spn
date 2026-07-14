#pragma once

#include "sp.h"
#include "sp/atomic_file.h"
#include "sp/sp_glob.h"
#include "utest.h"
#include "test.h"
#include "dag/dag.h"

#define DAG_TEST_MAX_INPUTS 4
#define DAG_TEST_MAX_OUTPUTS 4
#define DAG_TEST_MAX_OPS 8

#define EXPECT_STR(actual, cstr) EXPECT_TRUE(sp_str_equal((actual), sp_str_view(cstr)))

typedef struct {
  const c8* path;
  const c8* content;
  spn_dag_obs_kind_t kind;
  const c8* filter;
} dag_test_obs_t;

typedef struct {
  const c8* name;
  spn_dag_store_kind_t store;
  bool discovery;
} dag_test_env_config_t;

typedef struct {
  tmpfs_t fs;
  spn_dag_t* g;
  spn_dag_store_t store;
  spn_dag_file_cache_t files;
  spn_dag_action_cache_t cache;
  spn_dag_discovery_t discovery;
  spn_dag_env_t env;
  u32 runs;
} dag_test_env_t;

extern const spn_dag_store_kind_t dag_test_store_kinds [2];

void             dag_test_env_init(dag_test_env_t* env, dag_test_env_config_t config);
void             dag_test_env_cold(dag_test_env_t* env);
spn_dag_t*       dag_test_env_graph(dag_test_env_t* env);
void             dag_test_env_deinit(dag_test_env_t* env);
spn_dag_digest_t dag_test_digest(const c8* data);
u32              dag_test_obs_build(const dag_test_obs_t* specs, u32 cap, spn_dag_obs_t* out);
s32              dag_test_exec_stamp(spn_dag_action_t* action, void* user_data);
void             dag_test_expect_file(s32* utest_result, sp_mem_t mem, sp_str_t path, const c8* expected);
