#include "ctx/types.h"
#include "event/types.h"

#include "app/app.h"
#include "err.h"
#include "graph/graph.h"
#include "event/event.h"
#include "session/session.h"
#include "task/build/graph.h"

spn_task_result_t spn_task_prepare_build_graph(spn_app_t* app) {
  spn_session_t* session = &app->session;
  spn_build_graph_t* graph = &session->build.graph;
  spn_pkg_unit_t* root = spn_session_find_root(session);

  graph->error.some = SP_OPT_NONE;
  spn_err_union_t err = prepare_build_graph(app);
  if (err.kind != SPN_OK) {
    spn_event_buffer_push_ctx(spn.events, &root->ctx, (spn_build_event_t) {
      .kind = SPN_EVENT_PREPARE_BUILD_GRAPH_FAILED,
      .err = err,
    });
    return SPN_TASK_ERROR;
  }

  return SPN_TASK_DONE;
}


void spn_task_init_build_graph(spn_app_t* app) {
  spn_session_t* b = &app->session;
  u32 num_packages = 0;
  sp_om_for(b->units.packages, it) {
    num_packages++;
  }

  spn_event_buffer_push_ctx(spn.events, &spn_session_find_root(b)->ctx, (spn_build_event_t) {
    .kind = SPN_EVENT_INIT_BUILD_GRAPH,
    .graph_init = {
      .profile = b->profile->name,
      .force = app->config.force,
      .packages = num_packages,
    }
  });

  b->build.dirty = app->config.force ?
    spn_bg_compute_forced_dirty(&b->build.graph) :
    spn_bg_compute_dirty(&b->build.graph);

  spn_pkg_unit_t* root = spn_session_find_root(b);
  u32 dirty_files = sp_ht_size(b->build.dirty->files);
  u32 dirty_cmds = sp_ht_size(b->build.dirty->commands);

  spn_event_buffer_push_ctx(spn.events, &root->ctx, (spn_build_event_t) {
    .kind = SPN_EVENT_DIRTY_SUMMARY,
    .dirty_summary = {
      .total_commands = sp_da_size(b->build.graph.commands),
      .dirty_commands = dirty_cmds,
      .total_files = sp_da_size(b->build.graph.files),
      .dirty_files = dirty_files,
      .forced = app->config.force,
    }
  });

  b->build.executor = spn_bg_executor_new(&b->build.graph, b->build.dirty, (spn_bg_executor_config_t) {
    .num_threads = 8,
    .enable_logging = false
  });

  spn_bg_executor_run(app->session.build.executor);
}

spn_task_result_t spn_task_run_build_graph(spn_app_t* app) {
  spn_session_t* b = &app->session;
  spn_bg_ctx_t* build = &b->build;

  if (sp_atomic_s32_get(&build->executor->shutdown)) {
    spn_bg_executor_join(build->executor);

    // sp_tui_home();
    // sp_tui_clear_line();

    sp_opt(spn_bg_exec_error_t) error = SP_ZERO_INITIALIZE();
    if (sp_da_size(build->executor->errors)) {
      sp_opt_set(error, build->executor->errors[0]);
    }

    spn_pkg_unit_t* root = spn_session_find_root(b);
    u32 num_errors = sp_da_size(build->executor->errors);
    u32 dirty_cmds = sp_ht_size(b->build.dirty->commands);

    switch (error.some) {
      case SP_OPT_SOME: {
        sp_str_t first_error = sp_str_lit("");
        spn_bg_cmd_t* err_cmd = spn_bg_find_command(&b->build.graph, error.value.cmd_id);
        if (err_cmd) {
          first_error = err_cmd->tag;
        }

        spn_event_buffer_push_ctx(spn.events, &root->ctx, (spn_build_event_t) {
          .kind = SPN_EVENT_BUILD_FAILED,
          .build_failed = {
            .profile = app->session.profile->name,
            .time = b->build.executor->elapsed,
            .num_errors = num_errors,
            .first_error = first_error,
          }
        });

        spn_event_buffer_push_ctx(spn.events, &root->ctx, (spn_build_event_t) {
          .kind = SPN_EVENT_BUILD_SUMMARY,
          .build_summary = {
            .success = false,
            .num_dirty = dirty_cmds,
            .total_commands = sp_da_size(b->build.graph.commands),
            .time = b->build.executor->elapsed,
            .profile = app->session.profile->name,
          }
        });

        return SPN_TASK_ERROR;
      }
      case SP_OPT_NONE: {
        if (!app->lock.some) {
          spn_app_update_lock_file(app);
        }

        spn_event_buffer_push_ctx(spn.events, &root->ctx, (spn_build_event_t) {
          .kind = SPN_EVENT_BUILD_PASSED,
          .build.passed = {
            .profile = app->session.profile,
            .time = b->build.executor->elapsed
          }
        });

        spn_event_buffer_push_ctx(spn.events, &root->ctx, (spn_build_event_t) {
          .kind = SPN_EVENT_BUILD_SUMMARY,
          .build_summary = {
            .success = true,
            .num_dirty = dirty_cmds,
            .total_commands = sp_da_size(b->build.graph.commands),
            .time = b->build.executor->elapsed,
            .profile = app->session.profile->name,
          }
        });

        return SPN_TASK_DONE;
      }
    }

    return SPN_TASK_DONE;
  }

  return SPN_TASK_CONTINUE;
}
