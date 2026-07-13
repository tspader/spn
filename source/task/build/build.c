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

void add_deps_to_cc_target(spn_cc_link_t* link, spn_target_unit_t* target) {
  spn_session_t* session = target->pkg->session;
  spn_pkg_unit_t* pkg = target->pkg;

  sp_mem_arena_marker_t s = sp_mem_begin_scratch();

  // Sibling targets named in this target's deps come first: they may lean on
  // the package closure below, never the reverse
  sp_da_for(target->deps.target, it) {
    spn_target_unit_t* lib = target->deps.target[it];
    if (lib->info->no_link) continue;

    switch (lib->lib_kind) {
      case SPN_LIB_KIND_STATIC: {
        sp_da_push(link->lib_dirs, lib->pkg->paths.lib);
        sp_da_push(link->system_libs, lib->info->name);
        break;
      }
      case SPN_LIB_KIND_SHARED: {
        sp_da_push(link->lib_dirs, lib->pkg->paths.lib);
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
  sp_str_t path = sp_fs_join_path(mem, unit->pkg->paths.generated, name);
  sp_mem_end_scratch(s);
  return path;
}

sp_str_t get_embed_header_path(sp_mem_t mem, spn_target_unit_t* unit) {
  sp_mem_arena_marker_t s = sp_mem_begin_scratch_for(mem);
  sp_str_t name = sp_fmt(s.mem, "{}.embed.h", SP_FMT_STR(unit->info->name)).value;
  sp_str_t path = sp_fs_join_path(mem, unit->pkg->paths.generated, name);
  sp_mem_end_scratch(s);
  return path;
}

sp_str_t get_target_staged_path(sp_mem_t mem, spn_target_unit_t* target) {
  if (target->kind != SPN_CC_OUTPUT_EXE) return sp_zero_s(sp_str_t);

  switch (target->info->kind) {
    case SPN_TARGET_EXE:
    case SPN_TARGET_SCRIPT: {
      return sp_fs_join_path(mem, target->pkg->build->paths.root, target->info->name);
    }
    case SPN_TARGET_TEST: {
      sp_str_t dir = sp_fs_join_path(mem, target->pkg->build->paths.root, SP_LIT("test"));
      return sp_fs_join_path(mem, dir, target->info->name);
    }
    case SPN_TARGET_LIB:
    case SPN_TARGET_CONFIGURE_METAPROGRAM:
    case SPN_TARGET_BUILD_METAPROGRAM: {
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
      return sp_fs_join_path(mem, target->pkg->paths.bin, info->name);
    }
    case SPN_CC_OUTPUT_STATIC_LIB: {
      sp_str_t file_name = sp_os_lib_to_file_name(s.mem, info->name, SP_OS_LIB_STATIC);
      path = sp_fs_join_path(mem, target->pkg->paths.lib, file_name);
      break;
    }
    case SPN_CC_OUTPUT_SHARED_LIB: {
      sp_str_t file_name = sp_os_lib_to_file_name(s.mem, info->name, SP_OS_LIB_SHARED);
      path = sp_fs_join_path(mem, target->pkg->paths.lib, file_name);
      break;
    }
    case SPN_CC_OUTPUT_REACTOR: {
      sp_str_t file_name = sp_fmt(s.mem, "{}.wasm", SP_FMT_STR(info->name)).value;
      path = sp_fs_join_path(mem, target->pkg->paths.generated, file_name);
      break;
    }
    case SPN_CC_OUTPUT_OBJECT: {
      sp_unreachable_case();
    }
  }

  sp_mem_end_scratch(s);
  return path;
}
