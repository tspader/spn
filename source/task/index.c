#include "sp.h"
#include "sp/macro.h"

#include "app/types.h"
#include "ctx/types.h"
#include "error/types.h"
#include "event/types.h"
#include "forward/types.h"
#include "index/types.h"
#include "spn.h"
#include "task/types.h"

#include "event/event.h"
#include "index/index.h"
#include "task/task.h"
#include "external/wasm/wasm.h"

static s32 sync_index_node(spn_bg_cmd_t* cmd, void* user_data) {
  spn_sync_index_job_t* job = (spn_sync_index_job_t*)user_data;
  job->err = spn_index_sync(job->index, job->force);
  return job->err;
}

spn_task_step_t spn_task_sync_indexes_init(spn_app_t* app) {
  spn_build_graph_t* graph = &app->index_sync.bg.graph;
  spn_bg_init(graph, spn.mem);
  sp_da_init(spn.mem, app->index_sync.jobs);

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
          .url = index->url,
        }});
    }

    spn_sync_index_job_t* job = sp_alloc_type(spn.mem, spn_sync_index_job_t);
    job->index = index;
    job->force = force;
    sp_da_push(app->index_sync.jobs, job);

    spn_bg_add_fn(graph, sync_index_node, job);
  }

  app->index_sync.bg.dirty = spn_bg_compute_forced_dirty(graph);
  app->index_sync.bg.executor = spn_bg_executor_new(graph, app->index_sync.bg.dirty, (spn_bg_executor_config_t) {
    .num_threads = 8,
    .on_worker_exit = spn_wasm_thread_exit,
  });
  spn_bg_executor_run(app->index_sync.bg.executor);

  return spn_task_continue();
}

spn_task_step_t spn_task_sync_indexes_update(spn_app_t* app) {
  spn_bg_ctx_t* sync = &app->index_sync.bg;

  if (!sp_atomic_s32_get(&sync->executor->shutdown)) {
    return spn_task_continue();
  }

  spn_bg_executor_join(sync->executor);

  sp_da_for(app->index_sync.jobs, it) {
    spn_sync_index_job_t* job = app->index_sync.jobs[it];
    if (job->err == SPN_OK) {
      continue;
    }

    if (!job->force && sp_fs_exists(job->index->location)) {
      spn_event_buffer_push(spn.events, (spn_build_event_t) {
        .kind = SPN_EVENT_SYNC_STALE,
        .sync = {
          .name = job->index->name,
          .url = job->index->url,
        }});
      continue;
    }

    return spn_task_fail(SPN_ERR_INDEX_SYNC, .index = {
      .name = job->index->name,
      .url = job->index->url,
    });
  }

  return spn_task_done();
}
