#include "sp.h"
#include "sp/macro.h"
#include "cc.h"
#include "ctx/types.h"
#include "error/types.h"
#include "event/types.h"
#include "forward/types.h"
#include "unit/types.h"

#include "gen.h"
#include "enum/enum.h"
#include "event/event.h"
#include "filter/filter.h"
#include "pkg/types.h"
#include "session/invocation.h"
#include "session/session.h"
#include "target/closure.h"
#include "task/build/build.h"

// Build deps are usable from configure and build scripts, so compile against them
void spn_cc_target_add_build_deps(spn_cc_target_t* target, spn_pkg_unit_t* unit) {
  spn_session_t* session = unit->session;

  sp_da(spn_pkg_dep_t) deps = spn_session_pkg_deps(session, unit);
  sp_da_for(deps, it) {
    if (deps[it].kind != SPN_DEP_KIND_BUILD) {
      continue;
    }
    if (!deps[it].unit) {
      continue;
    }

    spn_cc_target_add_absolute_include(target, deps[it].unit->paths.include);
  }
}

spn_err_t spn_compile_script_module(spn_pkg_unit_t* unit, spn_target_info_t* script, sp_str_t output) {
  spn_session_t* session = unit->session;

  spn_cc_t* cc = sp_alloc_type(spn.mem, spn_cc_t);
  spn_cc_init(cc, spn.mem);
  spn_cc_set_profile(cc, (spn_profile_info_t) {
    .arch = SPN_ARCH_WASM32,
    .os = SPN_OS_WASI,
    .abi = SPN_ABI_NONE,
    .mode = SPN_BUILD_MODE_DEBUG,
    .linkage = SPN_LIB_KIND_SHARED,
    .standard = SPN_C99
  });
  spn_cc_set_output_dir(cc, sp_fs_parent_path(output));
  spn_cc_set_toolchain(cc, session->units.script);
  spn_cc_add_include(cc, spn.paths.include);

  spn_cc_target_t* target = spn_cc_add_target(cc, SPN_CC_OUTPUT_WASM, sp_fs_get_name(output));
  sp_da_for(script->source, it) {
    spn_cc_target_add_absolute_source(target, script->source[it]);
  }
  spn_cc_target_add_info(target, unit, script);
  spn_cc_target_add_build_deps(target, unit);
  spn_cc_target_add_flag(target, sp_str_lit("-fvisibility=hidden"));
  spn_cc_target_add_flag(target, sp_str_lit("-Wl,--export-dynamic"));
  spn_cc_target_add_flag(target, sp_str_lit("-O2"));
  // spn_cc_target_add_flag(target, sp_str_lit("-Wl,--export="));

  sp_ps_config_t ps = sp_zero_s(sp_ps_config_t);
  spn_cc_to_ps(spn.mem, cc, target, &ps);
  spn_cc_target_to_ps(spn.mem, cc, target, &ps);

  spn_invocation_t* invocation = sp_alloc_type(spn.mem, spn_invocation_t);
  *invocation = (spn_invocation_t) {
    .program = ps.command,
    .args = ps.dyn_args,
    .cwd = unit->paths.work,
  };

  spn_invocation_result_t run = spn_invocation_run(invocation);

  sp_str_t source = sp_str_join_n(spn.mem, script->source, sp_da_size(script->source), sp_str_lit(" "));
  if (run.result.status.exit_code) {
    spn_event_buffer_push_ex(session->events, unit->info, &unit->logs.io, (spn_build_event_t) {
      .kind = SPN_EVENT_TARGET_BUILD_FAILED,
      .target.failed = {
        .source_file = source,
        .object_file = output,
        .rc = run.result.status.exit_code,
        .out = run.result.out,
        .invocation = invocation,
        .time = run.elapsed,
      }
    });
    return SPN_ERROR;
  }

  spn_event_buffer_push_ex(session->events, unit->info, &unit->logs.io, (spn_build_event_t) {
    .kind = SPN_EVENT_TARGET_BUILD_PASSED,
    .target.passed = {
      .source_file = source,
      .object_file = output,
      .invocation = invocation,
      .out = run.result.out,
      .time = run.elapsed,
    }
  });

  return SPN_OK;
}

void add_deps_to_cc_target(spn_cc_target_t* cc, spn_target_unit_t* target) {
  spn_session_t* session = target->session;
  spn_pkg_unit_t* pkg = target->pkg;

  sp_mem_arena_marker_t s = sp_mem_begin_scratch();

  // Sibling targets named in this target's deps come first: they may lean on
  // the package closure below, never the reverse
  sp_da_for(target->deps.target, it) {
    spn_target_unit_t* lib = target->deps.target[it];
    if (lib->info->no_link) continue;

    switch (lib->lib_kind) {
      case SPN_LIB_KIND_STATIC: {
        spn_cc_target_add_lib_dir(cc, lib->paths.lib);
        spn_cc_target_add_system_lib(cc, lib->info->name);
        break;
      }
      case SPN_LIB_KIND_SHARED: {
        spn_cc_target_add_lib_dir(cc, lib->paths.lib);
        spn_cc_target_add_system_lib(cc, lib->info->name);
        break;
      }
      case SPN_LIB_KIND_SOURCE:
      case SPN_LIB_KIND_OBJECT:
      case SPN_LIB_KIND_NONE: {
        break;
      }
    }
  }

  sp_da(spn_closure_entry_t) deps = spn_target_link_closure(s.mem, target);

  // Packages must precede the system libraries they need
  sp_da(spn_link_lib_t) libs = spn_closure_link_libs(s.mem, deps, pkg);
  sp_da_for(libs, it) {
    spn_pkg_unit_t* dep = libs[it].pkg;
    spn_target_unit_t* lib = libs[it].lib;

    spn_cc_target_add_lib_dir(cc, dep->paths.lib);

    if (lib->lib_kind == SPN_LIB_KIND_SHARED) {
      spn_cc_target_add_system_lib(cc, lib->info->name);
      continue;
    }

    // A shared lib embedding a private static dep hides its symbols, so
    // the embedded copy can't collide with a consumer's own instance
    if (libs[it].private && target->kind == SPN_CC_OUTPUT_SHARED_LIB) {
      if (cc->cc->os == SPN_OS_MACOS) {
        spn_cc_target_add_flag(cc, sp_fmt(cc->cc->mem, "-Wl,-hidden-l{}", SP_FMT_STR(lib->info->name)).value);
      }
      else {
        spn_cc_target_add_system_lib(cc, lib->info->name);
        sp_str_t archive = sp_os_lib_to_file_name(s.mem, lib->info->name, SP_OS_LIB_STATIC);
        spn_cc_target_add_flag(cc, sp_fmt(cc->cc->mem, "-Wl,--exclude-libs,{}", SP_FMT_STR(archive)).value);
      }
    }
    else {
      spn_cc_target_add_system_lib(cc, lib->info->name);
    }
  }

  // System libraries last, so they resolve every archive above them.
  sp_da_for(pkg->info->system_deps, it) {
    spn_cc_target_add_system_lib(cc, pkg->info->system_deps[it]);
  }
  sp_da_for(deps, it) {
    spn_pkg_unit_t* dep = deps[it].pkg;
    if (!dep || dep == pkg) continue;

    sp_da_for(dep->info->system_deps, s) {
      spn_cc_target_add_system_lib(cc, dep->info->system_deps[s]);
    }
  }

  sp_mem_end_scratch(s);
}

sp_str_t get_embed_object_path(sp_mem_t mem, spn_target_unit_t* unit) {
  sp_mem_arena_marker_t s = sp_mem_begin_scratch_for(mem);
  sp_str_t name = sp_fmt(s.mem, "{}.embed.o", SP_FMT_STR(unit->info->name)).value;
  sp_str_t path = sp_fs_join_path(mem, unit->paths.generated, name);
  sp_mem_end_scratch(s);
  return path;
}

sp_str_t get_embed_header_path(sp_mem_t mem, spn_target_unit_t* unit) {
  sp_mem_arena_marker_t s = sp_mem_begin_scratch_for(mem);
  sp_str_t name = sp_fmt(s.mem, "{}.embed.h", SP_FMT_STR(unit->info->name)).value;
  sp_str_t path = sp_fs_join_path(mem, unit->paths.generated, name);
  sp_mem_end_scratch(s);
  return path;
}

sp_str_t get_target_staged_path(sp_mem_t mem, spn_target_unit_t* target) {
  if (target->pkg->source != SPN_PKG_SOURCE_ROOT) return sp_zero_s(sp_str_t);
  if (target->kind != SPN_CC_OUTPUT_EXE) return sp_zero_s(sp_str_t);

  switch (target->info->kind) {
    case SPN_TARGET_EXE:
    case SPN_TARGET_SCRIPT: {
      return sp_fs_join_path(mem, target->pkg->build->paths.profile, target->info->name);
    }
    case SPN_TARGET_TEST: {
      sp_str_t dir = sp_fs_join_path(mem, target->pkg->build->paths.profile, SP_LIT("test"));
      return sp_fs_join_path(mem, dir, target->info->name);
    }
    case SPN_TARGET_LIB: {
      return sp_zero_s(sp_str_t);
    }
  }
  sp_unreachable_return(sp_zero_s(sp_str_t));
}

sp_str_t get_target_output_path(sp_mem_t mem, spn_target_unit_t* target) {
  spn_target_info_t* info = target->info;

  sp_mem_arena_marker_t s = sp_mem_begin_scratch_for(mem);

  sp_str_t path = sp_zero;

  switch (target->kind) {
    case SPN_CC_OUTPUT_EXE: {
      return sp_fs_join_path(mem, target->paths.bin, info->name);
    }
    case SPN_CC_OUTPUT_STATIC_LIB: {
      sp_str_t file_name = sp_os_lib_to_file_name(s.mem, info->name, SP_OS_LIB_STATIC);
      path = sp_fs_join_path(mem, target->paths.lib, file_name);
      break;
    }
    case SPN_CC_OUTPUT_SHARED_LIB: {
      sp_str_t file_name = sp_os_lib_to_file_name(s.mem, info->name, SP_OS_LIB_SHARED);
      path = sp_fs_join_path(mem, target->paths.lib, file_name);
      break;
    }
    case SPN_CC_OUTPUT_WASM:
    case SPN_CC_OUTPUT_OBJECT: {
      sp_unreachable_case();
    }
  }

  sp_mem_end_scratch(s);
  return path;
}
