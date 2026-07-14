#include "ctx/types.h"
#include "sp/sp_graph.h"
#include "spn.h"
#include "unit/types.h"
#include "session/types.h"

#include "external/cc.h"
#include "compiler/driver.h"
#include "event/event.h"
#include "session/invocation.h"
#include "target/closure.h"
#include "task/build/build.h"
#include "unit/compiler.h"
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

static spn_err_union_t render_archive_invocation(spn_target_unit_t* target, sp_str_t output) {
  sp_mem_t mem = target->pkg->session->mem;
  spn_toolchain_unit_t* toolchain = target->pkg->build->toolchain;

  sp_ps_config_t ps = sp_zero_s(sp_ps_config_t);
  spn_cc_archive_t archive = {
    .output = output,
  };
  sp_da_init(mem, archive.objects);
  sp_da_init(mem, archive.args);
  sp_da_for(target->objects, it) {
    sp_da_push(archive.objects, target->objects[it]->paths.object);
  }
  spn_profile_info_t* profile = &target->pkg->build->profile;
  spn_cc_toolchain_t compiler = spn_toolchain_unit_compiler(toolchain);
  spn_err_union_t err = spn_cc_render_archive(mem, &compiler, profile, &archive, &ps);
  if (err.kind) {
    return err;
  }

  target->invocation = (spn_invocation_t) {
    .program = ps.command,
    .args = ps.dyn_args,
    .cwd = target->pkg->paths.work,
  };
  return spn_result(SPN_OK);
}

static spn_err_union_t render_link_invocation(spn_target_unit_t* target, sp_str_t output) {
  sp_mem_t mem = target->pkg->session->mem;

  spn_cc_link_t link = {
    .lang = get_link_language(target),
    .kind = target->kind,
    .output = output,
  };
  sp_da_init(mem, link.objects);
  sp_da_init(mem, link.args);
  sp_da_init(mem, link.libs);
  sp_da_init(mem, link.system_libs);
  sp_da_init(mem, link.hidden_libs);
  sp_da_init(mem, link.lib_dirs);
  sp_da_init(mem, link.rpath);
  sp_da_init(mem, link.frameworks);
  add_deps_to_cc_target(&link, target);

  switch (target->pkg->build->profile.os) {
    case SPN_OS_LINUX: {
      sp_da_push(link.rpath, sp_str_lit("$ORIGIN"));
      break;
    }
    case SPN_OS_MACOS: {
      sp_da_push(link.rpath, sp_str_lit("@loader_path"));
      link.min_os = spn_target_macos_min_os(target);
      break;
    }
    case SPN_OS_WINDOWS: {
      link.subsystem = target->info->windows.subsystem;
      break;
    }
    case SPN_OS_WASI:
    case SPN_OS_NONE: {
      break;
    }
  }

  sp_da_for(target->objects, it) {
    sp_da_push(link.objects, target->objects[it]->paths.object);
  }

  if (!sp_da_empty(target->info->embed)) {
    sp_da_push(link.objects, get_embed_object_path(mem, target));
  }

  sp_ps_config_t ps = sp_zero_s(sp_ps_config_t);
  spn_cc_toolchain_t toolchain = spn_toolchain_unit_compiler(target->pkg->build->toolchain);
  spn_err_union_t err = spn_cc_render_link(mem, &toolchain, &target->pkg->build->profile, &link, &ps);
  if (err.kind) {
    return err;
  }

  target->invocation = (spn_invocation_t) {
    .program = ps.command,
    .args = ps.dyn_args,
    .cwd = target->pkg->paths.work,
  };
  return spn_result(SPN_OK);
}

spn_err_union_t spn_build_link_invocation(spn_target_unit_t* target) {
  if (sp_da_empty(target->objects)) {
    return spn_result(SPN_OK);
  }
  switch (target->kind) {
    case SPN_CC_OUTPUT_STATIC_LIB: {
      return render_archive_invocation(target, get_target_output_path(target->pkg->session->mem, target));
    }
    case SPN_CC_OUTPUT_EXE:
    case SPN_CC_OUTPUT_SHARED_LIB:
    case SPN_CC_OUTPUT_REACTOR: {
      return render_link_invocation(target, get_target_output_path(target->pkg->session->mem, target));
    }
    case SPN_CC_OUTPUT_OBJECT: {
      return spn_result(SPN_OK);
    }
  }
  sp_unreachable_return(spn_result(SPN_ERROR));
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
