#ifndef SPN_DAG_TYPES_H
#define SPN_DAG_TYPES_H

#include "sp.h"
#include "spn.h"

typedef struct spn_dag_action_t spn_dag_action_t;

typedef struct {
  u8 bytes [32];
} spn_dag_digest_t;

typedef struct {
  u64 device;
  u64 id;
} spn_dag_file_id_t;

typedef struct {
  spn_dag_file_id_t id;
  sp_sys_timespec_t mtime;
  s64 size;
  spn_dag_digest_t digest;
} spn_dag_file_meta_t;

typedef enum {
  SPN_DAG_OBS_FILE,
  SPN_DAG_OBS_ABSENT,
  SPN_DAG_OBS_ENUMERATION,
} spn_dag_obs_kind_t;

typedef struct {
  spn_dag_obs_kind_t kind;
  sp_str_t path;
  sp_str_t filter;
  spn_dag_file_meta_t meta;
} spn_dag_obs_t;

typedef struct {
  sp_str_t path;
  sp_str_t relative;
} spn_dag_match_t;

SP_TYPEDEF_FN(s32, spn_dag_exec_fn_t, spn_dag_action_t*, void*);
SP_TYPEDEF_FN(spn_err_t, spn_dag_discover_fn_t, spn_dag_action_t*, void*, sp_mem_t, sp_da(spn_dag_obs_t)*);

typedef struct {
  u32 index;
  u32 occupied;
} spn_dag_id_t;

typedef enum {
  SPN_DAG_ARTIFACT_KIND_VALUE,
  SPN_DAG_ARTIFACT_KIND_FILE,
  SPN_DAG_ARTIFACT_KIND_TREE,
} spn_dag_artifact_kind_t;

typedef struct {
  spn_dag_id_t id;
  spn_dag_artifact_kind_t kind;
  sp_str_t name;
  sp_str_t target;
  sp_str_t path;
  spn_dag_digest_t digest;
  spn_dag_id_t producer;
  sp_da(spn_dag_id_t) consumers;
} spn_dag_artifact_t;

struct spn_dag_action_t {
  spn_dag_id_t id;
  spn_dag_digest_t identity;
  spn_dag_exec_fn_t execute;
  spn_dag_discover_fn_t discover;
  void* user_data;
  sp_da(spn_dag_id_t) consumes;
  sp_da(spn_dag_id_t) produces;
};

typedef struct {
  spn_dag_digest_t identity;
  spn_dag_exec_fn_t execute;
  spn_dag_discover_fn_t discover;
  void* user_data;
} spn_dag_action_config_t;

typedef struct {
  sp_mem_arena_t* arena;
  sp_mem_t mem;
  sp_da(spn_dag_artifact_t) artifacts;
  sp_da(spn_dag_action_t) actions;
  sp_ht(sp_str_t, spn_dag_id_t) paths;
} spn_dag_t;

typedef struct {
  sp_mem_arena_t* arena;
  sp_mem_t mem;
  sp_ht(spn_dag_file_id_t, spn_dag_file_meta_t) entries;
  sp_ht(sp_str_t, sp_sys_file_meta_t) metadata;
} spn_dag_file_cache_t;

typedef struct {
  sp_str_t name;
  spn_dag_digest_t digest;
} spn_dag_action_output_t;

typedef struct {
  sp_da(spn_dag_action_output_t) outputs;
} spn_dag_action_entry_t;

typedef struct {
  sp_mem_arena_t* arena;
  sp_mem_t mem;
  sp_str_t dir;
  sp_ht(spn_dag_digest_t, spn_dag_action_entry_t) entries;
} spn_dag_action_cache_t;

typedef struct {
  sp_da(spn_dag_obs_t) obs;
} spn_dag_pathset_t;

typedef struct {
  sp_mem_arena_t* arena;
  sp_mem_t mem;
  sp_str_t dir;
  sp_ht(spn_dag_digest_t, spn_dag_pathset_t) entries;
} spn_dag_discovery_t;

typedef enum {
  SPN_DAG_STORE_MEM,
  SPN_DAG_STORE_FILESYSTEM,
} spn_dag_store_kind_t;

typedef struct {
  spn_dag_store_kind_t kind;
  sp_mem_t mem;
  sp_str_t dir;
} spn_dag_store_config_t;

typedef struct {
  spn_dag_store_kind_t kind;
  sp_mem_arena_t* arena;
  sp_mem_t mem;
  sp_str_t dir;
  sp_mutex_t mutex;
  sp_ht(spn_dag_digest_t, sp_mem_slice_t) blobs;
} spn_dag_store_t;

typedef struct {
  spn_dag_file_cache_t* files;
  spn_dag_action_cache_t* cache;
  spn_dag_store_t* store;
  spn_dag_discovery_t* discovery;
  sp_str_t scratch;
} spn_dag_env_t;

typedef struct {
  void (*fn)(void* data);
  void* data;
} spn_dag_job_t;

typedef struct spn_dag_executor_t spn_dag_executor_t;
struct spn_dag_executor_t {
  void (*submit)(spn_dag_executor_t* ex, spn_dag_job_t job);
  spn_dag_job_t (*poll)(spn_dag_executor_t* ex);
};

typedef struct {
  spn_dag_executor_t executor;
  sp_mem_arena_t* arena;
  sp_mem_t mem;
  sp_mutex_t mutex;
  sp_cv_t submitted;
  sp_cv_t completed;
  sp_da(spn_dag_job_t) queue;
  sp_da(spn_dag_job_t) done;
  sp_da(sp_thread_t) workers;
  bool shutdown;
} spn_dag_pool_t;

#endif
