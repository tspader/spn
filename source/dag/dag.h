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
spn_dag_id_t        spn_dag_add_action(spn_dag_t* g, spn_dag_action_config_t config);
void                spn_dag_action_add_input(spn_dag_t* g, spn_dag_id_t action, spn_dag_id_t artifact);
spn_err_t           spn_dag_action_add_output(spn_dag_t* g, spn_dag_id_t action, spn_dag_id_t artifact);
spn_dag_digest_t    spn_dag_action_key(spn_dag_t* g, spn_dag_id_t action);

spn_dag_digest_t    spn_dag_digest(const void* data, u64 len);
bool                spn_dag_digest_equal(spn_dag_digest_t a, spn_dag_digest_t b);
sp_str_t            spn_dag_digest_hex(sp_mem_t mem, spn_dag_digest_t digest);

void                spn_dag_file_cache_init(spn_dag_file_cache_t* c, sp_mem_t mem);
spn_err_t           spn_dag_get_file_meta(spn_dag_file_cache_t* c, sp_str_t path, sp_sys_file_meta_t* meta);
spn_err_t           spn_dag_get_file_digest(spn_dag_file_cache_t* c, sp_str_t path, spn_dag_digest_t* digest);
spn_err_t           spn_dag_file_cache_save(spn_dag_file_cache_t* c, sp_str_t path);
spn_err_t           spn_dag_file_cache_load(spn_dag_file_cache_t* c, sp_str_t path);

void                spn_dag_store_init(spn_dag_store_t* store, spn_dag_store_config_t config);
spn_err_t           spn_dag_put(spn_dag_store_t* store, const void* data, u64 len, spn_dag_digest_t* digest);
spn_err_t           spn_dag_store_put_file(spn_dag_store_t* store, sp_str_t path, spn_dag_digest_t* digest);
bool                spn_dag_store_has(spn_dag_store_t* store, spn_dag_digest_t digest);
spn_err_t           spn_dag_store_get(spn_dag_store_t* store, spn_dag_digest_t digest, sp_mem_t mem, sp_mem_slice_t* data);
spn_err_t           spn_dag_store_materialize(spn_dag_store_t* store, spn_dag_digest_t digest, sp_str_t path);

#endif
