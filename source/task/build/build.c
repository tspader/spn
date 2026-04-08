#include "error/types.h"

#include "gen.h"
#include "enum/enum.h"
#include "filter/filter.h"
#include "pkg/types.h"
#include "session/session.h"
#include "task/build/build.h"

void add_pkg_to_cc(spn_cc_t* cc, spn_pkg_info_t* pkg) {
  sp_da_for(pkg->include, it) {
    spn_cc_add_include(cc, pkg->include[it]);
  }

  sp_da_for(pkg->define, it) {
    spn_cc_add_define(cc, pkg->define[it]);
  }
}

void add_pkg_to_cc_target(spn_cc_target_t* target, spn_pkg_unit_t* pkg, spn_target_info_t* info) {
  sp_da_for(info->include, i) {
    spn_cc_target_add_absolute_include(target, sp_fs_join_path(pkg->paths.source, info->include[i]));
  }

  sp_da_for(info->define, i) {
    spn_cc_target_add_define(target, info->define[i]);
  }
}

void add_deps_to_cc_target(spn_cc_target_t* cc, spn_target_unit_t* target) {
  spn_session_t* s = target->session;
  spn_target_info_t* info = target->info;
  spn_pkg_info_t* pkg = target->pkg;
  spn_cc_driver_t driver = cc->cc->driver;

  sp_da_for(pkg->system_deps, i) {
    spn_cc_target_add_lib(cc, spn_gen_format_entry(pkg->system_deps[i], SPN_GEN_SYSTEM_LIBS, driver));
  }

  sp_da_for(info->deps, it) {
    sp_str_t name = info->deps[it];
    if (sp_om_has(pkg->libs, name)) {
      spn_target_info_t* lib = sp_om_get(pkg->libs, name);
    }
    else {
      spn_pkg_unit_t* dep = spn_session_find_pkg(s, name);
      spn_cc_target_add_dep(cc, dep);
      sp_da_for(dep->info->system_deps, j) {
        sp_str_t system_dep = dep->info->system_deps[j];
        spn_cc_target_add_lib(cc, spn_gen_format_entry(system_dep, SPN_GEN_SYSTEM_LIBS, driver));
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

sp_str_t get_target_output_path(spn_target_unit_t* unit) {
  spn_target_info_t* target = unit->info;

  switch (target->kind) {
    case SPN_TARGET_EXE: {
      return sp_fs_join_path(unit->paths.bin, target->name);
    }
    case SPN_TARGET_STATIC_LIB:
    case SPN_TARGET_SHARED_LIB: {
      spn_linkage_t linkage = spn_target_kind_to_pkg_linkage(target->kind);
      sp_str_t file = sp_os_lib_to_file_name(target->name, spn_lib_kind_to_sp_os_lib_kind(linkage));
      return sp_fs_join_path(unit->paths.lib, file);
    }
    case SPN_TARGET_NONE:
    case SPN_TARGET_JIT:
    case SPN_TARGET_OBJECT: {
      SP_UNREACHABLE_CASE();
    }
  }

  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

void invoke_compiler();
