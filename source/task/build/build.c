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
