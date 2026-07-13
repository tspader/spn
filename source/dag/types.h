#ifndef SPN_DAG_TYPES_H
#define SPN_DAG_TYPES_H

#include "sp.h"

typedef struct spn_dag_action_t spn_dag_action_t;

typedef struct {
  u8 bytes [32];
} spn_dag_digest_t;

SP_TYPEDEF_FN(s32, spn_dag_exec_fn_t, spn_dag_action_t*, void*);

typedef struct {
  u32 index;
  u32 occupied;
} spn_dag_id_t;

typedef enum {
  SPN_DAG_ARTIFACT_KIND_VALUE,
  SPN_DAG_ARTIFACT_KIND_FILE,
} spn_dag_artifact_kind_t;

typedef struct {
  spn_dag_id_t id;
  spn_dag_artifact_kind_t kind;
  sp_str_t path;
  spn_dag_digest_t digest;
  spn_dag_id_t producer;
  sp_da(spn_dag_id_t) consumers;
} spn_dag_artifact_t;

struct spn_dag_action_t {
  spn_dag_id_t id;
  spn_dag_digest_t salt;
  spn_dag_exec_fn_t execute;
  void* user_data;
  sp_da(spn_dag_id_t) consumes;
  sp_da(spn_dag_id_t) produces;
};

typedef struct {
  spn_dag_digest_t salt;
  spn_dag_exec_fn_t execute;
  void* user_data;
} spn_dag_action_config_t;

typedef struct {
  sp_mem_arena_t* arena;
  sp_mem_t mem;
  sp_da(spn_dag_artifact_t) artifacts;
  sp_da(spn_dag_action_t) actions;
} spn_dag_t;

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


typedef struct {
  sp_mem_arena_t* arena;
  sp_mem_t mem;
  sp_ht(spn_dag_file_id_t, spn_dag_file_meta_t) entries;
  sp_ht(sp_str_t, sp_sys_file_meta_t) metadata;
} spn_dag_file_cache_t;

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
  sp_ht(spn_dag_digest_t, sp_mem_slice_t) blobs;
} spn_dag_store_t;

#endif
