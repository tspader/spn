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

static bool objects_have_cxx(sp_da(spn_compile_unit_t*) objects) {
  sp_da_for(objects, it) {
    if (objects[it]->lang == SPN_LANG_CXX) {
      return true;
    }
  }
  return false;
}

static bool target_is_dynamic(spn_target_unit_t* target) {
  return target->kind == SPN_CC_OUTPUT_SHARED_LIB || target->kind == SPN_CC_OUTPUT_REACTOR;
}

static sp_str_t static_archive_path(sp_mem_t mem, spn_target_unit_t* lib) {
  spn_profile_info_t* profile = &lib->pkg->build->profile;
  spn_triple_t triple = { profile->arch, profile->os, profile->abi };
  sp_mem_arena_marker_t s = sp_mem_begin_scratch_for(mem);
  sp_str_t file_name = spn_triple_lib_file_name(s.mem, triple, lib->info->name, SP_OS_LIB_STATIC);
  sp_str_t path = sp_fs_join_path(mem, lib->pkg->paths.lib, file_name);
  sp_mem_end_scratch(s);
  return path;
}

void spn_target_resolve_link(spn_target_unit_t* target) {
  spn_pkg_unit_t* pkg = target->pkg;
  sp_mem_t mem = pkg->session->mem;

  sp_mem_arena_marker_t s = sp_mem_begin_scratch();

  sp_da_init(mem, target->link.lib_dirs);
  sp_da_init(mem, target->link.system_libs);
  sp_da_init(mem, target->link.whole_archives);
  sp_da_init(mem, target->link.private_libs);
  sp_da_init(mem, target->link.frameworks);

  spn_os_version_t min_os = max_os_version(target->info->macos.min_os, pkg->info->macos.min_os);
  spn_lang_t lang = objects_have_cxx(target->objects) ? SPN_LANG_CXX : SPN_LANG_C;

  push_frameworks(&target->link.frameworks, target->info->macos.frameworks);
  push_frameworks(&target->link.frameworks, pkg->info->macos.frameworks);

  sp_da_for(target->deps.target, it) {
    spn_target_unit_t* lib = target->deps.target[it];
    min_os = max_os_version(min_os, lib->info->macos.min_os);
    if (lib->info->no_link) continue;

    if (lib->lib_kind != SPN_LIB_KIND_SHARED) {
      push_frameworks(&target->link.frameworks, lib->info->macos.frameworks);
    }

    switch (lib->lib_kind) {
      case SPN_LIB_KIND_STATIC: {
        if (target_is_dynamic(target)) {
          sp_da_push(target->link.whole_archives, static_archive_path(mem, lib));
          break;
        }
        sp_da_push(target->link.lib_dirs, lib->pkg->paths.lib);
        sp_da_push(target->link.system_libs, lib->info->name);
        break;
      }
      case SPN_LIB_KIND_SHARED: {
        sp_da_push(target->link.lib_dirs, lib->pkg->paths.lib);
        sp_da_push(target->link.system_libs, lib->info->name);
        break;
      }
      case SPN_LIB_KIND_SOURCE:
      case SPN_LIB_KIND_OBJECT:
      case SPN_LIB_KIND_NONE: {
        break;
      }
    }
  }

  sp_da(spn_closure_entry_t) closure = spn_target_link_closure(s.mem, target);

  // Packages must precede the system libraries they need
  target->link.libs = spn_closure_link_libs(mem, closure, pkg);
  sp_da_for(target->link.libs, it) {
    spn_pkg_unit_t* dep = target->link.libs[it].pkg;
    spn_target_unit_t* lib = target->link.libs[it].lib;

    if (lang != SPN_LANG_CXX && lib->lib_kind == SPN_LIB_KIND_STATIC && objects_have_cxx(lib->objects)) {
      lang = SPN_LANG_CXX;
    }

    sp_da_push(target->link.lib_dirs, dep->paths.lib);

    if (lib->lib_kind == SPN_LIB_KIND_SHARED) {
      sp_da_push(target->link.system_libs, lib->info->name);
      continue;
    }

    if (!target_is_dynamic(target)) {
      sp_da_push(target->link.system_libs, lib->info->name);
    }
    else if (target->link.libs[it].private) {
      sp_da_push(target->link.private_libs, lib->info->name);
    }
    else {
      sp_da_push(target->link.whole_archives, static_archive_path(mem, lib));
    }
  }

  sp_da_for(pkg->info->system_deps, it) {
    sp_da_push(target->link.system_libs, pkg->info->system_deps[it]);
  }
  sp_da_for(closure, it) {
    spn_pkg_unit_t* dep = closure[it].pkg;
    if (!dep || dep == pkg) continue;

    min_os = max_os_version(min_os, dep->info->macos.min_os);

    sp_da_for(dep->info->system_deps, st) {
      sp_da_push(target->link.system_libs, dep->info->system_deps[st]);
    }
    bool static_link = false;
    sp_da_for(dep->libs, lt) {
      spn_target_unit_t* lib = dep->libs[lt];
      min_os = max_os_version(min_os, lib->info->macos.min_os);
      if (lib->info->no_link) continue;
      if (lib->lib_kind == SPN_LIB_KIND_SHARED) continue;
      static_link = true;
      push_frameworks(&target->link.frameworks, lib->info->macos.frameworks);
    }
    if (static_link || sp_da_empty(dep->libs)) {
      push_frameworks(&target->link.frameworks, dep->info->macos.frameworks);
    }
  }

  target->link.min_os = min_os;
  target->link.lang = lang;

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
