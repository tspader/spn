#include "app.h"
#include "external/git.h"

s32 spn_executor_sync_repo(spn_bg_cmd_t* cmd, void* user_data) {
  spn_pkg_unit_t* build = (spn_pkg_unit_t*)user_data;

  spn_event_buffer_push_ctx(spn.events, &build->ctx, (spn_build_event_t) {
    .kind = SPN_EVENT_SYNC,
    .sync = {
      .url = spn_pkg_get_url(build->ctx.pkg)
    }
  });
  spn_pkg_unit_sync_remote(build);

  sp_str_t message = spn_git_get_commit_message(build->ctx.paths.source, build->metadata.commit);
  message = sp_str_truncate(message, 32, SP_LIT("..."));
  message = sp_str_replace_c8(message, '\n', ' ');
  message = sp_str_replace_c8(message, '{', '['); // @spader @hack
  message = sp_str_replace_c8(message, '}', ']');
  message = sp_str_pad(message, 32);

  spn_event_buffer_push_ctx(spn.events, &build->ctx, (spn_build_event_t) {
    .kind = SPN_EVENT_CHECKOUT,
    .checkout = {
      .commit = spn_intern(build->metadata.commit),
      .version = build->metadata.version,
      .message = spn_intern(message)
    }
  });
  spn_pkg_unit_sync_local(build);

  return SPN_OK;
}

void spn_task_sync_init(spn_app_t* app) {
  spn_session_t* b = &app->session;

  spn_build_graph_t* graph = &b->sync.graph;

  spn_event_buffer_push(spn.events, &spn_session_find_root(b)->ctx, SPN_EVENT_FETCH);

  sp_om_for(b->units.packages, it) {
    spn_pkg_unit_t* dep = sp_om_at(b->units.packages, it);

    if (dep->ctx.pkg->kind != SPN_PACKAGE_KIND_INDEX) {
      continue;
    }

    spn_bg_id_t sync = spn_bg_add_fn(graph, spn_executor_sync_repo, dep);
    spn_bg_cmd_set_metadata(graph, sync, sp_format("sync ({})", SP_FMT_STR(dep->ctx.name)), sp_str_lit(""), SPN_BG_VIZ_DEFAULT);
  }

  if (!sp_da_empty(graph->commands)) {
    b->sync.dirty = spn_bg_compute_forced_dirty(graph);
    b->sync.executor = spn_bg_executor_new(graph, b->sync.dirty, (spn_bg_executor_config_t) {
      .num_threads = 3,
      .enable_logging = false
    });

    spn_bg_executor_run(b->sync.executor);
  }
}

spn_task_result_t spn_task_sync_update(spn_app_t* app) {
  spn_bg_ctx_t* sync = &app->session.sync;

  if (!sync->executor) {
    return SPN_TASK_DONE;
  }

  if (sp_atomic_s32_get(&sync->executor->shutdown)) {
    spn_bg_executor_join(sync->executor);
    return SPN_TASK_DONE;
  }

  return SPN_TASK_CONTINUE;
}
