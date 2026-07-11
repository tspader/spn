#include "sp.h"
#include "sp/macro.h"
#include "make.h"

#include "api/api.h"
#include "ctx/types.h"
#include "session/types.h"
#include "unit/types.h"

s32 spn_make(spn_t* build) {
  spn_make_t* make = spn_make_new(build);
  return spn_make_run(make);
}

spn_make_t* spn_make_new(spn_t* build) {
  sp_mem_t mem = spn.mem;
  spn_make_t* make = sp_alloc_type(mem, spn_make_t);
  *make = (spn_make_t) {
    .mem = mem,
    .build = build,
  };
  return make;
}

void spn_make_add_target(spn_make_t* make, const c8* target) {
  make->target = sp_str_from_cstr(make->mem, target);
}

s32 spn_make_run(spn_make_t* make) {
  spn_pkg_unit_t* unit = spn_api_unit(make->build);

  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  sp_ps_config_t ps = SP_ZERO_INITIALIZE();
  ps.command = sp_str_lit("make");
  sp_ps_config_add_arg(scratch.mem, &ps, sp_str_lit("--quiet"));
  sp_ps_config_add_arg(scratch.mem, &ps, sp_str_lit("--directory"));
  sp_ps_config_add_arg(scratch.mem, &ps, unit->paths.work);
  if (!sp_str_empty(make->target)) {
    sp_ps_config_add_arg(scratch.mem, &ps, make->target);
  }
  sp_env_init(scratch.mem, &ps.env.env);
  spn_api_add_profile_flags_env(scratch.mem, unit->build->toolchain->toolchain->driver, &unit->build->profile, &ps.env.env);

  sp_ps_output_t result = spn_api_subprocess(scratch.mem, unit, ps);
  s32 exit_code = result.status.exit_code;
  sp_mem_end_scratch(scratch);
  return exit_code;
}
