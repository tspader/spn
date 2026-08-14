#include "sp.h"
#include "sp/macro.h"

#include "ctx/ctx.h"
#include "ctx/types.h"
#include "error/error.h"
#include "spn/errors.h"
#include "event/types.h"
#include "core/types.h"
#include "index/types.h"
#include "spn/host.h"

#include "event/event.h"
#include "external/wasm/wasm.h"
#include "index/index.h"
#include "op/op.h"
#include "thread_pool/thread_pool.h"

typedef struct {
  spn_op_t* op;
  spn_index_info_t* index;
  bool force;
  spn_err_t err;
} job_t;

static void sync_index_node(void* data) {
  job_t* job = (job_t*)data;
  if (spn_op_cancelled(job->op)) {
    return;
  }
  job->err = spn_index_sync(job->index, job->force);
}

static spn_err_t sync_indexes(spn_op_t* op) {
  spn_ctx_t* ctx = op->ctx;
  spn_sync_request_t request = op->request.indexes;
  if (!sp_str_empty(request.only) && !spn_find_index(ctx, request.only)) {
    return spn_err_emit(ctx, (spn_err_union_t) {
      .kind = SPN_ERR_INDEX_UNKNOWN,
      .index = { .name = request.only },
    });
  }

  sp_da(job_t*) jobs = sp_da_new(ctx->mem, job_t*);

  sp_da_for(ctx->indexes, it) {
    spn_index_info_t* index = &ctx->indexes[it];

    if (!sp_str_empty(request.only) && !sp_str_equal(index->name, request.only)) {
      continue;
    }

    if (request.force || spn_index_needs_fetch(index)) {
      spn_event_buffer_push(ctx->events, (spn_event_t) {
        .kind = SPN_EVENT_SYNC,
        .sync = {
          .name = index->name,
          .url = spn_index_source(index),
        }});
    }

    job_t* job = sp_alloc_type(ctx->mem, job_t);
    *job = (job_t) {
      .op = op,
      .index = index,
      .force = request.force,
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

  if (spn_op_cancelled(op)) {
    return SPN_ERR_CANCELLED;
  }

  sp_da_for(jobs, it) {
    job_t* job = jobs[it];
    if (job->err == SPN_OK) {
      continue;
    }

    if (!job->force && sp_fs_exists(job->index->location)) {
      spn_event_buffer_push(ctx->events, (spn_event_t) {
        .kind = SPN_EVENT_SYNC_STALE,
        .sync = {
          .name = job->index->name,
          .url = spn_index_source(job->index),
        }});
      continue;
    }

    return spn_err_emit(ctx, (spn_err_union_t) {
      .kind = SPN_ERR_INDEX_SYNC,
      .index = {
        .name = job->index->name,
        .url = spn_index_source(job->index),
      }});
  }

  return SPN_OK;
}

spn_err_t spn_op_sync_indexes(spn_op_t* op) {
  return sync_indexes(op);
}

spn_op_t* spn_sync_indexes(spn_ctx_t* ctx, spn_sync_request_t request) {
  spn_op_t* op = spn_op_new(ctx, SP_NULLPTR, SPN_OP_SYNC_INDEXES);
  op->request.indexes = (spn_sync_request_t) {
    .force = request.force,
    .only = sp_str_copy(ctx->heap, request.only),
  };
  spn_op_submit(op);
  return op;
}
