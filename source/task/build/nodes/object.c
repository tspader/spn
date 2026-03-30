#include "session/types.h"
#include "unit/types.h"

#include "external/cc.h"
#include "task/build/build.h"
#include "task/build/nodes/nodes.h"

s32 compile_object(spn_bg_cmd_t* cmd, void* user_data) {
  spn_compile_unit_t* unit = (spn_compile_unit_t*)user_data;

  sp_str_t file = sp_fs_get_name(unit->paths.object);
  sp_str_t dir = sp_fs_parent_path(unit->paths.object);
  sp_fs_create_dir(dir);

  spn_cc_t* cc = sp_alloc_type(spn_cc_t);
  spn_cc_set_profile(cc, unit->profile);
  spn_cc_set_output_dir(cc, dir);
  spn_cc_set_toolchain(cc, unit->session->toolchain);
  add_pkg_to_cc(cc, unit->pkg);

  spn_cc_target_t* target = spn_cc_add_target(cc, SPN_TARGET_OBJECT, file);
  add_pkg_to_cc_target(target, unit->pkg, unit->target->info);
  add_deps_to_cc_target(target, unit->pkg, unit->target->info, unit->session);

  if (!sp_da_empty(unit->target->info->embed)) {
    spn_cc_target_add_absolute_include(target, unit->target->paths.generated);
  }

  spn_cc_target_add_absolute_source(target, unit->paths.source);
  spn_cc_run_t result = spn_cc_target_run(target, unit->target->paths.work);
  return result.result.status.exit_code;
}

