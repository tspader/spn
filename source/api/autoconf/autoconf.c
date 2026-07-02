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
  sp_mem_t mem = spn_api_unit(build)->session->mem;
  spn_autoconf_t* autoconf = sp_alloc_type(mem, spn_autoconf_t);
  *autoconf = (spn_autoconf_t) {
    .mem = mem,
    .build = build,
  };
  sp_da_init(mem, autoconf->flags);
  return autoconf;
}

s32 spn_autoconf_run(spn_autoconf_t* autoconf) {
  spn_pkg_unit_t* unit = spn_api_unit(autoconf->build);
  spn_profile_info_t* profile = &unit->session->profile;

  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();

  sp_ps_config_t config = {
    .command = sp_fs_join_path(scratch.mem, unit->paths.source, sp_str_lit("configure")),
    .args = {
      sp_fmt(scratch.mem, "--prefix={}", sp_fmt_str(unit->paths.store)).value,
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
    sp_ps_config_add_arg(scratch.mem, &config, sp_fmt(scratch.mem, "--build={}", sp_fmt_str(spn_triple_to_autoconf(scratch.mem, host))).value);
    sp_ps_config_add_arg(scratch.mem, &config, sp_fmt(scratch.mem, "--host={}", sp_fmt_str(spn_triple_to_autoconf(scratch.mem, target))).value);
  }

  sp_da_for(autoconf->flags, it) {
    sp_ps_config_add_arg(scratch.mem, &config, autoconf->flags[it]);
  }

  sp_ps_output_t result = spn_api_subprocess(scratch.mem, unit, config);
  s32 exit_code = result.status.exit_code;
  sp_mem_end_scratch(scratch);
  return exit_code;
}

void spn_autoconf_add_flag(spn_autoconf_t* autoconf, const c8* flag) {
  sp_da_push(autoconf->flags, sp_str_from_cstr(autoconf->mem, flag));
}
