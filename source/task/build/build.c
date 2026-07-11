#include "sp.h"
#include "sp/macro.h"
#include "cc.h"
#include "ctx/types.h"
#include "error/types.h"
#include "event/types.h"
#include "forward/types.h"
#include "unit/types.h"

#include "compiler/driver.h"
#include "enum/enum.h"
#include "event/event.h"
#include "filter/filter.h"
#include "pkg/types.h"
#include "session/invocation.h"
#include "session/session.h"
#include "target/closure.h"
#include "task/build/build.h"
#include "unit/compiler.h"

// Build deps are usable from configure and build scripts, so compile against them
void spn_cc_compile_add_build_deps(spn_cc_compile_t* compile, spn_pkg_unit_t* unit) {
  spn_session_t* session = unit->session;

  sp_da(spn_pkg_dep_t) deps = spn_session_pkg_deps(session, unit);
  sp_da_for(deps, it) {
    if (deps[it].kind != SPN_DEP_KIND_BUILD) {
      continue;
    }
    if (!deps[it].unit) {
      continue;
    }

    sp_da_push(compile->include, deps[it].unit->paths.include);
  }
}

static sp_str_t script_object_path(sp_mem_t mem, spn_pkg_unit_t* unit, sp_str_t module, sp_str_t source) {
  sp_str_t relative = sp_str_strip_left(source, unit->paths.recipe);
  relative = sp_str_strip_left(relative, sp_str_lit("/"));
  sp_str_t object = sp_fmt(mem, "{}.o", SP_FMT_STR(relative)).value;
  sp_str_t dir = sp_fs_join_path(mem, unit->paths.generated, sp_str_lit("object"));
  dir = sp_fs_join_path(mem, dir, sp_fs_get_stem(module));
  return sp_fs_join_path(mem, dir, object);
}

spn_err_t spn_compile_script_module(spn_pkg_unit_t* unit, spn_target_info_t* script, sp_str_t output) {
  spn_session_t* session = unit->session;
  spn_profile_info_t profile = {
    .arch = SPN_ARCH_WASM32,
    .os = SPN_OS_WASI,
    .abi = SPN_ABI_NONE,
    .mode = SPN_BUILD_MODE_DEBUG,
    .opt = SPN_OPT_LEVEL_2,
    .standard = SPN_C99
  };

  spn_cc_toolchain_t toolchain = spn_toolchain_unit_compiler(session->units.script);
  spn_cc_link_t link = {
    .lang = SPN_LANG_C,
    .kind = SPN_CC_OUTPUT_REACTOR,
    .output = output,
  };
  sp_da_init(spn.mem, link.objects);
  sp_da_init(spn.mem, link.args);
  sp_da_init(spn.mem, link.libs);
  sp_da_init(spn.mem, link.system_libs);
  sp_da_init(spn.mem, link.hidden_libs);
  sp_da_init(spn.mem, link.lib_dirs);
  sp_da_init(spn.mem, link.rpath);

  sp_da_for(script->source, it) {
    sp_str_t source = script->source[it];
    sp_str_t object = script_object_path(spn.mem, unit, output, source);
    spn_lang_t lang = spn_lang_from_path(source);
    if (lang == SPN_LANG_CXX) {
      link.lang = SPN_LANG_CXX;
    }
    spn_cc_compile_t compile = {
      .lang = lang,
      .source = source,
      .output = object,
      .cxx = script->cxx,
      .visibility = SPN_SYMBOL_VISIBILITY_HIDDEN,
    };
    sp_da_init(spn.mem, compile.include);
    sp_da_init(spn.mem, compile.define);
    sp_da_init(spn.mem, compile.args);
    sp_da_push(compile.include, spn.paths.include);
    sp_da_for(script->include, include) {
      sp_str_t path = script->include[include];
      sp_da_push(compile.include, sp_fs_is_absolute(path) ? path : sp_fs_join_path(spn.mem, unit->paths.source, path));
    }
    sp_da_for(script->define, define) {
      sp_da_push(compile.define, script->define[define]);
    }
    sp_da_for(script->flags, flag) {
      sp_da_push(compile.args, script->flags[flag]);
    }
    spn_cc_compile_add_build_deps(&compile, unit);

    sp_ps_config_t compile_ps = SP_ZERO_INITIALIZE();
    spn_err_union_t err = spn_cc_render_compile(spn.mem, &toolchain, &profile, &compile, &compile_ps);
    if (err.kind) {
      spn_event_buffer_push(session->events, (spn_build_event_t) { .kind = SPN_EVENT_ERR, .err = err });
      return err.kind;
    }
    spn_invocation_t* invocation = sp_alloc_type(spn.mem, spn_invocation_t);
    *invocation = (spn_invocation_t) {
      .program = compile_ps.command,
      .args = compile_ps.dyn_args,
      .cwd = unit->paths.work,
    };
    sp_da_push(session->units.compile_commands, ((spn_compile_command_t) {
      .source = source,
      .output = object,
      .invocation = *invocation,
    }));
    spn_session_write_compile_commands(session, spn_session_compile_commands_path(session));
    sp_fs_create_dir(sp_fs_parent_path(object));
    spn_invocation_result_t compiled = spn_invocation_run(invocation);
    if (compiled.result.status.exit_code) {
      spn_event_buffer_push_ex(session->events, unit->info, &unit->logs.io, (spn_build_event_t) {
        .kind = SPN_EVENT_TARGET_BUILD_FAILED,
        .target.failed = {
          .source_file = source,
          .object_file = object,
          .rc = compiled.result.status.exit_code,
          .out = compiled.result.out,
          .invocation = invocation,
          .time = compiled.elapsed,
        }
      });
      return SPN_ERROR;
    }
    sp_da_push(link.objects, object);
  }
  sp_ps_config_t ps = sp_zero_s(sp_ps_config_t);
  spn_err_union_t err = spn_cc_render_link(spn.mem, &toolchain, &profile, &link, &ps);
  if (err.kind) {
    spn_event_buffer_push(session->events, (spn_build_event_t) { .kind = SPN_EVENT_ERR, .err = err });
    return err.kind;
  }

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

void add_deps_to_cc_target(spn_cc_link_t* link, spn_target_unit_t* target) {
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
        sp_da_push(link->lib_dirs, lib->paths.lib);
        sp_da_push(link->system_libs, lib->info->name);
        break;
      }
      case SPN_LIB_KIND_SHARED: {
        sp_da_push(link->lib_dirs, lib->paths.lib);
        sp_da_push(link->system_libs, lib->info->name);
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

    sp_da_push(link->lib_dirs, dep->paths.lib);

    if (lib->lib_kind == SPN_LIB_KIND_SHARED) {
      sp_da_push(link->system_libs, lib->info->name);
      continue;
    }

    // A shared lib embedding a private static dep hides its symbols, so
    // the embedded copy can't collide with a consumer's own instance
    if (libs[it].private && target->kind == SPN_CC_OUTPUT_SHARED_LIB) {
      sp_da_push(link->hidden_libs, lib->info->name);
    }
    else {
      sp_da_push(link->system_libs, lib->info->name);
    }
  }

  // System libraries last, so they resolve every archive above them.
  sp_da_for(pkg->info->system_deps, it) {
    sp_da_push(link->system_libs, pkg->info->system_deps[it]);
  }
  sp_da_for(deps, it) {
    spn_pkg_unit_t* dep = deps[it].pkg;
    if (!dep || dep == pkg) continue;

    sp_da_for(dep->info->system_deps, s) {
      sp_da_push(link->system_libs, dep->info->system_deps[s]);
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
    case SPN_CC_OUTPUT_OBJECT:
    case SPN_CC_OUTPUT_REACTOR: {
      sp_unreachable_case();
    }
  }

  sp_mem_end_scratch(s);
  return path;
}
