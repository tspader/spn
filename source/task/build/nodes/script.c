#include "graph/types.h"

#include "ctx/ctx.h"
#include "error/types.h"
#include "external/wasm/wasm.h"
#include "task/build/build.h"
#include "unit/package.h"

s32 compile_build_script(spn_bg_cmd_t* cmd, void* user_data) {
  spn_pkg_unit_t* unit = (spn_pkg_unit_t*)user_data;

  spn_pkg_unit_announce_compile(unit);
  spn_try(spn_compile_script_module(unit, &unit->info->build, unit->wasm.build.path));
  spn_try(spn_wasm_script_open(&unit->wasm.build, unit));

  return SPN_OK;
}
