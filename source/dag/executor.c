#include "dag/dag.h"
#include "dag/types.h"
#include "sp.h"

typedef struct {
  spn_dag_executor_t base;
  spn_dag_job_t job;
  bool full;
} spn_dag_inline_t;

static void inline_submit(spn_dag_executor_t* ex, spn_dag_job_t job) {
  spn_dag_inline_t* inline_ex = (spn_dag_inline_t*)ex;
  sp_assert(!inline_ex->full);
  job.fn(job.data);
  inline_ex->job = job;
  inline_ex->full = true;
}

static spn_dag_job_t inline_poll(spn_dag_executor_t* ex) {
  spn_dag_inline_t* inline_ex = (spn_dag_inline_t*)ex;
  sp_assert(inline_ex->full);
  inline_ex->full = false;
  return inline_ex->job;
}

static spn_dag_job_t inline_try_poll(spn_dag_executor_t* ex) {
  spn_dag_inline_t* inline_ex = (spn_dag_inline_t*)ex;
  if (!inline_ex->full) {
    return (spn_dag_job_t) sp_zero;
  }
  inline_ex->full = false;
  return inline_ex->job;
}

spn_err_t spn_dag_run(spn_dag_t* g, spn_dag_env_t* env) {
  spn_dag_inline_t ex = {
    .base = {
      .submit = inline_submit,
      .poll = inline_poll,
      .try_poll = inline_try_poll,
    },
  };
  return spn_dag_run_executor(g, env, &ex.base);
}

static s32 pool_worker(void* data) {
  spn_dag_pool_t* pool = (spn_dag_pool_t*)data;

  while (true) {
    sp_mutex_lock(&pool->mutex);
    while (!pool->shutdown && sp_da_empty(pool->queue)) {
      sp_cv_wait(&pool->submitted, &pool->mutex);
    }
    if (pool->shutdown) {
      sp_mutex_unlock(&pool->mutex);
      if (pool->on_worker_exit) {
        pool->on_worker_exit();
      }
      return 0;
    }
    spn_dag_job_t job = *sp_da_back(pool->queue);
    sp_da_pop(pool->queue);
    sp_mutex_unlock(&pool->mutex);

    job.fn(job.data);

    sp_mutex_lock(&pool->mutex);
    sp_da_push(pool->done, job);
    sp_mutex_unlock(&pool->mutex);
    sp_cv_notify_all(&pool->completed);
  }
}

static void pool_submit(spn_dag_executor_t* ex, spn_dag_job_t job) {
  spn_dag_pool_t* pool = (spn_dag_pool_t*)ex;
  sp_mutex_lock(&pool->mutex);
  sp_da_push(pool->queue, job);
  sp_mutex_unlock(&pool->mutex);
  sp_cv_notify_one(&pool->submitted);
}

static spn_dag_job_t pool_poll(spn_dag_executor_t* ex) {
  spn_dag_pool_t* pool = (spn_dag_pool_t*)ex;
  sp_mutex_lock(&pool->mutex);
  while (sp_da_empty(pool->done)) {
    sp_cv_wait(&pool->completed, &pool->mutex);
  }
  spn_dag_job_t job = *sp_da_back(pool->done);
  sp_da_pop(pool->done);
  sp_mutex_unlock(&pool->mutex);
  return job;
}

static spn_dag_job_t pool_try_poll(spn_dag_executor_t* ex) {
  spn_dag_pool_t* pool = (spn_dag_pool_t*)ex;
  spn_dag_job_t job = sp_zero;
  sp_mutex_lock(&pool->mutex);
  if (!sp_da_empty(pool->done)) {
    job = *sp_da_back(pool->done);
    sp_da_pop(pool->done);
  }
  sp_mutex_unlock(&pool->mutex);
  return job;
}

void spn_dag_pool_init(spn_dag_pool_t* pool, sp_mem_t mem, spn_dag_pool_config_t config) {
  sp_assert(config.workers);

  pool->executor = (spn_dag_executor_t) {
    .submit = pool_submit,
    .poll = pool_poll,
    .try_poll = pool_try_poll,
  };
  pool->arena = sp_mem_arena_new(mem);
  pool->mem = sp_mem_arena_as_allocator(pool->arena);
  sp_mutex_init(&pool->mutex, SP_MUTEX_PLAIN);
  sp_cv_init(&pool->submitted);
  sp_cv_init(&pool->completed);
  sp_da_init(pool->mem, pool->queue);
  sp_da_init(pool->mem, pool->done);
  sp_da_init(pool->mem, pool->workers);
  pool->on_worker_exit = config.on_worker_exit;
  pool->shutdown = false;

  sp_for(it, config.workers) {
    sp_thread_t thread = sp_zero;
    sp_thread_init(&thread, pool_worker, pool);
    sp_da_push(pool->workers, thread);
  }
}

void spn_dag_pool_deinit(spn_dag_pool_t* pool) {
  sp_mutex_lock(&pool->mutex);
  sp_assert(sp_da_empty(pool->queue));
  sp_assert(sp_da_empty(pool->done));
  pool->shutdown = true;
  sp_mutex_unlock(&pool->mutex);
  sp_cv_notify_all(&pool->submitted);

  sp_da_for(pool->workers, it) {
    sp_thread_join(&pool->workers[it]);
  }

  sp_cv_destroy(&pool->submitted);
  sp_cv_destroy(&pool->completed);
  sp_mutex_destroy(&pool->mutex);
  sp_mem_arena_destroy(pool->arena);
}
