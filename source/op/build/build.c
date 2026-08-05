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
#include "unit/unit.h"
#include "op/build/build.h"
#include "triple/triple.h"


sp_str_t spn_embed_object_path(sp_mem_t mem, spn_target_unit_t* unit) {
  sp_mem_arena_marker_t s = sp_mem_begin_scratch_for(mem);
  sp_str_t name = sp_fmt(s.mem, "{}.embed.o", sp_fmt_str(unit->info->name)).value;
  sp_str_t path = sp_fs_join_path(mem, unit->pkg->paths.generated, name);
  sp_mem_end_scratch(s);
  return path;
}

sp_str_t spn_embed_header_path(sp_mem_t mem, spn_target_unit_t* unit) {
  sp_mem_arena_marker_t s = sp_mem_begin_scratch_for(mem);
  sp_str_t name = sp_fmt(s.mem, "{}.embed.h", sp_fmt_str(unit->info->name)).value;
  sp_str_t path = sp_fs_join_path(mem, unit->pkg->paths.generated, name);
  sp_mem_end_scratch(s);
  return path;
}

static spn_triple_t get_target_triple(spn_target_unit_t* target) {
  spn_profile_info_t* profile = &target->pkg->build->profile;
  return (spn_triple_t) { profile->arch, profile->os, profile->abi };
}

sp_str_t spn_target_staged_path(sp_mem_t mem, spn_target_unit_t* target) {
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

sp_str_t spn_target_output_path(sp_mem_t mem, spn_target_unit_t* target) {
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
      sp_str_t file_name = sp_fmt(s.mem, "{}.wasm", sp_fmt_str(info->name)).value;
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

sp_str_t spn_target_exports_path(sp_mem_t mem, spn_target_unit_t* target) {
  spn_cc_exports_format_t format = spn_cc_exports_format(target->kind, target->pkg->build->profile.os);

  sp_mem_arena_marker_t s = sp_mem_begin_scratch_for(mem);
  sp_str_t file_name = sp_fmt(s.mem, "{}.{}", sp_fmt_str(target->info->name), sp_fmt_cstr(spn_cc_exports_extension(format))).value;
  sp_str_t path = sp_fs_join_path(mem, target->pkg->paths.work, file_name);
  sp_mem_end_scratch(s);
  return path;
}
