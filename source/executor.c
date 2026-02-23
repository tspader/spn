#include "app.h"

#include "cc.h"
#include "external/git.h"
#include "gen.h"

static spn_cc_t* spn_make_cc_for_compile_or_link(spn_pkg_t* pkg, sp_str_t path, spn_profile_t* profile) {
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

static void spn_setup_target_for_compile_or_link(spn_cc_t* cc, spn_cc_target_t* cc_target, spn_target_t* target, spn_pkg_t* pkg, spn_session_t* session) {
  sp_da_for(target->include, it) {
    spn_cc_target_add_relative_include(cc_target, target->include[it]);
  }

  sp_da_for(target->define, it) {
    spn_cc_target_add_define(cc_target, target->define[it]);
  }

  sp_da_for(pkg->system_deps, it) {
    spn_cc_target_add_lib(cc_target, spn_gen_format_entry(pkg->system_deps[it], SPN_GEN_SYSTEM_LIBS, cc->compiler.kind));
  }

  sp_ht_for_kv(pkg->deps, it) {
    if (spn_is_visibility_linked(target->visibility, it.val->visibility)) {
      spn_pkg_unit_t* dep = spn_session_find_pkg(session, *it.key);
      spn_cc_target_add_dep(cc_target, dep);

      sp_da_for(dep->ctx.pkg->system_deps, n) {
        spn_cc_target_add_lib(cc_target, spn_gen_format_entry(dep->ctx.pkg->system_deps[n], SPN_GEN_SYSTEM_LIBS, cc->compiler.kind));
      }
    }
  }
}

static spn_err_t spn_executor_run_cc(spn_cc_t* cc, spn_cc_target_t* cc_target, sp_str_t cwd, spn_pkg_t* pkg, spn_build_io_t* io) {
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

  spn_event_buffer_push_ex(spn.events, pkg, io, (spn_build_event_t) {
    .kind = SPN_EVENT_TARGET_BUILD_PASSED
  });
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
  message = sp_str_replace_c8(message, '{', '[');
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

s32 spn_executor_compile_object(spn_bg_cmd_t* cmd, void* user_data) {
  spn_compile_unit_t* unit = (spn_compile_unit_t*)user_data;

  spn_event_buffer_push_ex(spn.events, unit->pkg, &unit->target->logs, (spn_build_event_t) {
    .kind = SPN_EVENT_DEBUG,
    .debug = {
      .message = unit->paths.object
    }
  });

  sp_str_t file = sp_fs_get_name(unit->paths.object);

  spn_cc_t* cc = spn_make_cc_for_compile_or_link(unit->pkg, unit->target->paths.object, unit->profile);
  spn_cc_target_t* cc_target = spn_cc_add_target(cc, SPN_TARGET_OBJECT, file);
  spn_setup_target_for_compile_or_link(cc, cc_target, unit->target->info, unit->pkg, unit->session);
  spn_cc_target_add_absolute_source(cc_target, unit->paths.source);

  return spn_executor_run_cc(cc, cc_target, unit->target->paths.work, unit->pkg, &unit->target->logs);
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

  spn_cc_t* cc = spn_make_cc_for_compile_or_link(unit->pkg, unit->paths.bin, unit->session->profile);
  spn_cc_target_t* cc_target = spn_cc_add_target(cc, SPN_TARGET_EXE, target->name);
  spn_setup_target_for_compile_or_link(cc, cc_target, target, unit->pkg, unit->session);

  sp_da_for(unit->objects, it) {
    spn_cc_target_add_absolute_source(cc_target, unit->objects[it]->paths.object);
  }

  return spn_executor_run_cc(cc, cc_target, unit->paths.work, unit->pkg, &unit->logs);
}
