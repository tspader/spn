#include "make.h"

#include "unit/build.h"

s32 spn_make(spn_build_ctx_t* build) {
  spn_make_t* make = spn_make_new(build);
  return spn_make_run(make);
}

spn_make_t* spn_make_new(spn_build_ctx_t* build) {
  spn_make_t* make = SP_ALLOC(spn_make_t);
  make->build = build;
  return make;
}

void spn_make_add_target(spn_make_t* make, const c8* target) {
  make->target = sp_str_from_cstr(target);
}

s32 spn_make_run(spn_make_t* make) {
  return 1;
  // spn_build_ctx_t* build = make->build;
  //
  // sp_ps_config_t ps = SP_ZERO_INITIALIZE();
  // ps.command = SP_LIT("make");
  // sp_ps_config_add_arg(&ps, SP_LIT("--quiet"));
  // sp_ps_config_add_arg(&ps, SP_LIT("--directory"));
  // sp_ps_config_add_arg(&ps, spn_ctx_build_work_dir(build));
  // if (!sp_str_empty(make->target)) {
  //   sp_ps_config_add_arg(&ps, make->target);
  // }
  //
  // sp_ps_output_t result = spn_ctx_build_subprocess(build, ps);
  // return result.status.exit_code;
}
