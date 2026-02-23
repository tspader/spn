#include "app.h"
#include "gen.h"
#include "external/cc.h"
#include "external/git.h"

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

s32 spn_executor_user_fn(spn_bg_cmd_t* cmd, void* user_data) {
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

s32 spn_executor_build_target_unit_2(spn_bg_cmd_t* cmd, void* user_data) {
  // @target run the compiler, log and emit as needed
  return SPN_OK;
}

s32 spn_executor_compile_object(spn_bg_cmd_t* cmd, void* user_data) {
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

s32 spn_executor_link_target(spn_bg_cmd_t* cmd, void* user_data) {
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

s32 spn_executor_build_pkg(spn_bg_cmd_t* cmd, void* user_data) {
  spn_pkg_unit_t* unit = (spn_pkg_unit_t*)user_data;

  spn_build_graph_t* graph = spn_bg_new();
  spn_bg_add_package(graph, unit);

  spn_bg_dirty_t* dirty = spn_bg_compute_dirty(graph);
  spn_bg_executor_t* executor = spn_bg_executor_new(graph, dirty, (spn_bg_executor_config_t) {
    .num_threads = 1
  });
  spn_bg_executor_run(executor);
  spn_bg_executor_join(executor);

  s32 result = SPN_OK;
  if (sp_da_size(executor->errors)) {
    result = SPN_ERROR;
  }

  return result;
}

s32 spn_executor_run_package_hook(spn_bg_cmd_t* cmd, void* user_data) {
  spn_pkg_unit_t* ctx = (spn_pkg_unit_t*)user_data;

  spn_event_buffer_push(spn.events, &ctx->ctx, SPN_EVENT_BUILD_SCRIPT_PACKAGE);

  if (spn_pkg_unit_run_package_hook(ctx)) {
    spn_event_buffer_push(spn.events, &ctx->ctx, SPN_EVENT_BUILD_SCRIPT_PACKAGE_FAILED);
    return SPN_ERROR;
  }

  spn_pkg_unit_write_stamp(ctx, ctx->paths.stamp.package);

  return SPN_OK;
}

s32 spn_executor_write_enter_stamp(spn_bg_cmd_t* cmd, void* user_data) {
  spn_pkg_unit_t* unit = (spn_pkg_unit_t*)user_data;
  spn_pkg_unit_write_stamp(unit, unit->paths.stamp.main);
  return SPN_OK;
}

s32 spn_executor_write_exit_stamp(spn_bg_cmd_t* cmd, void* user_data) {
  spn_pkg_unit_t* unit = (spn_pkg_unit_t*)user_data;
  spn_pkg_unit_write_stamp(unit, unit->paths.stamp.exit);
  return SPN_OK;
}

