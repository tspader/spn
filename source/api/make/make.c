#include "make.h"

#include "api/api.h"
#include "unit/types.h"

s32 spn_make(spn_t* build) {
  spn_make_t* make = spn_make_new(build);
  return spn_make_run(make);
}

spn_make_t* spn_make_new(spn_t* build) {
  spn_make_t* make = SP_ALLOC(spn_make_t);
  make->build = build;
  return make;
}

void spn_make_add_target(spn_make_t* make, const c8* target) {
  make->target = sp_str_from_cstr(spn_allocator, target);
}

s32 spn_make_run(spn_make_t* make) {
  spn_pkg_unit_t* unit = spn_api_unit(make->build);

  sp_ps_config_t ps = SP_ZERO_INITIALIZE();
  ps.command = sp_str_lit("make");
  sp_ps_config_add_arg(spn_allocator, &ps, sp_str_lit("--quiet"));
  sp_ps_config_add_arg(spn_allocator, &ps, sp_str_lit("--directory"));
  sp_ps_config_add_arg(spn_allocator, &ps, unit->paths.work);
  if (!sp_str_empty(make->target)) {
    sp_ps_config_add_arg(spn_allocator, &ps, make->target);
  }

  sp_ps_output_t result = spn_api_subprocess(unit, ps);
  return result.status.exit_code;
}
