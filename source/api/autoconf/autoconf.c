#include "api/autoconf/autoconf.h"

#include "api/api.h"
#include "api/core/types.h"
#include "session/types.h"
#include "unit/types.h"

#include "triple/triple.h"

s32 spn_autoconf(spn_t* build) {
  spn_autoconf_t* autoconf = spn_autoconf_new(build);
  return spn_autoconf_run(autoconf);
}

spn_autoconf_t* spn_autoconf_new(spn_t* build) {
  spn_autoconf_t* autoconf = SP_ALLOC(spn_autoconf_t);
  autoconf->build = build;
  return autoconf;
}

s32 spn_autoconf_run(spn_autoconf_t* autoconf) {
  spn_pkg_unit_t* unit = spn_api_unit(autoconf->build);
  spn_profile_info_t* profile = &unit->session->profile;

  sp_ps_config_t config = {
    .command = sp_fs_join_path(spn_mem_todo, unit->paths.source, sp_str_lit("configure")),
    .args = {
      sp_format("--prefix={}", SP_FMT_STR(unit->paths.store)),
      profile->linkage == SPN_LIB_KIND_SHARED ?
        sp_str_lit("--enable-shared") :
        sp_str_lit("--disable-shared"),
      profile->linkage == SPN_LIB_KIND_STATIC ?
        sp_str_lit("--enable-static") :
        sp_str_lit("--disable-static"),
    },
  };

  // Pass --build and --host when cross-compiling
  spn_triple_t host = spn_triple_host();
  spn_triple_t target = { profile->arch, profile->os, profile->abi };
  if (!spn_triple_match(target, host) || !spn_triple_match(host, target)) {
    sp_ps_config_add_arg(spn_mem_todo, &config, sp_format("--build={}", SP_FMT_STR(spn_triple_to_autoconf(host))));
    sp_ps_config_add_arg(spn_mem_todo, &config, sp_format("--host={}", SP_FMT_STR(spn_triple_to_autoconf(target))));
  }

  sp_da_for(autoconf->flags, it) {
    sp_ps_config_add_arg(spn_mem_todo, &config, autoconf->flags[it]);
  }

  sp_ps_output_t result = spn_api_subprocess(unit, config);
  return result.status.exit_code;
}

void spn_autoconf_add_flag(spn_autoconf_t* autoconf, const c8* flag) {
  sp_da_push(autoconf->flags, sp_str_from_cstr(spn_mem_todo, flag));
}
