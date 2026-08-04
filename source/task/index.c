#include "sp.h"
#include "sp/macro.h"

#include "ctx/types.h"
#include "error/types.h"
#include "event/types.h"
#include "forward/types.h"
#include "index/types.h"
#include "spn.h"
#include "task/types.h"

#include "event/event.h"
#include "index/index.h"
#include "thread_pool/thread_pool.h"
#include "task/task.h"
#include "external/wasm/wasm.h"

typedef struct {
  spn_index_info_t* index;
  bool force;
  spn_err_t err;
} spn_sync_index_job_t;

struct spn_index_sync_op_t {
  spn_thread_pool_t pool;
  sp_da(spn_sync_index_job_t*) jobs;
};

static void sync_index_node(void* data) {
  spn_sync_index_job_t* job = (spn_sync_index_job_t*)data;
  job->err = spn_index_sync(job->index, job->force);
}

spn_task_step_t spn_task_sync_indexes_init(spn_ctx_t* ctx, spn_task_t* task) {
  spn_index_sync_op_t* op = sp_alloc_type(spn.mem, spn_index_sync_op_t);
  task->index_sync = op;
  sp_da_init(spn.mem, op->jobs);

  bool force = spn.cli.index.force;
  sp_str_t only = spn.cli.index.name;

  sp_da_for(spn.indexes, it) {
    spn_index_info_t* index = &spn.indexes[it];

    if (!sp_str_empty(only) && !sp_str_equal(index->name, only)) {
      continue;
    }

    if (force || spn_index_needs_fetch(index)) {
      spn_event_buffer_push(spn.events, (spn_build_event_t) {
        .kind = SPN_EVENT_SYNC,
        .sync = {
          .name = index->name,
          .url = spn_index_source(index),
        }});
    }

    spn_sync_index_job_t* job = sp_alloc_type(spn.mem, spn_sync_index_job_t);
    job->index = index;
    job->force = force;
    sp_da_push(op->jobs, job);
  }

  spn_thread_pool_init(&op->pool, spn.mem, (spn_thread_pool_config_t) {
    .workers = (u32)sp_min(8, sp_da_size(op->jobs)),
    .on_worker_exit = spn_wasm_thread_exit,
  });

  sp_da_for(op->jobs, it) {
    spn_thread_pool_submit(&op->pool.executor, (spn_thread_pool_job_t) { .fn = sync_index_node, .data = op->jobs[it] });
  }

  return spn_task_continue();
}

spn_task_step_t spn_task_sync_indexes_update(spn_ctx_t* ctx, spn_task_t* task) {
  spn_index_sync_op_t* op = task->index_sync;

  if (spn_thread_pool_pending(&op->pool)) {
    return spn_task_continue();
  }

  spn_thread_pool_deinit(&op->pool);

  sp_da_for(op->jobs, it) {
    spn_sync_index_job_t* job = op->jobs[it];
    if (job->err == SPN_OK) {
      continue;
    }

    if (!job->force && sp_fs_exists(job->index->location)) {
      spn_event_buffer_push(spn.events, (spn_build_event_t) {
        .kind = SPN_EVENT_SYNC_STALE,
        .sync = {
          .name = job->index->name,
          .url = spn_index_source(job->index),
        }});
      continue;
    }

    return spn_task_fail(SPN_ERR_INDEX_SYNC, .index = {
      .name = job->index->name,
      .url = spn_index_source(job->index),
    });
  }

  return spn_task_done();
}
