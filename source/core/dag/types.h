#ifndef SPN_DAG_TYPES_H
#define SPN_DAG_TYPES_H

#include "sp.h"
#include "spn/core.h"
#include "core/types.h"
#include "paths/types.h"

typedef struct spn_dag_action_t spn_dag_action_t;
typedef struct spn_dag_t spn_dag_t;

typedef struct {
  u8 bytes [32];
} spn_dag_digest_t;

typedef struct {
  u64 device;
  u64 inode;
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
  spn_path_t path;
  sp_str_t filter;
} spn_dag_obs_t;

typedef struct spn_dag_env_t spn_dag_env_t;

SP_TYPEDEF_FN(spn_err_t, spn_dag_exec_fn_t, spn_dag_t*, spn_dag_action_t*, void*, spn_dag_env_t*, sp_mem_t, sp_da(spn_dag_obs_t)*);

typedef struct {
  u32 index;
  u32 graph;
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
  spn_path_t path;
  spn_path_t materialized;
  spn_dag_digest_t digest;
  spn_dag_id_t producer;
  sp_da(spn_dag_id_t) consumers;
} spn_dag_artifact_t;

typedef enum {
  SPN_DAG_ACTION_STATIC,
  SPN_DAG_ACTION_DISCOVERED,
  SPN_DAG_ACTION_UNCACHEABLE,
} spn_dag_action_kind_t;

struct spn_dag_action_t {
  spn_dag_id_t id;
  spn_dag_action_kind_t kind;
  spn_dag_digest_t identity;
  spn_dag_exec_fn_t execute;
  void* user_data;
  sp_da(spn_dag_id_t) consumes;
  sp_da(spn_dag_id_t) produces;
  bool wrote;
};

typedef struct {
  spn_dag_action_kind_t kind;
  spn_dag_digest_t identity;
  spn_dag_exec_fn_t execute;
  void* user_data;
} spn_dag_action_config_t;

struct spn_dag_t {
  u32 id;
  sp_mem_arena_t* arena;
  sp_mem_t mem;
  sp_mutex_t mutex;
  const spn_path_roots_t* roots;
  sp_da(spn_dag_artifact_t) artifacts;
  sp_da(spn_dag_action_t) actions;
  sp_ht(spn_path_t, spn_dag_id_t) paths;
};

typedef struct {
  sp_atomic_u32_t hashed_files;
  sp_atomic_u64_t hashed_bytes;
  sp_atomic_u32_t stats;
  sp_atomic_u32_t obs_rows;
  sp_atomic_u32_t cache_reads;
  sp_atomic_u32_t cache_writes;
} spn_dag_stats_t;

typedef struct {
  sp_sys_timespec_t fence;
  sp_str_t dir;
} spn_dag_stamp_t;

typedef struct {
  sp_mem_arena_t* arena;
  sp_mem_t mem;
  sp_mutex_t mutex;
  const spn_path_roots_t* roots;
  sp_ht(spn_dag_file_id_t, spn_dag_file_meta_t) entries;
  sp_ht(spn_path_t, sp_sys_file_meta_t) metadata;
  sp_ht(spn_path_t, spn_dag_file_meta_t) hints;
  sp_ht(sp_str_t, sp_str_t) canonical;
  bool hints_dirty;
  spn_dag_stamp_t stamp;
  spn_dag_stats_t* stats;
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
  sp_mutex_t mutex;
  sp_str_t dir;
  sp_ht(spn_dag_digest_t, spn_dag_action_entry_t) entries;
  spn_dag_stats_t* stats;
} spn_dag_action_cache_t;

typedef struct {
  sp_da(spn_dag_obs_t) obs;
} spn_dag_pathset_t;

typedef struct {
  sp_mem_arena_t* arena;
  sp_mem_t mem;
  sp_mutex_t mutex;
  sp_str_t dir;
  sp_ht(spn_dag_digest_t, spn_dag_pathset_t) entries;
  spn_dag_stats_t* stats;
} spn_dag_obs_table_t;

typedef enum {
  SPN_DAG_STORE_MEM,
  SPN_DAG_STORE_FILESYSTEM,
} spn_dag_store_kind_t;

typedef struct {
  spn_dag_store_kind_t kind;
  sp_mem_t mem;
  const spn_path_roots_t* roots;
  spn_path_t dir;
} spn_dag_store_config_t;

typedef struct {
  spn_dag_store_kind_t kind;
  sp_mem_arena_t* arena;
  sp_mem_t mem;
  const spn_path_roots_t* roots;
  spn_path_t dir;
  sp_mutex_t mutex;
  sp_ht(spn_dag_digest_t, sp_mem_slice_t) blobs;
  spn_dag_stats_t* stats;
} spn_dag_store_t;

typedef struct {
  sp_atomic_s32_t total;
  sp_atomic_s32_t completed;
  sp_atomic_s32_t hits;
  sp_atomic_s32_t misses;
} spn_dag_progress_t;

typedef enum {
  SPN_DAG_TRACE_KEY,
  SPN_DAG_TRACE_DISCOVERY,
  SPN_DAG_TRACE_RESOLVE,
  SPN_DAG_TRACE_STRONG,
  SPN_DAG_TRACE_CACHE,
  SPN_DAG_TRACE_EXECUTE,
  SPN_DAG_TRACE_COMMIT,
  SPN_DAG_TRACE_DEFER,
  SPN_DAG_TRACE_REQUEUE,
  SPN_DAG_TRACE_SETTLE,
} spn_dag_trace_kind_t;

typedef struct {
  spn_dag_trace_kind_t kind;
  spn_dag_id_t action;
  spn_dag_digest_t key;
  spn_dag_id_t producer;
  bool present;
  bool hit;
} spn_dag_trace_event_t;

SP_TYPEDEF_FN(void, spn_dag_trace_fn_t, const spn_dag_trace_event_t*, void*);

typedef struct {
  spn_err_t err;
  spn_dag_id_t action;
  sp_str_t path;
} spn_dag_diag_t;

struct spn_dag_env_t {
  spn_dag_file_cache_t* files;
  spn_dag_action_cache_t* cache;
  spn_dag_store_t* store;
  spn_dag_obs_table_t* discovery;
  spn_dag_stats_t* stats;
  spn_dag_progress_t* progress;
  spn_wake_t* wake;
  sp_atomic_s32_t* cancel;
  spn_dag_trace_fn_t trace;
  void* trace_data;
  spn_path_t scratch;
  spn_dag_diag_t diag;
};

#endif
