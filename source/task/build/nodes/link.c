#include "ctx/types.h"
#include "sp/sp_graph.h"
#include "spn.h"
#include "unit/types.h"
#include "session/types.h"

#include "external/cc.h"
#include "event/event.h"
#include "session/invocation.h"
#include "target/closure.h"
#include "task/build/build.h"
#include "unit/package.h"

static bool objects_have_cxx(sp_da(spn_compile_unit_t*) objects) {
  sp_da_for(objects, it) {
    if (objects[it]->lang == SPN_LANG_CXX) {
      return true;
    }
  }
  return false;
}

static spn_lang_t get_link_language(spn_target_unit_t* target) {
  if (objects_have_cxx(target->objects)) {
    return SPN_LANG_CXX;
  }

  sp_mem_arena_marker_t s = sp_mem_begin_scratch();
  spn_lang_t language = SPN_LANG_C;

  sp_da(spn_link_lib_t) libs = spn_closure_link_libs(s.mem, spn_target_link_closure(s.mem, target), target->pkg);
  sp_da_for(libs, it) {
    if (libs[it].lib->lib_kind != SPN_LIB_KIND_STATIC) {
      continue;
    }
    if (objects_have_cxx(libs[it].lib->objects)) {
      language = SPN_LANG_CXX;
      break;
    }
  }

  sp_mem_end_scratch(s);
  return language;
}

static void build_archive_invocation(spn_target_unit_t* target, sp_str_t output) {
  sp_mem_t mem = target->session->mem;
  spn_toolchain_unit_t* toolchain = target->pkg->build->toolchain;

  sp_ps_config_t ps = sp_zero_s(sp_ps_config_t);
  sp_da_for(toolchain->archiver.args, it) {
    sp_ps_config_add_arg(mem, &ps, toolchain->archiver.args[it]);
  }
  sp_ps_config_add_arg(mem, &ps, sp_str_lit("rcs"));
  sp_ps_config_add_arg(mem, &ps, output);
  sp_da_for(target->objects, it) {
    sp_ps_config_add_arg(mem, &ps, target->objects[it]->paths.object);
  }

  target->invocation = (spn_invocation_t) {
    .program = toolchain->archiver.program,
    .args = ps.dyn_args,
    .cwd = target->paths.work,
  };
}

static void build_link_invocation(spn_target_unit_t* target, sp_str_t output) {
  sp_mem_t mem = target->session->mem;

  spn_cc_t cc;
  spn_cc_init(&cc, mem);
  spn_cc_set_profile(&cc, target->pkg->build->profile);
  spn_cc_set_output_dir(&cc, sp_fs_parent_path(output));
  spn_cc_set_toolchain(&cc, target->pkg->build->toolchain);

  spn_cc_target_t* cc_target = spn_cc_add_target(&cc, target->kind, sp_fs_get_name(output));
  spn_cc_target_set_lang(cc_target, get_link_language(target));
  add_deps_to_cc_target(cc_target, target);

  switch (target->pkg->build->profile.os) {
    case SPN_OS_LINUX: {
      spn_cc_target_add_rpath(cc_target, sp_str_lit("$ORIGIN"));
      break;
    }
    case SPN_OS_MACOS: {
      spn_cc_target_add_rpath(cc_target, sp_str_lit("@loader_path"));
      break;
    }
    case SPN_OS_WINDOWS:
    case SPN_OS_WASI:
    case SPN_OS_NONE: {
      break;
    }
  }

  sp_da_for(target->objects, it) {
    spn_cc_target_add_absolute_source(cc_target, target->objects[it]->paths.object);
  }

  if (!sp_da_empty(target->info->embed)) {
    spn_cc_target_add_absolute_source(cc_target, get_embed_object_path(mem, target));
  }

  sp_ps_config_t ps = sp_zero_s(sp_ps_config_t);
  spn_cc_to_ps(mem, &cc, cc_target, &ps);
  spn_cc_target_to_ps(mem, &cc, cc_target, &ps);

  target->invocation = (spn_invocation_t) {
    .program = ps.command,
    .args = ps.dyn_args,
    .cwd = target->paths.work,
  };
}

void spn_build_link_invocations(spn_session_t* session) {
  sp_om_for(session->units.targets, it) {
    spn_target_unit_t* target = sp_om_at(session->units.targets, it);
    if (sp_da_empty(target->objects)) {
      continue;
    }

    switch (target->kind) {
      case SPN_CC_OUTPUT_STATIC_LIB: {
        build_archive_invocation(target, get_target_output_path(session->mem, target));
        break;
      }
      case SPN_CC_OUTPUT_EXE:
      case SPN_CC_OUTPUT_SHARED_LIB: {
        build_link_invocation(target, get_target_output_path(session->mem, target));
        break;
      }
      case SPN_CC_OUTPUT_WASM:
      case SPN_CC_OUTPUT_OBJECT: {
        break;
      }
    }
  }
}

spn_err_t emit_success(spn_target_unit_t* unit, sp_str_t output, sp_str_t out, u64 elapsed) {
  spn_event_buffer_push(spn.events, (spn_build_event_t) {
    .kind = SPN_EVENT_LINK_PASSED,
    .pkg = unit->pkg->info,
    .io = &unit->logs,
    .target.name = unit->info->name,
    .target.link_passed = {
      .output_path = output,
      .invocation = &unit->invocation,
      .out = out,
      .time = elapsed,
    }
  });
  return SPN_OK;
}

spn_err_t emit_failure(spn_target_unit_t* unit, s32 rc, sp_str_t out, sp_str_t err) {
  spn_event_buffer_push(spn.events, (spn_build_event_t) {
    .kind = SPN_EVENT_LINK_FAILED,
    .pkg = unit->pkg->info,
    .io = &unit->logs,
    .target.name = unit->info->name,
    .target.link_failed = {
      .exit_code = rc,
      .out = out,
      .err = err,
      .invocation = &unit->invocation,
    }
  });
  return SPN_ERROR;
}


s32 link_target(spn_bg_cmd_t* cmd, void* user_data) {
  spn_target_unit_t* target = (spn_target_unit_t*)user_data;
  spn_target_info_t* info = target->info;

  if (sp_da_empty(target->objects)) return 0;

  spn_pkg_unit_announce_compile(target->pkg);

  sp_str_t output = get_target_output_path(spn.mem, target);

  spn_event_buffer_push(spn.events, (spn_build_event_t) {
    .kind = SPN_EVENT_LINK_START,
    .pkg = target->pkg->info,
    .io = &target->logs,
    .target.name = info->name,
    .target.link_start = {
      .target = target
    }
  });

  spn_invocation_result_t run = spn_invocation_run(&target->invocation);

  if (run.result.status.exit_code) {
    return emit_failure(target, run.result.status.exit_code, run.result.out, run.result.err);
  }

  return emit_success(target, output, run.result.out, run.elapsed);
}
