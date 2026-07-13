#ifndef SPN_DAG_DAG_H
#define SPN_DAG_DAG_H

#include "sp.h"
#include "spn.h"
#include "dag/types.h"

spn_dag_t*          spn_dag_new(sp_mem_t mem);
spn_dag_artifact_t* spn_dag_find_artifact(spn_dag_t* g, spn_dag_id_t id);
spn_dag_action_t*   spn_dag_find_action(spn_dag_t* g, spn_dag_id_t id);
spn_dag_id_t        spn_dag_add_value(spn_dag_t* g, const void* data, u64 len);
spn_dag_id_t        spn_dag_add_file(spn_dag_t* g, sp_str_t path);
spn_dag_id_t        spn_dag_find_file(spn_dag_t* g, sp_str_t path);
spn_dag_id_t        spn_dag_add_action(spn_dag_t* g, spn_dag_action_config_t config);
void                spn_dag_action_add_input(spn_dag_t* g, spn_dag_id_t action, spn_dag_id_t artifact);
spn_err_t           spn_dag_action_add_output(spn_dag_t* g, spn_dag_id_t action, spn_dag_id_t artifact);
spn_dag_digest_t    spn_dag_action_key(spn_dag_t* g, spn_dag_id_t action);
spn_dag_digest_t    spn_dag_strong_key(spn_dag_digest_t prelim, const spn_dag_obs_t* obs, const spn_dag_digest_t* digests, u32 count);

spn_dag_digest_t    spn_dag_digest(const void* data, u64 len);
bool                spn_dag_digest_equal(spn_dag_digest_t a, spn_dag_digest_t b);
bool                spn_dag_digest_valid(spn_dag_digest_t digest);
sp_str_t            spn_dag_digest_hex(sp_mem_t mem, spn_dag_digest_t digest);

void                spn_dag_file_cache_init(spn_dag_file_cache_t* c, sp_mem_t mem);
void                spn_dag_file_cache_refresh(spn_dag_file_cache_t* c);
void                spn_dag_file_cache_invalidate(spn_dag_file_cache_t* c, sp_str_t path);
spn_err_t           spn_dag_get_file_meta(spn_dag_file_cache_t* c, sp_str_t path, sp_sys_file_meta_t* meta);
spn_err_t           spn_dag_get_file_digest(spn_dag_file_cache_t* c, sp_str_t path, spn_dag_digest_t* digest);
spn_err_t           spn_dag_file_cache_save(spn_dag_file_cache_t* c, sp_str_t path);
spn_err_t           spn_dag_file_cache_load(spn_dag_file_cache_t* c, sp_str_t path);

void                     spn_dag_action_cache_init(spn_dag_action_cache_t* c, sp_mem_t mem);
const spn_dag_action_entry_t* spn_dag_action_cache_get(spn_dag_action_cache_t* c, spn_dag_digest_t key);
void                     spn_dag_action_cache_put(spn_dag_action_cache_t* c, spn_dag_digest_t key, const spn_dag_action_output_t* outputs, u32 count);
bool                     spn_dag_action_cache_remove(spn_dag_action_cache_t* c, spn_dag_digest_t key);
spn_err_t                spn_dag_action_cache_save(spn_dag_action_cache_t* c, sp_str_t path);
spn_err_t                spn_dag_action_cache_load(spn_dag_action_cache_t* c, sp_str_t path);
spn_err_t                spn_dag_execute(spn_dag_t* g, spn_dag_id_t action, spn_dag_file_cache_t* files, spn_dag_action_cache_t* cache, spn_dag_store_t* store);

void                        spn_dag_discovery_init(spn_dag_discovery_t* d, sp_mem_t mem);
const spn_dag_pathset_t*    spn_dag_discovery_get(spn_dag_discovery_t* d, spn_dag_digest_t prelim);
void                        spn_dag_discovery_put(spn_dag_discovery_t* d, spn_dag_digest_t prelim, const spn_dag_obs_t* obs, u32 count);
spn_err_t                   spn_dag_discovery_save(spn_dag_discovery_t* d, sp_str_t path);
spn_err_t                   spn_dag_discovery_load(spn_dag_discovery_t* d, sp_str_t path);
spn_err_t                   spn_dag_execute_discovered(spn_dag_t* g, spn_dag_id_t action, spn_dag_file_cache_t* files, spn_dag_action_cache_t* cache, spn_dag_store_t* store, spn_dag_discovery_t* discovery);

spn_err_t                   spn_dag_run(spn_dag_t* g, spn_dag_file_cache_t* files, spn_dag_action_cache_t* cache, spn_dag_store_t* store, spn_dag_discovery_t* discovery);

spn_err_t                   spn_cc_deps_parse(sp_mem_t mem, sp_str_t content, sp_da(sp_str_t)* out);
spn_err_t                   spn_cc_discover(spn_dag_action_t* action, void* user_data, sp_mem_t mem, sp_da(spn_dag_obs_t)* out);

void                spn_dag_store_init(spn_dag_store_t* store, spn_dag_store_config_t config);
spn_err_t           spn_dag_put(spn_dag_store_t* store, const void* data, u64 len, spn_dag_digest_t* digest);
spn_err_t           spn_dag_store_put_file(spn_dag_store_t* store, sp_str_t path, spn_dag_digest_t* digest);
bool                spn_dag_store_has(spn_dag_store_t* store, spn_dag_digest_t digest);
spn_err_t           spn_dag_store_get(spn_dag_store_t* store, spn_dag_digest_t digest, sp_mem_t mem, sp_mem_slice_t* data);
spn_err_t           spn_dag_store_materialize(spn_dag_store_t* store, spn_dag_digest_t digest, sp_str_t path);

#endif
