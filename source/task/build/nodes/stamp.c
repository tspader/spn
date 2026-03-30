#include "graph/types.h"
#include "unit/types.h"
#include "unit/package.h"

s32 stamp_enter(spn_bg_cmd_t* cmd, void* user_data) {
  spn_pkg_unit_t* unit = (spn_pkg_unit_t*)user_data;
  spn_pkg_unit_write_stamp(unit, unit->paths.stamp.main);
  return SPN_OK;
}

s32 stamp_exit(spn_bg_cmd_t* cmd, void* user_data) {
  spn_pkg_unit_t* unit = (spn_pkg_unit_t*)user_data;
  spn_pkg_unit_write_stamp(unit, unit->paths.stamp.exit);
  return SPN_OK;
}

