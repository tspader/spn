#include "sp.h"
#include "macro/macro.h"
#include "cc.h"
#include "ctx/types.h"
#include "spn/core.h"
#include "event/types.h"
#include "core/types.h"
#include "unit/types.h"

#include "compiler/driver.h"
#include "enum/enum.h"
#include "event/event.h"
#include "filter/filter.h"
#include "paths/paths.h"
#include "pkg/types.h"
#include "session/invocation.h"
#include "session/session.h"
#include "unit/unit.h"
#include "graph/build.h"
#include "triple/triple.h"


static spn_triple_t get_target_triple(spn_target_unit_t* target) {
  spn_profile_info_t* profile = &target->pkg->build->profile;
  return (spn_triple_t) { profile->arch, profile->os, profile->abi };
}

spn_path_t spn_target_exports_path(sp_mem_t mem, spn_target_unit_t* target) {
  spn_cc_exports_format_t format = spn_cc_exports_format(target->kind, target->pkg->build->profile.os);

  sp_mem_arena_marker_t s = sp_mem_begin_scratch_for(mem);
  sp_str_t file_name = sp_fmt(s.mem, "{}.{}", sp_fmt_str(target->info->name), sp_fmt_cstr(spn_cc_exports_extension(format))).value;
  spn_path_t path = spn_path_join(mem, target->pkg->paths.work, file_name);
  sp_mem_end_scratch(s);
  return path;
}

spn_path_t spn_target_exports_archive(sp_mem_t mem, spn_path_t exports) {
  return spn_path_suffix(mem, exports, sp_str_lit(".a"));
}

spn_path_t spn_target_unit_staged_path(sp_mem_t mem, spn_target_unit_t* target) {
  if (target->kind != SPN_CC_OUTPUT_EXE) return sp_zero_s(spn_path_t);

  sp_mem_arena_marker_t s = sp_mem_begin_scratch_for(mem);
  sp_str_t file_name = spn_triple_exe_file_name(s.mem, get_target_triple(target), target->info->name);
  spn_path_t root = target->pkg->build->paths.root;

  spn_path_t path = sp_zero;
  switch (target->info->kind) {
    case SPN_TARGET_KIND_EXE:
    case SPN_TARGET_KIND_SCRIPT: {
      path = spn_path_join(mem, root, file_name);
      break;
    }
    case SPN_TARGET_KIND_TEST: {
      path = spn_path_join(mem, spn_path_join(s.mem, root, SP_LIT("test")), file_name);
      break;
    }
    case SPN_TARGET_KIND_EXAMPLE: {
      path = spn_path_join(mem, spn_path_join(s.mem, root, SP_LIT("example")), file_name);
      break;
    }
    case SPN_TARGET_KIND_LIB:
    case SPN_TARGET_KIND_CONFIGURE_METAPROGRAM:
    case SPN_TARGET_KIND_BUILD_METAPROGRAM: {
      break;
    }
  }

  sp_mem_end_scratch(s);
  return path;
}

spn_path_t spn_target_output_path(sp_mem_t mem, spn_target_unit_t* target) {
  spn_target_info_t* info = target->info;

  sp_mem_arena_marker_t s = sp_mem_begin_scratch_for(mem);

  spn_path_t path = sp_zero;

  switch (target->kind) {
    case SPN_CC_OUTPUT_EXE: {
      sp_str_t file_name = spn_triple_exe_file_name(s.mem, get_target_triple(target), info->name);
      path = spn_path_join(mem, target->pkg->paths.bin, file_name);
      break;
    }
    case SPN_CC_OUTPUT_STATIC_LIB: {
      sp_str_t file_name = spn_triple_lib_file_name(s.mem, get_target_triple(target), info->name, SP_OS_LIB_STATIC);
      path = spn_path_join(mem, target->pkg->paths.lib, file_name);
      break;
    }
    case SPN_CC_OUTPUT_SHARED_LIB: {
      sp_str_t file_name = spn_triple_lib_file_name(s.mem, get_target_triple(target), info->name, SP_OS_LIB_SHARED);
      path = spn_path_join(mem, target->pkg->paths.lib, file_name);
      break;
    }
    case SPN_CC_OUTPUT_REACTOR: {
      sp_str_t file_name = sp_fmt(s.mem, "{}.wasm", sp_fmt_str(info->name)).value;
      path = spn_path_join(mem, target->pkg->paths.work, file_name);
      break;
    }
    case SPN_CC_OUTPUT_OBJECT: {
      sp_unreachable_case();
    }
  }

  sp_mem_end_scratch(s);
  return path;
}
