#include "sp.h"
#include "sp/macro.h"

#include "ctx/ctx.h"
#include "ctx/types.h"
#include "error/types.h"
#include "event/types.h"
#include "forward/types.h"
#include "index/types.h"
#include "op/op.h"
#include "spn.h"

#include "event/event.h"
#include "external/wasm/wasm.h"
#include "index/index.h"
#include "thread_pool/thread_pool.h"

typedef struct {
  spn_index_info_t* index;
  bool force;
  spn_err_t err;
} job_t;

static void sync_index_node(void* data) {
  job_t* job = (job_t*)data;
  if (spn_ctx_cancelled(&spn)) {
    return;
  }
  job->err = spn_index_sync(job->index, job->force);
}

spn_err_union_t spn_op_sync_indexes(spn_ctx_t* ctx, spn_index_refresh_t refresh) {
  sp_da(job_t*) jobs = sp_da_new(ctx->mem, job_t*);

  sp_da_for(ctx->indexes, it) {
    spn_index_info_t* index = &ctx->indexes[it];

    if (!sp_str_empty(refresh.only) && !sp_str_equal(index->name, refresh.only)) {
      continue;
    }

    if (refresh.force || spn_index_needs_fetch(index)) {
      spn_event_buffer_push(ctx->events, (spn_build_event_t) {
        .kind = SPN_EVENT_SYNC,
        .sync = {
          .name = index->name,
          .url = spn_index_source(index),
        }});
    }

    job_t* job = sp_alloc_type(ctx->mem, job_t);
    *job = (job_t) {
      .index = index,
      .force = refresh.force,
    };
    sp_da_push(jobs, job);
  }

  spn_thread_pool_t pool = sp_zero;
  spn_thread_pool_init(&pool, ctx->mem, (spn_thread_pool_config_t) {
    .workers = (u32)sp_min(8, sp_da_size(jobs)),
    .on_worker_exit = spn_wasm_thread_exit,
  });

  sp_da_for(jobs, it) {
    spn_thread_pool_submit(&pool.executor, (spn_thread_pool_job_t) { .fn = sync_index_node, .data = jobs[it] });
  }

  spn_thread_pool_wait(&pool);
  spn_thread_pool_deinit(&pool);

  if (spn_ctx_cancelled(ctx)) {
    return spn_err_reported(SPN_ERROR);
  }

  sp_da_for(jobs, it) {
    job_t* job = jobs[it];
    if (job->err == SPN_OK) {
      continue;
    }

    if (!job->force && sp_fs_exists(job->index->location)) {
      spn_event_buffer_push(ctx->events, (spn_build_event_t) {
        .kind = SPN_EVENT_SYNC_STALE,
        .sync = {
          .name = job->index->name,
          .url = spn_index_source(job->index),
        }});
      continue;
    }

    return (spn_err_union_t) {
      .kind = SPN_ERR_INDEX_SYNC,
      .index = {
        .name = job->index->name,
        .url = spn_index_source(job->index),
      }};
  }

  return spn_result(SPN_OK);
}
