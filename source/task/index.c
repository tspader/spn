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

static void sync_index_node(void* data) {
  spn_sync_index_job_t* job = (spn_sync_index_job_t*)data;
  job->err = spn_index_sync(job->index, job->force);
}

spn_task_step_t spn_task_sync_indexes_init(spn_ctx_t* ctx) {
  sp_da_init(spn.mem, ctx->index_sync.jobs);

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
    sp_da_push(ctx->index_sync.jobs, job);
  }

  spn_thread_pool_t* pool = &ctx->index_sync.pool;
  spn_thread_pool_init(pool, spn.mem, (spn_thread_pool_config_t) {
    .workers = (u32)sp_min(8, sp_da_size(ctx->index_sync.jobs)),
    .on_worker_exit = spn_wasm_thread_exit,
  });

  sp_da_for(ctx->index_sync.jobs, it) {
    spn_thread_pool_submit(&pool->executor, (spn_thread_pool_job_t) { .fn = sync_index_node, .data = ctx->index_sync.jobs[it] });
  }

  return spn_task_continue();
}

spn_task_step_t spn_task_sync_indexes_update(spn_ctx_t* ctx) {
  if (spn_thread_pool_pending(&ctx->index_sync.pool)) {
    return spn_task_continue();
  }

  spn_thread_pool_deinit(&ctx->index_sync.pool);

  sp_da_for(ctx->index_sync.jobs, it) {
    spn_sync_index_job_t* job = ctx->index_sync.jobs[it];
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
