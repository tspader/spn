#include "thread_pool/thread_pool.h"

void spn_thread_pool_submit(spn_thread_pool_executor_t* ex, spn_thread_pool_job_t job) {
  ex->submit(ex, job);
}

spn_thread_pool_job_t spn_thread_pool_poll(spn_thread_pool_executor_t* ex) {
  return ex->poll(ex);
}

spn_thread_pool_job_t spn_thread_pool_try_poll(spn_thread_pool_executor_t* ex) {
  return ex->try_poll(ex);
}

bool spn_thread_pool_step(spn_thread_pool_t* pool) {
  sp_mutex_lock(&pool->mutex);
  if (sp_da_empty(pool->queue)) {
    sp_mutex_unlock(&pool->mutex);
    return false;
  }
  spn_thread_pool_job_t job = *sp_da_back(pool->queue);
  sp_da_pop(pool->queue);
  sp_mutex_unlock(&pool->mutex);

  job.fn(job.data);

  sp_mutex_lock(&pool->mutex);
  sp_da_push(pool->done, job);
  pool->count.completed++;
  sp_mutex_unlock(&pool->mutex);
  sp_cv_notify_all(&pool->signal.completed);
  return true;
}

static s32 pool_worker(void* data) {
  spn_thread_pool_t* pool = (spn_thread_pool_t*)data;

  while (true) {
    sp_mutex_lock(&pool->mutex);
    while (!pool->shutdown && sp_da_empty(pool->queue)) {
      sp_cv_wait(&pool->signal.submitted, &pool->mutex);
    }
    bool shutdown = pool->shutdown;
    sp_mutex_unlock(&pool->mutex);

    if (shutdown) {
      if (pool->on_worker_exit) {
        pool->on_worker_exit();
      }
      return 0;
    }

    spn_thread_pool_step(pool);
  }
}

static void pool_submit(spn_thread_pool_executor_t* ex, spn_thread_pool_job_t job) {
  spn_thread_pool_t* pool = (spn_thread_pool_t*)ex;
  sp_mutex_lock(&pool->mutex);
  sp_da_push(pool->queue, job);
  pool->count.submitted++;
  sp_mutex_unlock(&pool->mutex);
  sp_cv_notify_one(&pool->signal.submitted);
}

static spn_thread_pool_job_t pool_poll(spn_thread_pool_executor_t* ex) {
  spn_thread_pool_t* pool = (spn_thread_pool_t*)ex;

  while (true) {
    sp_mutex_lock(&pool->mutex);
    while (!sp_da_empty(pool->workers) && sp_da_empty(pool->done)) {
      sp_cv_wait(&pool->signal.completed, &pool->mutex);
    }
    if (!sp_da_empty(pool->done)) {
      spn_thread_pool_job_t job = *sp_da_back(pool->done);
      sp_da_pop(pool->done);
      sp_mutex_unlock(&pool->mutex);
      return job;
    }
    sp_assert(!sp_da_empty(pool->queue));
    sp_mutex_unlock(&pool->mutex);

    spn_thread_pool_step(pool);
  }
}

static spn_thread_pool_job_t pool_try_poll(spn_thread_pool_executor_t* ex) {
  spn_thread_pool_t* pool = (spn_thread_pool_t*)ex;
  spn_thread_pool_job_t job = sp_zero;
  sp_mutex_lock(&pool->mutex);
  if (!sp_da_empty(pool->done)) {
    job = *sp_da_back(pool->done);
    sp_da_pop(pool->done);
  }
  sp_mutex_unlock(&pool->mutex);
  return job;
}

void spn_thread_pool_init(spn_thread_pool_t* pool, sp_mem_t mem, spn_thread_pool_config_t config) {
  *pool = (spn_thread_pool_t) {
    .executor = {
      .submit = pool_submit,
      .poll = pool_poll,
      .try_poll = pool_try_poll,
    },
    .arena = sp_mem_arena_new(mem),
    .on_worker_exit = config.on_worker_exit,
  };

  sp_mem_t a = sp_mem_arena_as_allocator(pool->arena);
  sp_mutex_init(&pool->mutex, SP_MUTEX_PLAIN);
  sp_cv_init(&pool->signal.submitted);
  sp_cv_init(&pool->signal.completed);
  sp_da_init(a, pool->queue);
  sp_da_init(a, pool->done);
  sp_da_init(a, pool->workers);

  sp_for(it, config.workers) {
    sp_thread_t thread = sp_zero;
    sp_thread_init(&thread, pool_worker, pool);
    sp_da_push(pool->workers, thread);
  }
}

void spn_thread_pool_deinit(spn_thread_pool_t* pool) {
  sp_mutex_lock(&pool->mutex);
  sp_assert(sp_da_empty(pool->queue));
  pool->shutdown = true;
  sp_mutex_unlock(&pool->mutex);
  sp_cv_notify_all(&pool->signal.submitted);

  sp_da_for(pool->workers, it) {
    sp_thread_join(&pool->workers[it]);
  }

  sp_cv_destroy(&pool->signal.submitted);
  sp_cv_destroy(&pool->signal.completed);
  sp_mutex_destroy(&pool->mutex);
  sp_mem_arena_destroy(pool->arena);
}

void spn_thread_pool_wait(spn_thread_pool_t* pool) {
  sp_mutex_lock(&pool->mutex);
  while (pool->count.completed != pool->count.submitted) {
    sp_cv_wait(&pool->signal.completed, &pool->mutex);
  }
  sp_mutex_unlock(&pool->mutex);
}

u32 spn_thread_pool_pending(spn_thread_pool_t* pool) {
  sp_mutex_lock(&pool->mutex);
  u32 pending = pool->count.submitted - pool->count.completed;
  sp_mutex_unlock(&pool->mutex);
  return pending;
}
