#include "compiler/driver.h"

#include "enum/enum.h"

spn_sanitizer_set_t spn_msvc_supported_sanitizers(spn_triple_t target) {
  return target.os == SPN_OS_WINDOWS && target.abi == SPN_ABI_MSVC && target.arch == SPN_ARCH_X64 ? SPN_SANITIZER_ADDRESS : 0;
}

static sp_str_t opt_switch(spn_opt_level_t level) {
  switch (level) {
    case SPN_OPT_LEVEL_0: return sp_str_lit("/Od");
    case SPN_OPT_LEVEL_1: return sp_str_lit("/O1");
    case SPN_OPT_LEVEL_2:
    case SPN_OPT_LEVEL_3: return sp_str_lit("/O2");
    case SPN_OPT_LEVEL_S:
    case SPN_OPT_LEVEL_Z: return sp_str_lit("/O1");
    case SPN_OPT_LEVEL_NONE: return sp_str_lit("");
  }
  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

void spn_msvc_render_flags(sp_mem_t mem, const spn_profile_info_t* profile, spn_cc_flags_t* flags) {
  if (profile->mode == SPN_BUILD_MODE_DEBUG) {
    sp_da_push(flags->compile, sp_str_lit("/Zi"));
  }
  sp_str_t opt = opt_switch(profile->opt);
  if (!sp_str_empty(opt)) {
    sp_da_push(flags->compile, opt);
  }
  if (profile->mode == SPN_BUILD_MODE_RELEASE) {
    sp_da_push(flags->compile, sp_str_lit("/DNDEBUG"));
  }
  if (profile->sanitizers) {
    sp_str_t sanitizer = sp_fmt(mem, "/fsanitize={}", sp_fmt_str(spn_sanitizer_set_to_str(mem, profile->sanitizers))).value;
    sp_da_push(flags->compile, sanitizer);
    sp_da_push(flags->link, sanitizer);
  }
}
