#ifndef SPN_DAG_TYPES_H
#define SPN_DAG_TYPES_H

#include "sp.h"
#include "spn/core.h"
#include "core/types.h"

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
  sp_str_t path;
  sp_str_t filter;
  spn_dag_file_meta_t meta;
} spn_dag_obs_t;

typedef enum {
  SPN_DAG_ROOT_NONE = 0,
  SPN_DAG_ROOT_PROJECT,
  SPN_DAG_ROOT_STORE,
  SPN_DAG_ROOT_BUILD,
  SPN_DAG_ROOT_CHECKOUT,
  SPN_DAG_ROOT_TOOLCHAIN,
  SPN_DAG_ROOT_TOOLCHAIN_SCRIPT,
  SPN_DAG_ROOT_COUNT,
} spn_dag_root_t;

typedef struct {
  sp_str_t dirs [SPN_DAG_ROOT_COUNT];
} spn_dag_roots_t;

typedef struct {
  spn_dag_root_t root;
  sp_str_t sub;
} spn_dag_prefixed_t;

typedef struct {
  sp_str_t path;
  sp_str_t relative;
} spn_dag_match_t;

SP_TYPEDEF_FN(s32, spn_dag_exec_fn_t, spn_dag_t*, spn_dag_action_t*, void*);
SP_TYPEDEF_FN(spn_err_t, spn_dag_discover_fn_t, spn_dag_t*, spn_dag_action_t*, void*, sp_mem_t, sp_da(spn_dag_obs_t)*);

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
  bool uncacheable;
  sp_da(spn_dag_id_t) consumes;
  sp_da(spn_dag_id_t) produces;
  bool wrote;
};

typedef struct {
  spn_dag_digest_t identity;
  spn_dag_exec_fn_t execute;
  spn_dag_discover_fn_t discover;
  void* user_data;
  bool uncacheable;
} spn_dag_action_config_t;

struct spn_dag_t {
  u32 id;
  sp_mem_arena_t* arena;
  sp_mem_t mem;
  sp_mutex_t mutex;
  sp_da(spn_dag_artifact_t) artifacts;
  sp_da(spn_dag_action_t) actions;
  sp_ht(sp_str_t, spn_dag_id_t) paths;
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
  sp_mem_arena_t* arena;
  sp_mem_t mem;
  sp_mutex_t mutex;
  sp_ht(spn_dag_file_id_t, spn_dag_file_meta_t) entries;
  sp_ht(sp_str_t, sp_sys_file_meta_t) metadata;
  sp_ht(sp_str_t, spn_dag_file_meta_t) hints;
  bool hints_dirty;
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
  const spn_dag_roots_t* roots;
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
  sp_str_t dir;
} spn_dag_store_config_t;

typedef struct {
  spn_dag_store_kind_t kind;
  sp_mem_arena_t* arena;
  sp_mem_t mem;
  sp_str_t dir;
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

typedef struct {
  spn_dag_file_cache_t* files;
  spn_dag_action_cache_t* cache;
  spn_dag_store_t* store;
  spn_dag_obs_table_t* discovery;
  const spn_dag_roots_t* roots;
  spn_dag_stats_t* stats;
  spn_dag_progress_t* progress;
  spn_wake_t* wake;
  sp_atomic_s32_t* cancel;
  spn_dag_trace_fn_t trace;
  void* trace_data;
  sp_str_t scratch;
  spn_dag_diag_t diag;
} spn_dag_env_t;

#endif
