#ifndef SPN_OP_BUILD_DAG_H
#define SPN_OP_BUILD_DAG_H

#include "dag/dag.h"
#include "forward/types.h"
#include "thread_pool/types.h"
#include "unit/types.h"

typedef struct {
  spn_dag_id_t action;
  spn_dag_id_t object;
} spn_dag_object_ids_t;

typedef struct {
  spn_dag_id_t action;
  spn_dag_id_t output;
  struct {
    spn_dag_id_t action;
    spn_dag_id_t object;
    spn_dag_id_t header;
  } embed;
} spn_dag_target_ids_t;

typedef struct {
  spn_dag_id_t action;
  spn_dag_id_t stamp;
  spn_dag_id_t tree;
  sp_da(spn_dag_id_t) user_outputs;
  sp_da(spn_dag_id_t) user_actions;
} spn_dag_pkg_ids_t;

struct spn_dag_build_t {
  spn_session_t* session;
  sp_mem_t mem;
  spn_dag_t* graph;

  // This graph's ids for session-lifetime units, keyed by unit pointer.
  // Entries are inserted whole once a unit is fully added, so presence means
  // every id inside is valid. Only the thread constructing the graph may
  // touch these: sp_ht mutates scratch state even on lookup, so executors
  // take ids through action->produces or their ctx instead.
  struct {
    sp_ht(spn_pkg_unit_t*, spn_dag_pkg_ids_t) packages;
    sp_ht(spn_target_unit_t*, spn_dag_target_ids_t) targets;
    sp_ht(spn_compile_unit_t*, spn_dag_object_ids_t) objects;
  } ids;
  spn_dag_roots_t roots;
  spn_dag_file_cache_t files;
  spn_dag_action_cache_t actions;
  spn_dag_obs_table_t discovery;
  spn_dag_obs_table_t memos;
  spn_dag_store_t store;
  spn_thread_pool_t pool;
  spn_dag_env_t env;
  spn_dag_progress_t progress;
  spn_err_t result;
  sp_tm_timer_t timer;
};

spn_dag_build_t* spn_dag_build_new(spn_session_t* session);
spn_err_t        spn_dag_build_run(spn_dag_build_t* b, u32 workers);
spn_err_union_t  spn_dag_build_add_target(spn_dag_build_t* b, spn_target_unit_t* target);
spn_err_t        spn_build_publish_copies(spn_pkg_unit_t* unit, sp_str_t root, bool strict, sp_da(spn_dag_obs_t)* obs);

#endif
