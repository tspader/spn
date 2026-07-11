#include "compiler/flags.h"

#include "enum/enum.h"
#include "spn.h"

static sp_str_t debug_switch(spn_build_mode_t mode, spn_cc_driver_t driver) {
  if (mode != SPN_BUILD_MODE_DEBUG) {
    return sp_str_lit("");
  }
  return driver == SPN_CC_DRIVER_MSVC ? sp_str_lit("/Zi") : sp_str_lit("-g");
}

static sp_str_t opt_switch(spn_opt_level_t level, spn_cc_driver_t driver) {
  switch (driver) {
    case SPN_CC_DRIVER_NONE:
    case SPN_CC_DRIVER_GCC:
    case SPN_CC_DRIVER_CLANG: {
      switch (level) {
        case SPN_OPT_LEVEL_0: return sp_str_lit("-O0");
        case SPN_OPT_LEVEL_1: return sp_str_lit("-O1");
        case SPN_OPT_LEVEL_2: return sp_str_lit("-O2");
        case SPN_OPT_LEVEL_3: return sp_str_lit("-O3");
        case SPN_OPT_LEVEL_S: return sp_str_lit("-Os");
        case SPN_OPT_LEVEL_Z: return sp_str_lit("-Oz");
        case SPN_OPT_LEVEL_NONE: return sp_str_lit("");
      }
      SP_UNREACHABLE_RETURN(sp_str_lit(""));
    }
    case SPN_CC_DRIVER_MSVC: {
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
  }
  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

static sp_str_t sanitizer_switch(sp_mem_t mem, spn_sanitizer_set_t sanitizers, spn_cc_driver_t driver) {
  if (!sanitizers) {
    return sp_str_lit("");
  }
  sp_str_t value = spn_sanitizer_set_to_str(mem, sanitizers);
  switch (driver) {
    case SPN_CC_DRIVER_NONE:
    case SPN_CC_DRIVER_GCC:
    case SPN_CC_DRIVER_CLANG: {
      return sp_fmt(mem, "-fsanitize={}", sp_fmt_str(value)).value;
    }
    case SPN_CC_DRIVER_MSVC: {
      return sp_fmt(mem, "/fsanitize={}", sp_fmt_str(value)).value;
    }
  }
  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

static sp_str_t release_define(spn_build_mode_t mode, spn_cc_driver_t driver) {
  if (mode != SPN_BUILD_MODE_RELEASE) {
    return sp_str_lit("");
  }
  return driver == SPN_CC_DRIVER_MSVC ? sp_str_lit("/DNDEBUG") : sp_str_lit("-DNDEBUG");
}

static spn_sanitizer_set_t supported_sanitizers(spn_cc_driver_t driver, spn_triple_t target) {
  switch (target.os) {
    case SPN_OS_WASI: {
      return 0;
    }
    case SPN_OS_WINDOWS: {
      if (target.abi != SPN_ABI_MSVC || target.arch != SPN_ARCH_X64) {
        return 0;
      }
      return driver == SPN_CC_DRIVER_CLANG || driver == SPN_CC_DRIVER_MSVC ? SPN_SANITIZER_ADDRESS : 0;
    }
    case SPN_OS_MACOS: {
      switch (driver) {
        case SPN_CC_DRIVER_CLANG: {
          return SPN_SANITIZER_ADDRESS | SPN_SANITIZER_THREAD | SPN_SANITIZER_UNDEFINED | SPN_SANITIZER_LEAK;
        }
        case SPN_CC_DRIVER_GCC: {
          return SPN_SANITIZER_ADDRESS | SPN_SANITIZER_UNDEFINED | SPN_SANITIZER_LEAK;
        }
        case SPN_CC_DRIVER_NONE:
        case SPN_CC_DRIVER_MSVC: {
          return 0;
        }
      }
      SP_UNREACHABLE_RETURN(0);
    }
    case SPN_OS_LINUX:
    case SPN_OS_NONE: {
      switch (driver) {
        case SPN_CC_DRIVER_GCC: {
          return SPN_SANITIZER_ADDRESS | SPN_SANITIZER_THREAD | SPN_SANITIZER_UNDEFINED | SPN_SANITIZER_LEAK;
        }
        case SPN_CC_DRIVER_CLANG: {
          return SPN_SANITIZER_ADDRESS | SPN_SANITIZER_THREAD | SPN_SANITIZER_UNDEFINED | SPN_SANITIZER_MEMORY | SPN_SANITIZER_LEAK;
        }
        case SPN_CC_DRIVER_NONE:
        case SPN_CC_DRIVER_MSVC: {
          return 0;
        }
      }
      SP_UNREACHABLE_RETURN(0);
    }
  }
  SP_UNREACHABLE_RETURN(0);
}

static void push_flag(sp_da(sp_str_t)* flags, sp_str_t flag) {
  if (!sp_str_empty(flag)) {
    sp_da_push(*flags, flag);
  }
}

spn_err_union_t spn_cc_flags_resolve(
  sp_mem_t mem,
  const spn_profile_info_t* profile,
  const spn_toolchain_t* toolchain,
  spn_cc_flags_t* flags
) {
  sp_da_init(mem, flags->compile);
  sp_da_init(mem, flags->link);

  spn_triple_t target = { profile->arch, profile->os, profile->abi };
  spn_sanitizer_set_t unsupported = profile->sanitizers & ~supported_sanitizers(toolchain->driver, target);
  if (unsupported) {
    return (spn_err_union_t) {
      .kind = SPN_ERR_SANITIZER_UNSUPPORTED,
      .sanitizer = {
        .toolchain = toolchain->name,
        .target = target,
        .unsupported = unsupported,
      },
    };
  }

  push_flag(&flags->compile, debug_switch(profile->mode, toolchain->driver));
  push_flag(&flags->compile, opt_switch(profile->opt, toolchain->driver));
  push_flag(&flags->compile, release_define(profile->mode, toolchain->driver));
  sp_str_t sanitizer = sanitizer_switch(mem, profile->sanitizers, toolchain->driver);
  push_flag(&flags->compile, sanitizer);
  push_flag(&flags->link, sanitizer);
  return spn_result(SPN_OK);
}
