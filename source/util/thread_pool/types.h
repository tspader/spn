#ifndef SPN_THREAD_POOL_TYPES_H
#define SPN_THREAD_POOL_TYPES_H

#include "sp.h"

SP_TYPEDEF_FN(void, spn_thread_pool_job_fn_t, void* data);

typedef struct {
  spn_thread_pool_job_fn_t fn;
  void* data;
} spn_thread_pool_job_t;

typedef struct spn_thread_pool_executor_t spn_thread_pool_executor_t;
struct spn_thread_pool_executor_t {
  void (*submit)(spn_thread_pool_executor_t* ex, spn_thread_pool_job_t job);
  spn_thread_pool_job_t (*poll)(spn_thread_pool_executor_t* ex);
  spn_thread_pool_job_t (*try_poll)(spn_thread_pool_executor_t* ex);
};

typedef struct {
  u32 workers;
  void (*on_worker_exit)(void);
} spn_thread_pool_config_t;

typedef struct {
  spn_thread_pool_executor_t executor;
  sp_mem_arena_t* arena;
  sp_mutex_t mutex;
  struct {
    sp_cv_t submitted;
    sp_cv_t completed;
  } signal;
  struct {
    u32 submitted;
    u32 completed;
  } count;
  sp_da(spn_thread_pool_job_t) queue;
  sp_da(spn_thread_pool_job_t) done;
  sp_da(sp_thread_t) workers;
  void (*on_worker_exit)(void);
  bool shutdown;
} spn_thread_pool_t;

#endif
