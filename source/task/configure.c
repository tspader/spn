#include "app/types.h"
#include "ctx/types.h"
#include "event/types.h"
#include "graph/types.h"
#include "target/types.h"

#include "event/event.h"
#include "graph/graph.h"
#include "intern.h"
#include "sp/glob.h"
#include "session/session.h"
#include "task.h"
#include "unit/package.h"
#include "unit/types.h"

static bool spn_configure_has_source(sp_da(sp_str_t) source, sp_str_t path) {
  sp_da_for(source, it) {
    if (sp_str_equal(source[it], path)) {
      return true;
    }
  }

  return false;
}

static sp_str_t spn_configure_relative_source(sp_str_t root, sp_str_t path) {
  sp_str_t relative = sp_str_strip_left(path, root);
  relative = sp_str_strip_left(relative, sp_str_lit("/"));
  return relative;
}

static void spn_configure_expand_glob(sp_str_t root, sp_str_t pattern, sp_da(sp_str_t)* source) {
  sp_glob_t* glob = sp_glob_new_str(pattern);
  if (!glob) {
    return;
  }

  sp_da(sp_fs_entry_t) entries = sp_fs_collect_recursive(root);
  sp_da(sp_str_t) matches = SP_NULLPTR;

  sp_da_for(entries, it) {
    sp_fs_entry_t* entry = &entries[it];
    if (!sp_fs_is_file(entry->file_path)) {
      continue;
    }

    sp_str_t relative = spn_configure_relative_source(root, entry->file_path);
    if (!sp_glob_match(glob, relative)) {
      continue;
    }
    if (spn_configure_has_source(matches, relative)) {
      continue;
    }

    sp_da_push(matches, relative);
  }

  sp_dyn_array_sort(matches, sp_str_sort_kernel_alphabetical);

  sp_da_for(matches, it) {
    if (spn_configure_has_source(*source, matches[it])) {
      continue;
    }

    sp_da_push(*source, matches[it]);
  }
}

static sp_da(sp_str_t) spn_configure_collect_source(spn_pkg_unit_t* pkg, spn_target_unit_t* target) {
  sp_da(sp_str_t) source = SP_NULLPTR;

  sp_da_for(target->info->source, it) {
    sp_str_t path = target->info->source[it];
    if (sp_fs_is_glob(path)) {
      spn_configure_expand_glob(pkg->ctx.paths.source, path, &source);
      continue;
    }
    if (spn_configure_has_source(source, path)) {
      continue;
    }

    sp_da_push(source, path);
  }

  return source;
}

s32 download_toolchain(spn_bg_cmd_t* cmd, void* user_data) {
  spn_toolchain_unit_t* unit = (spn_toolchain_unit_t*)user_data;
  spn_session_t* session = unit->session;

  spn_event_buffer_push_ex(spn.events, unit->pkg, &unit->logs, (spn_build_event_t) {
    .kind = SPN_EVENT_TARGET_BUILD
  });

  sp_str_t output = sp_fs_join_path(unit->paths.work, sp_fs_get_name(unit->url));

  // This function runs as part of the configure graph, which is a DAG ordered
  // by dependency traversal. Normally, we'd model checks like this in the structure
  // of the graph. However, the configure graph is only a graph for the ordering
  // properties. We don't actually want to gate its execution on dirtiness, because
  // all of its nodes need to run every time (e.g. calling script::configure())
  //
  // But we ALSO need to have the toolchain before we configure. It's probably best
  // to just download outside of the graph, but I figured that if there was *any*
  // work that could be done while we're downloading, it's worthwhile.
  //
  // I hard gate everything on this node, though, so it's functionally sync.
  if (sp_fs_exists(unit->paths.stamp)) return 0;

  sp_str_t curl = sp_env_get(&session->env, sp_str_lit("SPN_CURL"));
  if (sp_str_empty(curl)) curl = sp_str_lit("curl");
  sp_ps_output_t dl = sp_ps_run((sp_ps_config_t) {
    .command = curl,
    .args = {
      sp_str_lit("-fSL"),
      sp_str_lit("-o"), output,
      unit->url
    }
  });
  if (dl.status.exit_code) return SPN_ERROR;

  sp_ps_output_t extract = sp_ps_run((sp_ps_config_t) {
    .command = sp_str_lit("tar"),
    .args = {
      sp_str_lit("xf"), output,
      sp_str_lit("--strip-components=1"),
      sp_str_lit("-C"), unit->paths.store,
    }
  });
  if (extract.status.exit_code) return SPN_ERROR;

  sp_fs_create_file(unit->paths.stamp);

  return SPN_OK;
}

s32 configure_package(spn_bg_cmd_t* cmd, void* user_data) {
  spn_pkg_unit_t* pkg = (spn_pkg_unit_t*)user_data;

  sp_try(spn_session_compile_pkg(pkg->ctx.session, pkg));

  if (pkg->on_configure) {
    spn_event_buffer_push(spn.events, &pkg->ctx, SPN_EVENT_BUILD_SCRIPT_CONFIGURE);

    sp_tm_timer_t timer = sp_tm_start_timer();
    spn_try(spn_pkg_unit_call_hook(pkg, pkg->on_configure));
    pkg->time.configure = sp_tm_read_timer(&timer);

    spn_event_buffer_push_ctx(spn.events, &pkg->ctx, (spn_build_event_t) {
      .kind = SPN_EVENT_BUILD_SCRIPT_CONFIGURE_OK,
      .configure.time = pkg->time.configure,
    });
  }

  sp_om_for(pkg->targets, it) {
    spn_target_unit_t* target = sp_om_at(pkg->targets, it);
    sp_da(sp_str_t) source = spn_configure_collect_source(pkg, target);
    sp_da_for(source, j) {
      sp_str_t relative = source[j];
      sp_str_t file = sp_fs_join_path(pkg->ctx.paths.source, relative);
      sp_str_t name = spn_intern(sp_fs_get_stem(file));
      sp_str_t extension = sp_fs_get_ext(relative);
      sp_str_t stem = relative;
      if (!sp_str_empty(extension)) {
        stem = sp_str_prefix(relative, relative.len - extension.len - 1);
      }

      sp_str_t object_path = sp_fs_join_path(target->paths.object, sp_format("{}.o", SP_FMT_STR(stem)));

      if (!sp_om_has(pkg->objects, file)) {
        sp_om_insert(pkg->objects, file, ((spn_compile_unit_t) {
          .name = name,
          .target = target,
          .pkg = target->pkg,
          .profile = &target->session->profile,
          .session = target->session,
          .paths = {
            .object = object_path,
            .source = file,
          },
        }));
      }

      spn_compile_unit_t* object = sp_om_get(pkg->objects, file);
      sp_da_push(target->objects, object);
    }
  }

  return SPN_OK;
}

spn_task_result_t spn_task_update_configure_graph(spn_app_t* app) {
  spn_bg_ctx_t* build = &app->session.configure;

  if (sp_atomic_s32_get(&build->executor->shutdown)) {
    return sp_da_empty(build->executor->errors) ?
      SPN_TASK_DONE :
      SPN_TASK_ERROR;
  }

  return SPN_TASK_CONTINUE;
}

#define try(expr) spn_try_as((expr), SPN_TASK_ERROR)

spn_task_result_t spn_task_init_configure_graph(spn_app_t* app) {
  spn_session_t* b = &app->session;
  spn_build_graph_t* graph = &b->configure.graph;
  spn_session_t* session = &app->session;
  spn_pkg_unit_t* root = spn_session_find_root(&app->session);
  spn_toolchain_unit_t* toolchain = session->units.toolchain;

  graph->error.some = SP_OPT_NONE;

  root->nodes.configure.run = spn_bg_add_fn(graph, configure_package, root);
  root->nodes.configure.stamp = spn_bg_add_file(graph, root->paths.stamp.package);
  try(spn_bg_cmd_add_output(graph, root->nodes.configure.run, root->nodes.configure.stamp));

  sp_om_for(session->units.packages, it) {
    spn_pkg_unit_t* unit = sp_om_at(session->units.packages, it);

    unit->nodes.configure.run = spn_bg_add_fn(graph, configure_package, unit);
    unit->nodes.configure.stamp = spn_bg_add_file(graph, unit->paths.stamp.configure);
    try(spn_bg_cmd_add_output(graph, unit->nodes.configure.run, unit->nodes.configure.stamp));
    try(spn_bg_cmd_add_input(graph, root->nodes.configure.run, unit->nodes.configure.stamp));
  }

  if (toolchain) {
    toolchain->nodes.download = spn_bg_add_fn(graph, download_toolchain, toolchain);
    toolchain->nodes.stamp = spn_bg_add_file(graph, toolchain->paths.stamp);
    try(spn_bg_cmd_add_output(graph, toolchain->nodes.download, toolchain->nodes.stamp));

    try(spn_bg_cmd_add_input(graph, root->nodes.configure.run, toolchain->nodes.stamp));

    sp_om_for(session->units.packages, it) {
      spn_pkg_unit_t* unit = sp_om_at(session->units.packages, it);
      try(spn_bg_cmd_add_input(graph, unit->nodes.configure.run, toolchain->nodes.stamp));
    }
  }

  sp_om_for(session->units.packages, it) {
    spn_pkg_unit_t* unit = sp_om_at(session->units.packages, it);
    spn_pkg_t* pkg = unit->ctx.pkg;

    sp_ht_for(pkg->deps, dit) {
      sp_str_t parent_name = *sp_ht_it_getkp(pkg->deps, dit);
      spn_pkg_unit_t* parent = sp_om_get(session->units.packages, parent_name);
      if (!parent) continue;

      try(spn_bg_cmd_add_input(graph, unit->nodes.configure.run, parent->nodes.configure.stamp));
    }
  }

  b->configure.dirty = spn_bg_compute_forced_dirty(graph);
  b->configure.executor = spn_bg_executor_new(
    graph,
    b->configure.dirty,
    (spn_bg_executor_config_t) {
      .num_threads = 1
    }
  );
  spn_bg_executor_run(b->configure.executor);

  return SPN_TASK_DONE;
}

