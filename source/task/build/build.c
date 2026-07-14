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
#include "triple/triple.h"
#include "unit/compiler.h"

static void push_frameworks(sp_da(sp_str_t)* frameworks, sp_da(sp_str_t) values) {
  sp_da_for(values, it) {
    bool present = false;
    sp_da_for(*frameworks, jt) {
      if (sp_str_equal((*frameworks)[jt], values[it])) {
        present = true;
        break;
      }
    }
    if (!present) {
      sp_da_push(*frameworks, values[it]);
    }
  }
}

static spn_os_version_t max_os_version(spn_os_version_t current, spn_os_version_t candidate) {
  return spn_os_version_less(current, candidate) ? candidate : current;
}

spn_os_version_t spn_target_macos_min_os(spn_target_unit_t* target) {
  sp_mem_arena_marker_t s = sp_mem_begin_scratch();

  spn_os_version_t best = target->info->macos.min_os;
  best = max_os_version(best, target->pkg->info->macos.min_os);

  sp_da_for(target->deps.target, it) {
    best = max_os_version(best, target->deps.target[it]->info->macos.min_os);
  }

  sp_da(spn_closure_entry_t) closure = spn_target_link_closure(s.mem, target);
  sp_da_for(closure, it) {
    spn_pkg_unit_t* dep = closure[it].pkg;
    if (!dep || dep == target->pkg) continue;
    best = max_os_version(best, dep->info->macos.min_os);
    sp_da_for(dep->libs, lt) {
      best = max_os_version(best, dep->libs[lt]->info->macos.min_os);
    }
  }

  sp_mem_end_scratch(s);
  return best;
}

void add_deps_to_cc_target(spn_cc_link_t* link, spn_target_unit_t* target) {
  spn_session_t* session = target->pkg->session;
  spn_pkg_unit_t* pkg = target->pkg;

  sp_mem_arena_marker_t s = sp_mem_begin_scratch();

  push_frameworks(&link->frameworks, target->info->macos.frameworks);
  push_frameworks(&link->frameworks, pkg->info->macos.frameworks);

  sp_da_for(target->deps.target, it) {
    spn_target_unit_t* lib = target->deps.target[it];
    if (lib->info->no_link) continue;

    if (lib->lib_kind != SPN_LIB_KIND_SHARED) {
      push_frameworks(&link->frameworks, lib->info->macos.frameworks);
    }

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

    if (libs[it].private && target->kind == SPN_CC_OUTPUT_SHARED_LIB) {
      sp_da_push(link->hidden_libs, lib->info->name);
    }
    else {
      sp_da_push(link->system_libs, lib->info->name);
    }
  }

  sp_da_for(pkg->info->system_deps, it) {
    sp_da_push(link->system_libs, pkg->info->system_deps[it]);
  }
  sp_da_for(deps, it) {
    spn_pkg_unit_t* dep = deps[it].pkg;
    if (!dep || dep == pkg) continue;

    sp_da_for(dep->info->system_deps, s) {
      sp_da_push(link->system_libs, dep->info->system_deps[s]);
    }
    bool static_link = false;
    sp_da_for(dep->libs, lt) {
      spn_target_unit_t* lib = dep->libs[lt];
      if (lib->info->no_link) continue;
      if (lib->lib_kind == SPN_LIB_KIND_SHARED) continue;
      static_link = true;
      push_frameworks(&link->frameworks, lib->info->macos.frameworks);
    }
    if (static_link || sp_da_empty(dep->libs)) {
      push_frameworks(&link->frameworks, dep->info->macos.frameworks);
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

static spn_triple_t get_target_triple(spn_target_unit_t* target) {
  spn_profile_info_t* profile = &target->pkg->build->profile;
  return (spn_triple_t) { profile->arch, profile->os, profile->abi };
}

sp_str_t get_target_staged_path(sp_mem_t mem, spn_target_unit_t* target) {
  if (target->kind != SPN_CC_OUTPUT_EXE) return sp_zero_s(sp_str_t);

  sp_mem_arena_marker_t s = sp_mem_begin_scratch_for(mem);
  sp_str_t file_name = spn_triple_exe_file_name(s.mem, get_target_triple(target), target->info->name);

  sp_str_t path = sp_zero;
  switch (target->info->kind) {
    case SPN_TARGET_EXE:
    case SPN_TARGET_SCRIPT: {
      path = sp_fs_join_path(mem, target->pkg->build->paths.root, file_name);
      break;
    }
    case SPN_TARGET_TEST: {
      sp_str_t dir = sp_fs_join_path(s.mem, target->pkg->build->paths.root, SP_LIT("test"));
      path = sp_fs_join_path(mem, dir, file_name);
      break;
    }
    case SPN_TARGET_LIB:
    case SPN_TARGET_CONFIGURE_METAPROGRAM:
    case SPN_TARGET_BUILD_METAPROGRAM: {
      break;
    }
  }

  sp_mem_end_scratch(s);
  return path;
}

sp_str_t get_target_output_path(sp_mem_t mem, spn_target_unit_t* target) {
  spn_target_info_t* info = target->info;

  sp_mem_arena_marker_t s = sp_mem_begin_scratch_for(mem);

  sp_str_t path = sp_zero;

  switch (target->kind) {
    case SPN_CC_OUTPUT_EXE: {
      sp_str_t file_name = spn_triple_exe_file_name(s.mem, get_target_triple(target), info->name);
      path = sp_fs_join_path(mem, target->pkg->paths.bin, file_name);
      break;
    }
    case SPN_CC_OUTPUT_STATIC_LIB: {
      sp_str_t file_name = spn_triple_lib_file_name(s.mem, get_target_triple(target), info->name, SP_OS_LIB_STATIC);
      path = sp_fs_join_path(mem, target->pkg->paths.lib, file_name);
      break;
    }
    case SPN_CC_OUTPUT_SHARED_LIB: {
      sp_str_t file_name = spn_triple_lib_file_name(s.mem, get_target_triple(target), info->name, SP_OS_LIB_SHARED);
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
