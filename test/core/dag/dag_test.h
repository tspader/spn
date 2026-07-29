#pragma once

#include "spn_test.h"

#include "sp/atomic_file.h"
#include "dag/dag.h"

#define DAG_TEST_MAX_INPUTS 4
#define DAG_TEST_MAX_OUTPUTS 4
#define DAG_TEST_MAX_OPS 8

typedef struct {
  const c8* path;
  const c8* content;
  spn_dag_obs_kind_t kind;
  const c8* filter;
} dag_test_obs_t;

typedef struct {
  const c8* project;
  const c8* store;
} dag_test_roots_t;

typedef struct {
  const c8* sub;
  spn_dag_store_kind_t store;
  bool discovery;
} dag_test_env_config_t;

typedef struct {
  sp_mem_t mem;
  sp_str_t root;
  spn_dag_t* g;
  spn_dag_roots_t roots;
  spn_dag_store_t store;
  spn_dag_file_cache_t files;
  spn_dag_action_cache_t cache;
  spn_dag_obs_table_t discovery;
  spn_dag_obs_table_t memos;
  spn_dag_env_t env;
  u32 runs;
} dag_test_env_t;

extern const spn_dag_store_kind_t dag_test_store_kinds [2];

const c8*        dag_test_store_name(spn_dag_store_kind_t kind);
void             dag_test_env_init(dag_test_env_t* env, sp_test_t* t, dag_test_env_config_t config);
void             dag_test_env_cold(dag_test_env_t* env);
spn_dag_t*       dag_test_env_graph(dag_test_env_t* env);
sp_str_t         dag_test_env_path(dag_test_env_t* env, sp_str_t rel);
void             dag_test_env_create(dag_test_env_t* env, sp_str_t rel, sp_str_t content);
void             dag_test_create(sp_str_t path, sp_str_t content);
spn_dag_digest_t dag_test_digest(const c8* data);
const spn_dag_roots_t* dag_test_roots_build(dag_test_roots_t spec, spn_dag_roots_t* out);
u32              dag_test_obs_build(const dag_test_obs_t* specs, u32 cap, spn_dag_obs_t* out);
s32              dag_test_exec_stamp(spn_dag_t* g, spn_dag_action_t* action, void* user_data);
sp_err_t         dag_test_expect_file(sp_test_t* t, sp_mem_t mem, sp_str_t path, const c8* expected);
