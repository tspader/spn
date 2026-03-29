#include "app/app.h"
#include "ctx/ctx.h"
#include "event/event.h"
#include "graph/graph.h"
#include "intern.h"
#include "sp/glob.h"
#include "target/types.h"
#include "log/log.h"
#include "session/session.h"
#include "task.h"
#include "toolchain/types.h"
#include "unit/package.h"

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
    if (!sp_fs_is_regular_file(entry->file_path)) {
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
  spn_session_t* session = (spn_session_t*)user_data;
  spn_toolchain_t* tc = &session->toolchain;
  spn_toolchain_info_t* info = tc->info;

  if (sp_str_empty(info->url)) {
    return SPN_OK;
  }

  sp_str_t store = tc->root;

  tc->compiler = sp_fs_join_path(store, info->compiler);
  tc->linker = sp_fs_join_path(store, info->linker);
  tc->archiver = sp_fs_join_path(store, info->archiver);

  if (sp_fs_exists(store)) return 0;

  sp_fs_create_dir(store);

  sp_str_t tarball = sp_fs_join_path(store, sp_str_lit("toolchain.tar.xz"));

  sp_ps_output_t dl = sp_ps_run((sp_ps_config_t) {
    .command = sp_str_lit("curl"),
    .args = {
      sp_str_lit("-fSL"),
      sp_str_lit("-o"), tarball,
      info->url
    }
  });
  if (dl.status.exit_code) return SPN_ERROR;

  sp_ps_output_t extract = sp_ps_run((sp_ps_config_t) {
    .command = sp_str_lit("tar"),
    .args = {
      sp_str_lit("xf"), tarball,
      sp_str_lit("--strip-components=1"),
      sp_str_lit("-C"), store,
    }
  });
  if (extract.status.exit_code) return SPN_ERROR;


  sp_fs_create_file(tc->stamp);

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
          .profile = pkg->ctx.profile,
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

spn_err_t init_configure_graph(spn_app_t* app) {
  spn_session_t* b = &app->session;
  spn_build_graph_t* graph = &b->configure.graph;
  spn_pkg_unit_t* root = spn_session_find_root(&app->session);

  // If the toolchain needs downloading, add a node that everything depends on
  spn_bg_id_t toolchain_stamp = {0};
  bool has_toolchain_download = !sp_str_empty(b->toolchain.stamp);
  if (has_toolchain_download) {
    spn_bg_id_t toolchain_run = spn_bg_add_fn_ex(
      graph, download_toolchain, b,
      SPN_BG_VIZ_DEFAULT, app->package.name, sp_str_lit("toolchain")
    );
    toolchain_stamp = spn_bg_add_file(graph, b->toolchain.stamp);
    sp_try(spn_bg_cmd_add_output(graph, toolchain_run, toolchain_stamp));
  }

  root->nodes.configure.run = spn_bg_add_fn_ex(graph, configure_package, root, SPN_BG_VIZ_DEFAULT, app->package.name, sp_str_lit("configure"));
  root->nodes.configure.stamp = spn_bg_add_file(graph, root->paths.stamp.package);
  sp_try(spn_bg_cmd_add_output(graph, root->nodes.configure.run, root->nodes.configure.stamp));
  if (has_toolchain_download) {
    sp_try(spn_bg_cmd_add_input(graph, root->nodes.configure.run, toolchain_stamp));
  }

  sp_om_for(b->units.packages, it) {
    spn_pkg_unit_t* unit = sp_om_at(b->units.packages, it);
    unit->nodes.configure.run = spn_bg_add_fn_ex(graph, configure_package, unit, SPN_BG_VIZ_DEFAULT, app->package.name, sp_str_lit("configure"));
    unit->nodes.configure.stamp = spn_bg_add_file(graph, unit->paths.stamp.configure);
    sp_try(spn_bg_cmd_add_output(graph, unit->nodes.configure.run, unit->nodes.configure.stamp));
    sp_try(spn_bg_cmd_add_input(graph, root->nodes.configure.run, unit->nodes.configure.stamp));
    if (has_toolchain_download) {
      sp_try(spn_bg_cmd_add_input(graph, unit->nodes.configure.run, toolchain_stamp));
    }
  }

  sp_om_for(b->units.packages, it) {
    spn_pkg_unit_t* dep = sp_om_at(b->units.packages, it);
    spn_pkg_t* pkg = dep->ctx.pkg;

    sp_ht_for(pkg->deps, dit) {
      sp_str_t parent_name = *sp_ht_it_getkp(pkg->deps, dit);
      spn_pkg_unit_t* parent = sp_om_get(b->units.packages, parent_name);
      if (!parent) continue;

      sp_try(spn_bg_cmd_add_input(graph, dep->nodes.configure.run, parent->nodes.configure.stamp));
    }
  }

  return SPN_OK;
}

spn_task_result_t spn_task_init_configure_graph(spn_app_t* app) {
  spn_session_t* b = &app->session;
  spn_build_graph_t* graph = &b->configure.graph;

  graph->error.some = SP_OPT_NONE;
  if (init_configure_graph(app)) {
    switch (graph->error.some) {
      case SP_OPT_SOME: {
        spn_log_error("{}", SP_FMT_STR(spn_bg_err_to_str(graph, graph->error.value)));
        break;
      }
      case SP_OPT_NONE: {
        spn_log_error("failed to prepare configure graph");
        break;
      }
    }

    return SPN_TASK_ERROR;
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
