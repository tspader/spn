#include "ctx/types.h"
#include "event/types.h"

#include "app/app.h"
#include "err.h"
#include "enum/enum.h"
#include "external/cc.h"
#include "filter/filter.h"
#include "gen.h"
#include "graph/graph.h"
#include "event/event.h"
#include "session/session.h"
#include "toolchain/toolchain.h"
#include "unit/package.h"

static spn_err_union_t spn_bg_error_to_union(spn_build_graph_t* graph) {
  if (!graph->error.some) {
    return (spn_err_union_t) {
      .kind = SPN_ERR_BUILD_GRAPH,
      .build_graph = {
        .kind = SPN_BUILD_GRAPH_ERR_UNKNOWN,
      },
    };
  }

  switch (graph->error.value.kind) {
    case SPN_BG_OK: {
      return (spn_err_union_t) {
        .kind = SPN_ERR_BUILD_GRAPH,
        .build_graph = {
          .kind = SPN_BUILD_GRAPH_ERR_UNKNOWN,
        },
      };
    }
    case SPN_BG_ERR_MISSING_INPUT: {
      return (spn_err_union_t) {
        .kind = SPN_ERR_BUILD_GRAPH,
        .build_graph = {
          .kind = SPN_BUILD_GRAPH_ERR_MISSING_INPUT,
          .file = spn_bg_file_id_to_str(graph, graph->error.value.missing_input.file_id),
        },
      };
    }
    case SPN_BG_ERR_DUPLICATE_OUTPUT: {
      return (spn_err_union_t) {
        .kind = SPN_ERR_BUILD_GRAPH,
        .build_graph = {
          .kind = SPN_BUILD_GRAPH_ERR_DUPLICATE_OUTPUT,
          .file = spn_bg_file_id_to_str(graph, graph->error.value.duplicate_output.file),
          .command_a = spn_bg_cmd_id_to_str(graph, graph->error.value.duplicate_output.cmds.a),
          .command_b = spn_bg_cmd_id_to_str(graph, graph->error.value.duplicate_output.cmds.b),
        },
      };
    }
  }

  return (spn_err_union_t) {
    .kind = SPN_ERR_BUILD_GRAPH,
    .build_graph = {
      .kind = SPN_BUILD_GRAPH_ERR_UNKNOWN,
    },
  };
}

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

sp_str_t get_embed_object_path(spn_target_unit_t* unit) {
  return sp_fs_join_path(unit->paths.generated, sp_format("{}.embed.o", SP_FMT_STR(unit->info->name)));
}

sp_str_t get_embed_header_path(spn_target_unit_t* unit) {
  return sp_fs_join_path(unit->paths.generated, sp_format("{}.embed.h", SP_FMT_STR(unit->info->name)));
}

sp_str_t get_target_output_path(spn_target_unit_t* unit) {
  spn_target_t* target = unit->info;

  switch (target->kind) {
    case SPN_TARGET_EXE: {
      return sp_fs_join_path(unit->paths.bin, target->name);
    }
    case SPN_TARGET_STATIC_LIB:
    case SPN_TARGET_SHARED_LIB: {
      spn_linkage_t linkage = spn_target_kind_to_pkg_linkage(target->kind);
      sp_str_t file = sp_os_lib_to_file_name(target->name, spn_lib_kind_to_sp_os_lib_kind(linkage));
      return sp_fs_join_path(unit->paths.lib, file);
    }
    case SPN_TARGET_NONE:
    case SPN_TARGET_JIT:
    case SPN_TARGET_OBJECT: {
      SP_UNREACHABLE_CASE();
    }
  }

  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

void setup_target_for_compile_or_link(spn_cc_t* cc, spn_cc_target_t* cc_target, spn_target_t* target, spn_pkg_t* pkg, spn_session_t* session) {
  sp_da_for(target->include, i) {
    spn_cc_target_add_absolute_include(cc_target, sp_fs_join_path(pkg->paths.root, target->include[i]));
  }

  sp_da_for(target->define, i) {
    spn_cc_target_add_define(cc_target, target->define[i]);
  }

  sp_da_for(pkg->system_deps, i) {
    spn_cc_target_add_lib(cc_target, spn_gen_format_entry(pkg->system_deps[i], SPN_GEN_SYSTEM_LIBS, cc->toolchain.info->driver));
  }

  sp_ht_for_kv(pkg->deps, i) {
    if (spn_is_visibility_linked(target->visibility, i.val->visibility)) {
      spn_pkg_unit_t* dep = spn_session_find_pkg(session, *i.key);
      spn_cc_target_add_dep(cc_target, dep);

      sp_da_for(dep->ctx.pkg->system_deps, n) {
        spn_cc_target_add_lib(cc_target, spn_gen_format_entry(dep->ctx.pkg->system_deps[n], SPN_GEN_SYSTEM_LIBS, cc->toolchain.info->driver));
      }
    }
  }
}

typedef struct {
  sp_ps_output_t result;
  u64 elapsed;
  sp_str_t args;
} spn_cc_run_result_t;

spn_cc_run_result_t run_cc_exec(spn_cc_t* cc, spn_cc_target_t* cc_target, sp_str_t cwd) {
  sp_ps_config_t ps = {
    .cwd = cwd,
    .io = {
      .in.mode = SP_PS_IO_MODE_NULL,
      .err.mode = SP_PS_IO_MODE_REDIRECT,
    }
  };
  spn_cc_to_ps(cc, &ps);
  spn_cc_target_to_ps(cc, cc_target, &ps);

  // Build a string of the entire command line. Use the scratch arena because
  // these can legitimately get quite long!
  sp_str_builder_t log = SP_ZERO_INITIALIZE();
  sp_str_builder_append(&log, cc->toolchain.compiler);
  sp_str_builder_append_c8(&log, ' ');
  sp_da_for(ps.dyn_args, it) {
    sp_str_builder_append(&log, ps.dyn_args[it]);
    sp_str_builder_append_c8(&log, ' ');
  }

  sp_tm_timer_t timer = sp_tm_start_timer();
  sp_ps_output_t result = sp_ps_run(ps);
  u64 elapsed = sp_tm_read_timer(&timer);

  return (spn_cc_run_result_t) {
    .result = result,
    .elapsed = elapsed,
    .args = sp_str_builder_to_str(&log),
  };
}

spn_err_t run_cc(spn_cc_t* cc, spn_cc_target_t* cc_target, sp_str_t cwd, spn_pkg_t* pkg, spn_build_io_t* io, sp_str_t target_name) {
  sp_str_t source = sp_da_size(cc_target->source) ? cc_target->source[0] : sp_str_lit("");
  sp_str_t object = cc_target->output;

  spn_cc_run_result_t run = run_cc_exec(cc, cc_target, cwd);

  spn_event_buffer_push_ex(spn.events, pkg, io, (spn_build_event_t) {
    .kind = SPN_EVENT_TARGET_BUILD,
    .target.name = target_name,
    .target.build = {
      .source_file = source,
      .object_file = object,
      .compiler = cc->toolchain.compiler,
      .args = run.args,
    }
  });

  if (run.result.status.exit_code) {
    spn_event_buffer_push_ex(spn.events, pkg, io, (spn_build_event_t) {
      .kind = SPN_EVENT_TARGET_BUILD_FAILED,
      .target.name = target_name,
      .target.failed = {
        .source_file = source,
        .object_file = object,
        .rc = run.result.status.exit_code,
        .out = run.result.out,
        .err = run.result.err,
        .time = run.elapsed,
      }
    });
    return SPN_ERROR;
  }

  spn_event_buffer_push_ex(spn.events, pkg, io, (spn_build_event_t) {
    .kind = SPN_EVENT_TARGET_BUILD_PASSED,
    .target.name = target_name,
    .target.passed = {
      .source_file = source,
      .object_file = object,
      .time = run.elapsed,
    }
  });
  return SPN_OK;
}

typedef struct {
  sp_ps_output_t result;
  u64 elapsed;
  sp_str_t args;
} spn_ar_run_result_t;

spn_ar_run_result_t run_ar_exec(spn_target_unit_t* unit, sp_str_t output) {
  sp_ps_config_t ps = {
    .command = sp_str_lit("ar"),
    .cwd = unit->paths.work,
    .io = {
      .in.mode = SP_PS_IO_MODE_NULL,
      .err.mode = SP_PS_IO_MODE_REDIRECT,
    },
  };
  sp_ps_config_add_arg(&ps, sp_str_lit("rcs"));
  sp_ps_config_add_arg(&ps, output);

  sp_da_for(unit->objects, it) {
    sp_ps_config_add_arg(&ps, unit->objects[it]->paths.object);
  }

  sp_str_builder_t log = SP_ZERO_INITIALIZE();
  sp_str_builder_append(&log, ps.command);
  sp_str_builder_append_c8(&log, ' ');
  sp_da_for(ps.dyn_args, it) {
    sp_str_builder_append(&log, ps.dyn_args[it]);
    sp_str_builder_append_c8(&log, ' ');
  }

  sp_tm_timer_t timer = sp_tm_start_timer();
  sp_ps_output_t result = sp_ps_run(ps);
  u64 elapsed = sp_tm_read_timer(&timer);

  return (spn_ar_run_result_t) {
    .result = result,
    .elapsed = elapsed,
    .args = sp_str_builder_to_str(&log),
  };
}

s32 compile_object(spn_bg_cmd_t* cmd, void* user_data) {
  spn_compile_unit_t* unit = (spn_compile_unit_t*)user_data;

  sp_str_t file = sp_fs_get_name(unit->paths.object);
  sp_str_t object_dir = sp_fs_parent_path(unit->paths.object);
  sp_fs_create_dir(object_dir);

  spn_cc_t* cc = make_cc_for_compile_or_link(unit->pkg, unit->target->info, object_dir, unit->profile);
  cc->toolchain = unit->session->toolchain;
  spn_cc_target_t* cc_target = spn_cc_add_target(cc, SPN_TARGET_OBJECT, file);
  setup_target_for_compile_or_link(cc, cc_target, unit->target->info, unit->pkg, unit->session);

  if (!sp_da_empty(unit->target->info->embed)) {
    spn_cc_target_add_absolute_include(cc_target, unit->target->paths.generated);
  }

  spn_cc_target_add_absolute_source(cc_target, unit->paths.source);

  return run_cc(cc, cc_target, unit->target->paths.work, unit->pkg, &unit->target->logs, unit->target->info->name);
}

s32 compile_embed(spn_bg_cmd_t* cmd, void* user_data) {
  spn_target_unit_t* unit = (spn_target_unit_t*)user_data;
  spn_target_t* target = unit->info;

  spn_event_buffer_push_ex(spn.events, unit->pkg, &unit->logs, (spn_build_event_t) {
    .kind = SPN_EVENT_EMBED_START,
    .embed_start = { .num_files = sp_da_size(target->embed) },
  });

  sp_tm_timer_t timer = sp_tm_start_timer();

  spn_cc_embed_ctx_t embedder = SP_ZERO_INITIALIZE();
  spn_cc_embed_ctx_init(&embedder);

  sp_da_for(target->embed, it) {
    spn_embed_t embed = target->embed[it];
    sp_str_t symbol = embed.symbol;
    spn_embed_types_t types = embed.types;
    sp_io_reader_t io = SP_ZERO_INITIALIZE();

    if (sp_str_empty(types.data)) {
      types.data = spn_intern_cstr("unsigned char");
    }

    if (sp_str_empty(types.size)) {
      types.size = spn_intern_cstr("unsigned long long");
    }

    switch (embed.kind) {
      case SPN_EMBED_MEM: {
        io = sp_io_reader_from_mem(embed.memory.buffer, embed.memory.size);
        break;
      }
      case SPN_EMBED_FILE: {
        if (!sp_fs_exists(embed.file.path)) {
          spn_event_buffer_push_ex(spn.events, unit->pkg, &unit->logs, (spn_build_event_t) {
            .kind = SPN_EVENT_EMBED_FAILED,
            .embed_failed = { .path = embed.file.path, .error = sp_str_lit("file not found") },
          });
          return SPN_ERROR;
        }

        io = sp_io_reader_from_file(embed.file.path);

        if (sp_str_empty(symbol)) {
          symbol = spn_cc_symbol_from_embedded_file(embed.file.path);
        }
        break;
      }
      case SPN_EMBED_DIR: {
        sp_da(sp_fs_entry_t) entries = sp_fs_collect_recursive(embed.dir.path);
        sp_da_for(entries, e) {
          if (!sp_fs_is_regular_file(entries[e].file_path)) continue;
          sp_str_t rel = sp_str_suffix(entries[e].file_path, entries[e].file_path.len - embed.dir.path.len - 1);
          sp_io_reader_t dir_io = sp_io_reader_from_file(entries[e].file_path);
          if (spn_cc_embed_ctx_add(&embedder, dir_io, spn_cc_symbol_from_embedded_file(rel), rel, types.data, types.size)) {
            spn_event_buffer_push_ex(spn.events, unit->pkg, &unit->logs, (spn_build_event_t) {
              .kind = SPN_EVENT_EMBED_FAILED,
              .embed_failed = { .path = entries[e].file_path, .error = sp_str_lit("embed add failed") },
            });
            return SPN_ERROR;
          }
        }
        continue;
      }
    }

    sp_str_t path = embed.kind == SPN_EMBED_FILE ? embed.file.path : sp_str_lit("");
    if (spn_cc_embed_ctx_add(&embedder, io, symbol, path, types.data, types.size)) {
      spn_event_buffer_push_ex(spn.events, unit->pkg, &unit->logs, (spn_build_event_t) {
        .kind = SPN_EVENT_EMBED_FAILED,
        .embed_failed = { .error = sp_str_lit("embed add failed") },
      });
      return SPN_ERROR;
    }
  }

  sp_str_t obj = get_embed_object_path(unit);
  sp_str_t hdr = get_embed_header_path(unit);

  if (spn_cc_embed_ctx_write(&embedder, obj, hdr)) {
    spn_event_buffer_push_ex(spn.events, unit->pkg, &unit->logs, (spn_build_event_t) {
      .kind = SPN_EVENT_EMBED_FAILED,
      .embed_failed = { .error = sp_str_lit("embed write failed") },
    });
    return SPN_ERROR;
  }

  u64 elapsed = sp_tm_read_timer(&timer);
  spn_event_buffer_push_ex(spn.events, unit->pkg, &unit->logs, (spn_build_event_t) {
    .kind = SPN_EVENT_EMBED_PASSED,
    .embed_passed = { .object_path = obj, .header_path = hdr, .time = elapsed },
  });

  return SPN_OK;
}

void emit_link_passed(spn_target_unit_t* unit, sp_str_t output, u64 elapsed) {
  spn_event_buffer_push_ex(spn.events, unit->pkg, &unit->logs, (spn_build_event_t) {
    .kind = SPN_EVENT_LINK_PASSED,
    .target.name = unit->info->name,
    .target.link_passed = {
      .output_path = output,
      .time = elapsed,
    }
  });
}

void emit_link_failed(spn_target_unit_t* unit, sp_str_t linker, sp_str_t args, s32 exit_code, sp_str_t out, sp_str_t err) {
  spn_event_buffer_push_ex(spn.events, unit->pkg, &unit->logs, (spn_build_event_t) {
    .kind = SPN_EVENT_LINK_FAILED,
    .target.name = unit->info->name,
    .target.link_failed = {
      .exit_code = exit_code,
      .out = out,
      .err = err,
      .linker = linker,
      .args = args,
    }
  });
}

s32 link_target(spn_bg_cmd_t* cmd, void* user_data) {
  spn_target_unit_t* unit = (spn_target_unit_t*)user_data;
  spn_target_t* target = unit->info;

  sp_str_t output = get_target_output_path(unit);
  sp_str_t output_name = sp_fs_get_name(output);
  bool has_embeds = !sp_da_empty(target->embed);

  switch (target->kind) {
    case SPN_TARGET_STATIC_LIB: {
      spn_ar_run_result_t run = run_ar_exec(unit, output);

      spn_event_buffer_push_ex(spn.events, unit->pkg, &unit->logs, (spn_build_event_t) {
        .kind = SPN_EVENT_LINK_START,
        .target.name = target->name,
        .target.link_start = {
          .kind = target->kind,
          .num_objects = sp_da_size(unit->objects),
          .output_path = output,
          .linker = sp_str_lit("ar"),
          .args = run.args,
          .has_embeds = has_embeds,
        }
      });

      if (run.result.status.exit_code) {
        emit_link_failed(unit, sp_str_lit("ar"), run.args, run.result.status.exit_code, run.result.out, run.result.err);
        return SPN_ERROR;
      }

      emit_link_passed(unit, output, run.elapsed);
      return SPN_OK;
    }
    case SPN_TARGET_EXE:
    case SPN_TARGET_SHARED_LIB: {
      spn_cc_t* cc = make_cc_for_compile_or_link(unit->pkg, unit->info, sp_fs_parent_path(output), unit->session->profile);
      cc->toolchain = unit->session->toolchain;
      spn_cc_target_t* cc_target = spn_cc_add_target(cc, target->kind, output_name);
      setup_target_for_compile_or_link(cc, cc_target, target, unit->pkg, unit->session);

      sp_da_for(unit->objects, it) {
        spn_cc_target_add_absolute_source(cc_target, unit->objects[it]->paths.object);
      }

      if (has_embeds) {
        spn_cc_target_add_absolute_source(cc_target, get_embed_object_path(unit));
      }

      sp_str_t linker = spn_toolchain_get_linker_driver(cc->toolchain.info);
      spn_cc_run_result_t run = run_cc_exec(cc, cc_target, unit->paths.work);

      spn_event_buffer_push_ex(spn.events, unit->pkg, &unit->logs, (spn_build_event_t) {
        .kind = SPN_EVENT_LINK_START,
        .target.name = target->name,
        .target.link_start = {
          .kind = target->kind,
          .num_objects = sp_da_size(unit->objects),
          .output_path = output,
          .linker = linker,
          .args = run.args,
          .has_embeds = has_embeds,
        }
      });

      s32 rc = run.result.status.exit_code;
      if (rc) {
        emit_link_failed(unit, linker, run.args, rc, run.result.out, run.result.err);
        return SPN_ERROR;
      }

      emit_link_passed(unit, output, run.elapsed);
      return SPN_OK;
    }
    case SPN_TARGET_NONE:
    case SPN_TARGET_JIT:
    case SPN_TARGET_OBJECT: {
      SP_UNREACHABLE_CASE();
    }
  }

  SP_UNREACHABLE_RETURN(SPN_ERROR);
}

s32 run_package_hook(spn_bg_cmd_t* cmd, void* user_data) {
  spn_pkg_unit_t* ctx = (spn_pkg_unit_t*)user_data;

  if (ctx->on_package) {
    spn_event_buffer_push(spn.events, &ctx->ctx, SPN_EVENT_BUILD_SCRIPT_PACKAGE);

    sp_tm_timer_t timer = sp_tm_start_timer();
    spn_try(spn_pkg_unit_call_hook(ctx, ctx->on_package));
    ctx->time.package = sp_tm_read_timer(&timer);

    spn_event_buffer_push_ctx(spn.events, &ctx->ctx, (spn_build_event_t) {
      .kind = SPN_EVENT_BUILD_SCRIPT_PACKAGE_OK,
      .package_ok = { .time = ctx->time.package },
    });
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
  unit->nodes.output = spn_bg_add_file_ex(graph, get_target_output_path(unit), SPN_BG_VIZ_BINARY, pkg->ctx.name);

  sp_try(spn_bg_cmd_add_output(graph, unit->nodes.link,  unit->nodes.output));
  //spn_bg_cmd_add_input(graph, unit->nodes.link, pkg->nodes.build.stamp.exit);
  //spn_bg_cmd_add_input(graph, unit->nodes.compile, pkg->nodes.build.stamp.main);
  sp_try(spn_bg_cmd_add_input(graph, pkg->nodes.build.package, unit->nodes.output));

  sp_da_for(unit->objects, it) {
    spn_compile_unit_t* obj = unit->objects[it];
    sp_try(spn_bg_cmd_add_input(graph, unit->nodes.link, obj->nodes.object));
  }

  if (!sp_da_empty(target->embed)) {
    unit->nodes.embed.run = spn_bg_add_fn_ex(graph, compile_embed, unit, SPN_BG_VIZ_CMD, pkg->ctx.name, sp_format("embed::{}", SP_FMT_STR(target->name)));
    unit->nodes.embed.object = spn_bg_add_file_ex(graph, get_embed_object_path(unit), SPN_BG_VIZ_DEFAULT, pkg->ctx.name);
    unit->nodes.embed.header = spn_bg_add_file_ex(graph, get_embed_header_path(unit), SPN_BG_VIZ_DEFAULT, pkg->ctx.name);

    sp_try(spn_bg_cmd_add_output(graph, unit->nodes.embed.run, unit->nodes.embed.object));
    sp_try(spn_bg_cmd_add_output(graph, unit->nodes.embed.run, unit->nodes.embed.header));
    sp_try(spn_bg_cmd_add_input(graph, unit->nodes.embed.run, pkg->nodes.build.stamp.exit));

    sp_da_for(target->embed, it) {
      spn_embed_t* embed = &target->embed[it];

      switch (embed->kind) {
        case SPN_EMBED_FILE: {
          spn_bg_id_t input = get_or_put_user_file(pkg, graph, embed->file.path);
          sp_try(spn_bg_cmd_add_input(graph, unit->nodes.embed.run, input));
          break;
        }
        case SPN_EMBED_MEM:
        case SPN_EMBED_DIR: {
          break;
        }
      }
    }

    sp_try(spn_bg_cmd_add_input(graph, unit->nodes.link, unit->nodes.embed.object));

    sp_da_for(unit->objects, it) {
      spn_compile_unit_t* obj = unit->objects[it];
      sp_try(spn_bg_cmd_add_input(graph, obj->nodes.compile, unit->nodes.embed.header));
    }
  }

  return SPN_OK;
}

bool should_build_target(spn_target_t* target) {
  switch (target->kind) {
    case SPN_TARGET_STATIC_LIB:
    case SPN_TARGET_SHARED_LIB: {
      return !sp_da_empty(target->source);
    }
    case SPN_TARGET_NONE:
      return false;
    case SPN_TARGET_OBJECT:
    case SPN_TARGET_EXE:
    case SPN_TARGET_JIT: {
      return true;
    }
  }

  SP_UNREACHABLE_RETURN(false);
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
    sp_try(spn_bg_cmd_add_input(graph, obj->nodes.compile, obj->nodes.source));
    sp_try(spn_bg_cmd_add_input(graph, obj->nodes.compile, unit->nodes.build.stamp.exit));
  }

  sp_om_for(unit->targets, it) {
    spn_target_unit_t* target = sp_om_at(unit->targets, it);
    if (!should_build_target(target->info)) {
      continue;
    }

    sp_try(add_target(graph, unit, target));
  }

  return SPN_OK;
}

static spn_err_union_t prepare_build_graph(spn_app_t* app) {
  spn_session_t* session = &app->session;
  spn_build_graph_t* graph = &session->build.graph;

  // phase 1: add each package to the graph
  sp_om_for(session->units.packages, it) {
    spn_pkg_unit_t* unit = sp_om_at(session->units.packages, it);
    if (add_package(graph, unit) != SPN_OK) {
      return spn_bg_error_to_union(graph);
    }
  }
  if (add_package(graph, spn_session_find_root(session)) != SPN_OK) {
    return spn_bg_error_to_union(graph);
  }

  // phase 2: link dependent packages
  sp_om_for(session->units.packages, it) {
    if (add_link_edges(session, graph, sp_om_at(session->units.packages, it)) != SPN_OK) {
      return spn_bg_error_to_union(graph);
    }
  }
  if (add_link_edges(session, graph, spn_session_find_root(session)) != SPN_OK) {
    return spn_bg_error_to_union(graph);
  }

  return spn_result(SPN_OK);
}

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
