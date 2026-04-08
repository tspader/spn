#include "autoconf.h"

#include "session/types.h"
#include "triple/triple.h"
#include "unit/build.h"

s32 spn_autoconf(spn_build_ctx_t* build) {
  spn_autoconf_t* autoconf = spn_autoconf_new(build);
  return spn_autoconf_run(autoconf);
}

spn_autoconf_t* spn_autoconf_new(spn_build_ctx_t* build) {
  spn_autoconf_t* autoconf = SP_ALLOC(spn_autoconf_t);
  // autoconf->build = build;
  return autoconf;
}

s32 spn_autoconf_run(spn_autoconf_t* autoconf) {
  return 1;
  // spn_build_ctx_t* build = autoconf->build;
  //
  // sp_ps_config_t config = {
  //   .command = sp_fs_join_path(spn_ctx_build_source_dir(build), SP_LIT("configure")),
  //   .args = {
  //     sp_format("--prefix={}", SP_FMT_STR(spn_ctx_build_store_dir(build))),
  //     spn_ctx_build_linkage(build) == SPN_LIB_KIND_SHARED ?
  //       SP_LIT("--enable-shared") :
  //       SP_LIT("--disable-shared"),
  //     spn_ctx_build_linkage(build) == SPN_LIB_KIND_STATIC ?
  //       SP_LIT("--enable-static") :
  //       SP_LIT("--disable-static"),
  //   },
  // };
  //
  // // Pass --build and --host when cross-compiling
  // spn_triple_t host = spn_triple_host();
  // spn_triple_t target = { build->session->profile.arch, build->session->profile.os, build->session->profile.abi };
  // if (!spn_triple_match(target, host) || !spn_triple_match(host, target)) {
  //   sp_ps_config_add_arg(&config, sp_format("--build={}", SP_FMT_STR(spn_triple_to_autoconf(host))));
  //   sp_ps_config_add_arg(&config, sp_format("--host={}", SP_FMT_STR(spn_triple_to_autoconf(target))));
  // }
  //
  // sp_da_for(autoconf->flags, it) {
  //   sp_ps_config_add_arg(&config, autoconf->flags[it]);
  // }
  //
  // sp_ps_output_t result = spn_ctx_build_subprocess(build, config);
  // return result.status.exit_code;
}

void spn_autoconf_add_flag(spn_autoconf_t* autoconf, const c8* flag) {
  sp_da_push(autoconf->flags, sp_str_from_cstr(flag));
}
