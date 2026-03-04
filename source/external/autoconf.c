#include "autoconf.h"

#include "unit/build.h"

void spn_autoconf(spn_build_ctx_t* build) {
  spn_autoconf_t* autoconf = spn_autoconf_new(build);
  spn_autoconf_run(autoconf);
}

spn_autoconf_t* spn_autoconf_new(spn_build_ctx_t* build) {
  spn_autoconf_t* autoconf = SP_ALLOC(spn_autoconf_t);
  autoconf->build = build;
  return autoconf;
}

void spn_autoconf_run(spn_autoconf_t* autoconf) {
  spn_build_ctx_t* build = autoconf->build;

  sp_ps_config_t config = {
    .command = sp_fs_join_path(spn_ctx_build_source_dir(build), SP_LIT("configure")),
    .args = {
      sp_format("--prefix={}", SP_FMT_STR(spn_ctx_build_store_dir(build))),
      spn_ctx_build_linkage(build) == SPN_LIB_KIND_SHARED ?
        SP_LIT("--enable-shared") :
        SP_LIT("--disable-shared"),
      spn_ctx_build_linkage(build) == SPN_LIB_KIND_STATIC ?
        SP_LIT("--enable-static") :
        SP_LIT("--disable-static"),
    },
  };

  sp_da_for(autoconf->flags, it) {
    sp_ps_config_add_arg(&config, autoconf->flags[it]);
  }

  spn_ctx_build_subprocess(build, config);
}

void spn_autoconf_add_flag(spn_autoconf_t* autoconf, const c8* flag) {
  sp_da_push(autoconf->flags, sp_str_from_cstr(flag));
}
