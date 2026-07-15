#ifndef SPN_TASK_BUILD_DAG_H
#define SPN_TASK_BUILD_DAG_H

#include "dag/dag.h"
#include "forward/types.h"
#include "task/types.h"
#include "unit/types.h"

struct spn_dag_build_t {
  spn_session_t* session;
  sp_mem_t mem;
  spn_dag_t* graph;
  spn_dag_file_cache_t files;
  spn_dag_action_cache_t actions;
  spn_dag_discovery_t discovery;
  spn_dag_store_t store;
  spn_dag_pool_t pool;
  spn_dag_env_t env;
  spn_dag_progress_t progress;
  sp_thread_t runner;
  sp_atomic_s32_t done;
  spn_err_t result;
  sp_tm_timer_t timer;
};

spn_task_step_t spn_dag_build_init(spn_app_t* app);
spn_task_step_t spn_dag_build_update(spn_app_t* app);

#endif
