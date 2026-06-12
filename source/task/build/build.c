#include "cc.h"
#include "error/types.h"
#include "forward/types.h"
#include "unit/types.h"

#include "gen.h"
#include "enum/enum.h"
#include "filter/filter.h"
#include "pkg/types.h"
#include "session/session.h"
#include "task/build/build.h"

// Manifest includes are source-relative; the build script API hands us absolute paths
static sp_str_t resolve_pkg_include(spn_pkg_unit_t* pkg, sp_str_t include) {
  if (sp_str_starts_with(include, sp_str_lit("/"))) return include;
  return sp_fs_join_path(pkg->paths.source, include);
}

void add_pkg_to_cc(spn_cc_t* cc, spn_pkg_unit_t* pkg) {
  sp_da_for(pkg->info->include, it) {
    spn_cc_add_include(cc, resolve_pkg_include(pkg, pkg->info->include[it]));
  }

  sp_da_for(pkg->info->define, it) {
    spn_cc_add_define(cc, pkg->info->define[it]);
  }
}

void add_pkg_to_cc_target(spn_cc_target_t* target, spn_pkg_unit_t* pkg, spn_target_info_t* info) {
  sp_da_for(info->include, i) {
    spn_cc_target_add_absolute_include(target, resolve_pkg_include(pkg, info->include[i]));
  }

  sp_da_for(info->define, i) {
    spn_cc_target_add_define(target, info->define[i]);
  }

  sp_da_for(info->flags, i) {
    spn_cc_target_add_flag(target, info->flags[i]);
  }
}

void add_deps_to_cc_target(spn_cc_target_t* cc, spn_target_unit_t* target) {
  spn_session_t* session = target->session;
  spn_pkg_unit_t* pkg = target->pkg;

  sp_da_for(pkg->info->system_deps, it) {
    spn_cc_target_add_system_lib(cc, pkg->info->system_deps[it]);
  }

  sp_da(spn_pkg_unit_t*) deps = spn_session_pkg_deps(session, pkg);
  sp_da_for(deps, it) {
    spn_pkg_unit_t* dep = deps[it];
    if (!dep || dep == pkg) continue;

    sp_da_for(dep->info->system_deps, s) {
      spn_cc_target_add_system_lib(cc, dep->info->system_deps[s]);
    }

    sp_da_for(dep->libs, l) {
      spn_target_unit_t* lib = dep->libs[l];
      if (lib->info->no_link) continue;

      switch (lib->lib_kind) {
        case SPN_LIB_KIND_STATIC: {
          spn_cc_target_add_lib_dir(cc, dep->paths.lib);
          spn_cc_target_add_system_lib(cc, lib->info->name);
          break;
        }
        case SPN_LIB_KIND_SHARED: {
          spn_cc_target_add_lib_dir(cc, dep->paths.lib);
          spn_cc_target_add_system_lib(cc, lib->info->name);
          spn_cc_target_add_rpath(cc, dep->paths.lib);
          break;
        }
        case SPN_LIB_KIND_SOURCE:
        case SPN_LIB_KIND_OBJECT:
        case SPN_LIB_KIND_NONE: break;
      }
    }
  }
}

sp_str_t get_embed_object_path(spn_target_unit_t* unit) {
  return sp_fs_join_path(unit->paths.generated, sp_format("{}.embed.o", SP_FMT_STR(unit->info->name)));
}

sp_str_t get_embed_header_path(spn_target_unit_t* unit) {
  return sp_fs_join_path(unit->paths.generated, sp_format("{}.embed.h", SP_FMT_STR(unit->info->name)));
}

sp_str_t get_target_output_path(spn_target_unit_t* target) {
  spn_target_info_t* info = target->info;

  spn_toolchain_unit_t* toolchain = target->session->units.toolchain;
  spn_profile_info_t profile = target->session->profile;
  switch (target->kind) {
    case SPN_CC_OUTPUT_EXE: {
      return sp_fs_join_path(target->paths.bin, info->name);
    }
    case SPN_CC_OUTPUT_STATIC_LIB: {
      return sp_fs_join_path(target->paths.lib, sp_os_lib_to_file_name(info->name, SP_OS_LIB_STATIC));
    }
    case SPN_CC_OUTPUT_SHARED_LIB: {
      return sp_fs_join_path(target->paths.lib, sp_os_lib_to_file_name(info->name, SP_OS_LIB_SHARED));
    }
    case SPN_CC_OUTPUT_JIT:
    case SPN_CC_OUTPUT_OBJECT: break;
  }

  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}
