#include "app.h"
#include "err.h"
#include "event.h"
#include "gen.h"
#include "session.h"
#include "external/cc.h"

spn_err_t add_link_edges(spn_session_t* session, spn_build_graph_t* graph, spn_pkg_unit_t* unit) {
  spn_pkg_t* pkg = unit->ctx.pkg;
  sp_ht_for(pkg->deps, it) {
    spn_pkg_unit_t* parent = spn_session_find_pkg(session, *sp_ht_it_getkp(pkg->deps, it));
    sp_try(spn_bg_cmd_add_input(graph, unit->nodes.build.main, parent->nodes.build.stamp.package));
  }

  return SPN_OK;
}

spn_bg_id_t get_or_put_user_file(spn_pkg_unit_t* ctx, spn_build_graph_t* graph, sp_str_t path) {
  spn_bg_id_t* existing = sp_str_ht_get(ctx->nodes.files, path);
  if (existing) return *existing;

  spn_bg_id_t id = spn_bg_add_file_ex(graph, path, SPN_BG_VIZ_CMD, ctx->ctx.name);
  sp_str_ht_insert(ctx->nodes.files, path, id);
  sp_da_push(ctx->nodes.build.user, id);
  return id;
}

spn_cc_t* make_cc_for_compile_or_link(spn_pkg_t* pkg, spn_target_t* target, sp_str_t path, spn_profile_t* profile) {
  spn_cc_t* cc = sp_alloc_type(spn_cc_t);
  spn_cc_set_profile(cc, profile);
  spn_cc_set_output_dir(cc, path);

  sp_da_for(pkg->include, it) {
    spn_cc_add_include(cc, pkg->include[it]);
  }

  sp_da_for(pkg->define, it) {
    spn_cc_add_define(cc, pkg->define[it]);
  }

  return cc;
}

void setup_target_for_compile_or_link(spn_cc_t* cc, spn_cc_target_t* cc_target, spn_target_t* target, spn_pkg_t* pkg, spn_session_t* session) {
  sp_da_for(target->include, i) {
    spn_cc_target_add_relative_include(cc_target, target->include[i]);
  }

  sp_da_for(target->define, i) {
    spn_cc_target_add_define(cc_target, target->define[i]);
  }

  sp_da_for(pkg->system_deps, i) {
    spn_cc_target_add_lib(cc_target, spn_gen_format_entry(pkg->system_deps[i], SPN_GEN_SYSTEM_LIBS, cc->compiler.kind));
  }

  sp_ht_for_kv(pkg->deps, i) {
    if (spn_is_visibility_linked(target->visibility, i.val->visibility)) {
      spn_pkg_unit_t* dep = spn_session_find_pkg(session, *i.key);
      spn_cc_target_add_dep(cc_target, dep);

      sp_da_for(dep->ctx.pkg->system_deps, n) {
        spn_cc_target_add_lib(cc_target, spn_gen_format_entry(dep->ctx.pkg->system_deps[n], SPN_GEN_SYSTEM_LIBS, cc->compiler.kind));
      }
    }
  }
}

spn_err_t run_cc(spn_cc_t* cc, spn_cc_target_t* cc_target, sp_str_t cwd, spn_pkg_t* pkg, spn_build_io_t* io) {
  sp_ps_config_t ps = {
    .cwd = cwd,
    .io = {
      .in.mode = SP_PS_IO_MODE_NULL,
      .err.mode = SP_PS_IO_MODE_REDIRECT,
    }
  };
  spn_cc_to_ps(cc, &ps);
  spn_cc_target_to_ps(cc, cc_target, &ps);

  sp_mem_scratch_t scratch = sp_mem_begin_scratch(); {
    sp_str_builder_t log = SP_ZERO_INITIALIZE();
    sp_str_builder_append(&log, cc->compiler.exe);
    sp_str_builder_append_c8(&log, ' ');

    sp_da_for(ps.dyn_args, it) {
      sp_str_builder_append(&log, ps.dyn_args[it]);
      sp_str_builder_append_c8(&log, ' ');
    }

    spn_push_event_ex((spn_build_event_t) {
      .kind = SPN_EVENT_TARGET_BUILD,
      .target.build = {
        .args = sp_str_builder_to_str(&log),
      }
    });

    sp_mem_end_scratch(scratch);
  }

  sp_ps_output_t result = sp_ps_run(ps);
  if (result.status.exit_code) {
    spn_event_buffer_push_ex(spn.events, pkg, io, (spn_build_event_t) {
      .kind = SPN_EVENT_TARGET_BUILD_FAILED,
      .target.failed = {
        .out = result.out,
        .err = result.err,
      }
    });

    return SPN_ERROR;
  }

  else {
    spn_event_buffer_push_ex(spn.events, pkg, io, (spn_build_event_t) {
      .kind = SPN_EVENT_TARGET_BUILD_PASSED
    });
  }

  return SPN_OK;
}

s32 compile_object(spn_bg_cmd_t* cmd, void* user_data) {
  spn_compile_unit_t* unit = (spn_compile_unit_t*)user_data;

  //spn_log_error("compile: {}", SP_FMT_STR(unit->paths.object));

  spn_event_buffer_push_ex(spn.events, unit->pkg, &unit->target->logs, (spn_build_event_t) {
    .kind = SPN_EVENT_DEBUG,
    .debug = {
      .message = unit->paths.object
    }
  });

  sp_str_t file = sp_fs_get_name(unit->paths.object);

  spn_cc_t* cc = make_cc_for_compile_or_link(unit->pkg, unit->target->info, unit->target->paths.object, unit->profile);
  spn_cc_target_t* cc_target = spn_cc_add_target(cc, SPN_TARGET_OBJECT, file);
  setup_target_for_compile_or_link(cc, cc_target, unit->target->info, unit->pkg, unit->session);
  spn_cc_target_add_absolute_source(cc_target, unit->paths.source);

  return run_cc(cc, cc_target, unit->target->paths.work, unit->pkg, &unit->target->logs);
}

s32 link_target(spn_bg_cmd_t* cmd, void* user_data) {
  spn_target_unit_t* unit = (spn_target_unit_t*)user_data;
  spn_target_t* target = unit->info;

  spn_event_buffer_push_ex(spn.events, unit->pkg, &unit->logs, (spn_build_event_t) {
    .kind = SPN_EVENT_LINK_TARGET,
    .target_link = {
      .target = target->name,
      .objects = sp_da_size(unit->objects),
    }
  });

  //spn_log_error("link: {}", SP_FMT_STR(unit->target->name));

  spn_cc_t* cc = make_cc_for_compile_or_link(unit->pkg, unit->info, unit->paths.bin, unit->session->profile);
  spn_cc_target_t* cc_target = spn_cc_add_target(cc, SPN_TARGET_EXE, target->name);
  setup_target_for_compile_or_link(cc, cc_target, target, unit->pkg, unit->session);

  sp_da_for(unit->objects, it) {
    spn_cc_target_add_absolute_source(cc_target, unit->objects[it]->paths.object);
  }

  return run_cc(cc, cc_target, unit->paths.work, unit->pkg, &unit->logs);


  if (!sp_da_empty(target->embed)) {
    // spn_cc_embed_ctx_t embedder = SP_ZERO_INITIALIZE();
    // spn_cc_embed_ctx_init(&embedder);
    //
    // sp_da_for(target->embed, it) {
    //   spn_embed_t embed = target->embed[it];
    //   sp_str_t symbol = embed.symbol;
    //   spn_embed_types_t types = embed.types;
    //   sp_io_reader_t io = SP_ZERO_INITIALIZE();
    //
    //   if (sp_str_empty(embed.types.size)) {
    //     embed.types.data = spn_intern_cstr("unsigned char");
    //     embed.types.size = spn_intern_cstr("unsigned long long");
    //   }
    //
    //   switch (embed.kind) {
    //     case SPN_EMBED_MEM: {
    //       io = sp_io_reader_from_mem(embed.memory.buffer, embed.memory.size);
    //       break;
    //     }
    //     case SPN_EMBED_FILE: {
    //       if (!sp_fs_exists(embed.file.path)) {
    //         return SPN_ERROR;
    //       }
    //
    //       io = sp_io_reader_from_file(embed.file.path);
    //
    //       if (sp_str_empty(symbol)) {
    //         symbol = spn_cc_symbol_from_embedded_file(embed.file.path);
    //       }
    //       break;
    //     }
    //   }
    //
    //   spn_cc_embed_ctx_add(&embedder, io, symbol, embed.types.data, embed.types.size);
    // }
    //
    // sp_str_t object = sp_fs_join_path(unit->paths.generated, sp_format("{}.embed.o", SP_FMT_STR(unit->pkg->name)));
    // sp_str_t header = sp_fs_join_path(unit->paths.generated, sp_format("{}.embed.h", SP_FMT_STR(unit->pkg->name)));
    // spn_cc_embed_ctx_write(&embedder, object, header);
    // spn_cc_target_add_lib(cc_target, object);
    // spn_cc_target_add_include_abs(cc_target, unit->paths.generated);
  }

    // sp_da_for(app.resolver.system_deps, i) {
    //   sp_str_t arg = spn_gen_format_entry(app.resolver.system_deps[i], SPN_GEN_SYSTEM_LIBS, ctx->profile->cc.kind);
    //   sp_ps_config_add_arg(&ps, arg);
    // }





  // sp_mem_scratch_t scratch = sp_mem_begin_scratch(); {
  //   sp_str_builder_t log = SP_ZERO_INITIALIZE();
  //   sp_str_builder_append(&log, ctx->profile->cc.exe);
  //   sp_str_builder_append_c8(&log, ' ');
  //
  //   sp_da_for(ps.dyn_args, it) {
  //     sp_str_builder_append(&log, ps.dyn_args[it]);
  //     sp_str_builder_append_c8(&log, ' ');
  //   }
  //
  //   spn_push_event_ex((spn_build_event_t) {
  //     .kind = SPN_EVENT_TARGET_BUILD,
  //     .target.build = {
  //       .args = sp_str_builder_to_str(&log),
  //     }
  //   });
  //
  //   sp_mem_end_scratch(scratch);
  // }
  //
  // sp_ps_output_t result = spn_build_ctx_subprocess(ctx, ps);
  // if (result.status.exit_code) {
  //   spn_event_buffer_push_ctx(spn.events, ctx, (spn_build_event_t) {
  //     .kind = SPN_EVENT_TARGET_BUILD_FAILED,
  //     .target.failed = {
  //       .out = result.out,
  //       .err = result.err,
  //     }
  //   });
  //
  //   return SPN_ERROR;
  // } else {
  //   spn_event_buffer_push(spn.events, ctx, SPN_EVENT_TARGET_BUILD_PASSED);
  // }

  return SPN_OK;
}

s32 run_package_hook(spn_bg_cmd_t* cmd, void* user_data) {
  spn_pkg_unit_t* ctx = (spn_pkg_unit_t*)user_data;

  spn_event_buffer_push(spn.events, &ctx->ctx, SPN_EVENT_BUILD_SCRIPT_PACKAGE);

  if (spn_pkg_unit_run_package_hook(ctx)) {
    spn_event_buffer_push(spn.events, &ctx->ctx, SPN_EVENT_BUILD_SCRIPT_PACKAGE_FAILED);
    return SPN_ERROR;
  }

  spn_pkg_unit_write_stamp(ctx, ctx->paths.stamp.package);

  return SPN_OK;
}

s32 write_enter_stamp(spn_bg_cmd_t* cmd, void* user_data) {
  spn_pkg_unit_t* unit = (spn_pkg_unit_t*)user_data;
  spn_pkg_unit_write_stamp(unit, unit->paths.stamp.main);
  return SPN_OK;
}

s32 write_exit_stamp(spn_bg_cmd_t* cmd, void* user_data) {
  spn_pkg_unit_t* unit = (spn_pkg_unit_t*)user_data;
  spn_pkg_unit_write_stamp(unit, unit->paths.stamp.exit);
  return SPN_OK;
}

s32 run_user_fn(spn_bg_cmd_t* cmd, void* user_data) {
  spn_user_node_t* node = (spn_user_node_t*)user_data;

  spn_event_buffer_push_ctx(spn.events, &node->ctx->ctx, (spn_build_event_t) {
    .kind = SPN_EVENT_BUILD_SCRIPT_USER_FN,
    .node = { .info = node }
  });

  spn_err_t err = SPN_OK;
  if (node->fn) {
    spn_node_ctx_t ctx = {
      .build = &node->ctx->ctx,
      .user_data = node->user_data
    };

    switch (node->fn(&ctx)) {
      case SPN_OK: {
        spn_pkg_unit_write_stamp(node->ctx, spn_pkg_unit_get_node_stamp_file(node->ctx, node));
        sp_str_t stamp = sp_fs_join_path(node->ctx->paths.stamp.dir, node->tag);
        sp_fs_create_file(stamp);
        break;
      }
      default: {
        break;
      }
    }
  }

  return err;
}


//                    ┌───────┐   ┌────────────────┐   ┌────────┐
//                    │ foo.c │──▶│ compile::foo.c │──▶│ foo.o  │──┐
//                    └───────┘   └────────────────┘   └────────┘  │
//                                                                 │  ┌──────┐   ┌───────────────────┐   ┌─────────────────┐
//                                                                 ├─▶│ link │──▶│ $STORE/bin/foobar │──▶│ script::package │
//                    ┌───────┐   ┌────────────────┐   ┌────────┐  │  └──────┘   └───────────────────┘   └─────────────────┘
//                    │ bar.c │──▶│ compile::bar.c │──▶│ bar.o  │──┘     ▲
//                    └───────┘   └────────────────┘   └────────┘        │
//                                                                       │
// ┌ ─ ─ ─ ─ ─ ─ ┐    ┌─────────────┐                                    │
//   user graph   ───▶│ user::exit  │────────────────────────────────────┘
// └ ─ ─ ─ ─ ─ ─ ┘    └─────────────┘
spn_err_t add_target(spn_build_graph_t* graph, spn_pkg_unit_t* pkg, spn_target_unit_t* unit) {
  spn_target_t* target = unit->info;

  unit->nodes.link = spn_bg_add_fn_ex(graph, link_target, unit, SPN_BG_VIZ_CMD, pkg->ctx.name, sp_format("link {}", SP_FMT_STR(target->name)));
  unit->nodes.output = spn_bg_add_file_ex(graph, sp_fs_join_path(pkg->ctx.paths.bin, target->name), SPN_BG_VIZ_BINARY, pkg->ctx.name);

  sp_try(spn_bg_cmd_add_output(graph, unit->nodes.link,  unit->nodes.output));
  //spn_bg_cmd_add_input(graph, unit->nodes.link, pkg->nodes.build.stamp.exit);
  //spn_bg_cmd_add_input(graph, unit->nodes.compile, pkg->nodes.build.stamp.main);
  sp_try(spn_bg_cmd_add_input(graph, pkg->nodes.build.package, unit->nodes.output));

  sp_da_for(unit->objects, it) {
    spn_compile_unit_t* obj = unit->objects[it];
    sp_try(spn_bg_cmd_add_input(graph, unit->nodes.link, obj->nodes.object));
  }

  return SPN_OK;
}

// ┌──────────┐
// │ spn.toml │──┐
// └──────────┘  │  ┌─────────────┐   ┌ ─ ─ ─ ─ ─ ─ ─ ─ ┐   ┌─────────────┐   ┌──────────────────┐
//               ├─▶│ user::main  │──▶    user graph     ──▶│ user::exit  │──▶│ script::package  │
// ┌──────────┐  │  └─────────────┘   └ ─ ─ ─ ─ ─ ─ ─ ─ ┘   └─────────────┘   └──────────────────┘
// │  spn.c   │──┘         │                                       ▲
// └──────────┘            └───────────────────────────────────────┘
//
spn_err_t add_package(spn_build_graph_t* graph, spn_pkg_unit_t* unit) {
  spn_pkg_nodes_t* nodes = &unit->nodes.build;
  spn_pkg_t* pkg = unit->ctx.pkg;

  nodes->manifest = spn_bg_add_file_ex(graph, pkg->paths.manifest, SPN_BG_VIZ_MANIFEST, unit->ctx.name);
  nodes->script = spn_bg_add_file_ex(graph, pkg->paths.script, SPN_BG_VIZ_MANIFEST, unit->ctx.name);
  nodes->package = spn_bg_add_fn_ex(graph, run_package_hook, unit, SPN_BG_VIZ_CMD, unit->ctx.name, sp_str_lit("script::package"));
  nodes->stamp.package = spn_bg_add_file_ex(graph, unit->paths.stamp.package, SPN_BG_VIZ_STAMP, unit->ctx.name);
  nodes->stamp.main = spn_bg_add_file_ex(graph, unit->paths.stamp.main, SPN_BG_VIZ_CMD, unit->ctx.name);
  nodes->stamp.exit = spn_bg_add_file_ex(graph, unit->paths.stamp.exit, SPN_BG_VIZ_CMD, unit->ctx.name);
  nodes->main = spn_bg_add_fn_ex(graph, write_enter_stamp, unit, SPN_BG_VIZ_CMD, pkg->name, sp_str_lit("user::enter"));
  nodes->exit = spn_bg_add_fn_ex(graph, write_exit_stamp, unit, SPN_BG_VIZ_CMD, pkg->name, sp_str_lit("user::exit"));

  sp_try(spn_bg_cmd_add_input(graph, nodes->main, nodes->manifest));
  sp_try(spn_bg_cmd_add_input(graph, nodes->main, nodes->script));
  sp_try(spn_bg_cmd_add_output(graph, nodes->main, nodes->stamp.main));
  sp_try(spn_bg_cmd_add_input(graph, nodes->exit, nodes->stamp.main));
  sp_try(spn_bg_cmd_add_output(graph, nodes->exit, nodes->stamp.exit));
  sp_try(spn_bg_cmd_add_input(graph, nodes->package, nodes->stamp.exit));
  sp_try(spn_bg_cmd_add_output(graph, nodes->package, nodes->stamp.package));

  // user nodes
  // pass 1: create all command nodes
  sp_da_for(unit->nodes.all, it) {
    spn_user_node_t* node = &unit->nodes.all[it];
    node->id = spn_bg_add_fn_ex(graph, run_user_fn, node, SPN_BG_VIZ_CMD, unit->ctx.name, node->tag);
    sp_da_push(unit->nodes.build.user, node->id);
  }

  // pass 2: create all file in/outputs + create a stamp file for phonies
  sp_da_for(unit->nodes.all, it) {
    spn_user_node_t* node = &unit->nodes.all[it];

    // if no outputs, depend on the exit node's stamp
    if (sp_da_empty(node->outputs)) {
      sp_da_push(node->outputs, spn_pkg_unit_get_node_stamp_file(unit, node));
    }

    sp_da_for(node->outputs, o) {
      spn_bg_id_t file = get_or_put_user_file(unit, graph, node->outputs[o]);
      sp_try(spn_bg_cmd_add_output(graph, node->id, file));
      sp_try(spn_bg_cmd_add_input(graph, nodes->exit, file));
    }

    // if no inputs, depend on the main node's stamp
    if (sp_da_empty(node->inputs) && sp_da_empty(node->deps)) {
      sp_try(spn_bg_cmd_add_input(graph, node->id, nodes->stamp.main));
    }

    sp_da_for(node->inputs, i) {
      spn_bg_id_t file = get_or_put_user_file(unit, graph, node->inputs[i]);
      sp_try(spn_bg_cmd_add_input(graph, node->id, file));
    }
  }

  // pass 3: now that all file nodes exist, set up links for command inputs
  sp_da_for(unit->nodes.all, it) {
    spn_user_node_t* node = &unit->nodes.all[it];

    // depend on the outputs of your command inputs
    sp_da_for(node->deps, dit) {
      spn_user_node_t* dep = spn_find_user_node(node->deps[dit]);
      sp_da_for(dep->outputs, oit) {
        spn_bg_id_t output = get_or_put_user_file(unit, graph, dep->outputs[oit]);
        sp_try(spn_bg_cmd_add_input(graph, node->id, output));
      }
    }
  }

  // object files
  // @spader gate this on user node exit?
  sp_om_for(unit->objects, it) {
    spn_compile_unit_t* obj = sp_om_at(unit->objects, it);
    obj->nodes.source = spn_bg_add_file_ex(graph, obj->paths.source, SPN_BG_VIZ_DEFAULT, pkg->name);
    obj->nodes.compile = spn_bg_add_fn_ex(graph, compile_object, obj, SPN_BG_VIZ_CMD, pkg->name, sp_format("compile::{}", SP_FMT_STR(obj->paths.source)));
    obj->nodes.object = spn_bg_add_file_ex(graph, obj->paths.object, SPN_BG_VIZ_DEFAULT, pkg->name);
    sp_try(spn_bg_cmd_add_output(graph, obj->nodes.compile, obj->nodes.object));
    sp_try(spn_bg_cmd_add_input(graph, obj->nodes.compile, unit->nodes.build.stamp.exit));
  }

  sp_om_for(unit->targets, it) {
    sp_try(add_target(graph, unit, sp_om_at(unit->targets, it)));
  }

  return SPN_OK;
}

spn_err_t prepare_build_graph(spn_app_t* app) {
  spn_session_t* session = &app->session;
  spn_build_graph_t* graph = &session->build.graph;

  // phase 1: add each package to the graph
  sp_om_for(session->units.packages, it) {
    spn_pkg_unit_t* unit = sp_om_at(session->units.packages, it);
    sp_try(add_package(graph, unit));
  }
  sp_try(add_package(graph, spn_session_find_root(session)));

  // phase 2: link dependent packages
  sp_om_for(session->units.packages, it) {
    sp_try(add_link_edges(session, graph, sp_om_at(session->units.packages, it)));
  }
  sp_try(add_link_edges(session, graph, spn_session_find_root(session)));

  return SPN_OK;
}

spn_task_result_t spn_task_prepare_build_graph(spn_app_t* app) {
  spn_session_t* session = &app->session;
  spn_build_graph_t* graph = &session->build.graph;

  graph->error.some = SP_OPT_NONE;
  if (prepare_build_graph(app)) {
    switch (graph->error.some) {
      case SP_OPT_SOME: {
        spn_log_error("{}", SP_FMT_STR(spn_bg_err_to_str(graph, graph->error.value)));
        break;
      }
      case SP_OPT_NONE: {
        spn_log_error("failed to prepare build graph");
        break;
      }
    }

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

  b->build.executor = spn_bg_executor_new(&b->build.graph, b->build.dirty, (spn_bg_executor_config_t) {
    .num_threads = 1,
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

    switch (error.some) {
      case SP_OPT_SOME: {
        return SPN_TASK_ERROR;
      }
      case SP_OPT_NONE: {
        if (!app->lock.some) {
          spn_app_update_lock_file(app);
        }

        spn_event_buffer_push_ctx(spn.events, &spn_session_find_root(b)->ctx, (spn_build_event_t) {
          .kind = SPN_EVENT_BUILD_PASSED,
          .build.passed = {
            .profile = app->session.profile,
            .time = b->build.executor->elapsed
          }
        });

        return SPN_TASK_DONE;
      }
    }

    return SPN_TASK_DONE;
  }

  return SPN_TASK_CONTINUE;
}
