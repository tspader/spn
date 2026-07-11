#include "sp.h"
#include "sp/macro.h"
#include "gen.h"

#include "spn.h"
#include "enum/enum.h"
#include "profile/types.h"
#include "toolchain/types.h"

spn_gen_entry_t spn_gen_entry_from_str(sp_str_t str) {
  if      (sp_str_equal_cstr(str, "")) return SPN_GEN_ALL;
  else if (sp_str_equal_cstr(str, "include")) return SPN_GEN_INCLUDE;
  else if (sp_str_equal_cstr(str, "lib-include")) return SPN_GEN_LIB_INCLUDE;
  else if (sp_str_equal_cstr(str, "libs")) return SPN_GEN_LIBS;
  else if (sp_str_equal_cstr(str, "system-libs")) return SPN_GEN_SYSTEM_LIBS;

  SP_FATAL("Unknown flag {.yellow}; options are [include, lib-include, libs, system-libs]", SP_FMT_STR(str));
  SP_UNREACHABLE_RETURN(SPN_GEN_ALL);
}

spn_gen_kind_t spn_gen_kind_from_str(sp_str_t str) {
  if      (sp_str_equal_cstr(str, "")) return SPN_GEN_KIND_RAW;
  else if (sp_str_equal_cstr(str, "shell")) return SPN_GEN_KIND_SHELL;
  else if (sp_str_equal_cstr(str, "make")) return SPN_GEN_KIND_MAKE;
  else if (sp_str_equal_cstr(str, "cmake")) return SPN_GEN_KIND_CMAKE;
  else if (sp_str_equal_cstr(str, "pkgconfig")) return SPN_GEN_KIND_PKGCONFIG;

  SP_FATAL("Unknown generator {.yellow}; options are [[empty], shell, make, cmake, pkgconfig]", SP_FMT_STR(str));
  SP_UNREACHABLE_RETURN(SPN_GEN_KIND_RAW);
}

sp_str_t spn_cc_lib_kind_to_switch(spn_linkage_t kind, spn_os_t os) {
  if (os == SPN_OS_MACOS) {
    return sp_str_lit("");
  }
  switch (kind) {
    case SPN_LIB_KIND_STATIC: return sp_str_lit("-static");
    case SPN_LIB_KIND_NONE:
    case SPN_LIB_KIND_SHARED:
    case SPN_LIB_KIND_SOURCE:
    case SPN_LIB_KIND_OBJECT: {
      return sp_str_lit("");
    }
  }
  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

sp_str_t spn_cc_c_standard_to_switch(spn_c_standard_t standard) {
  switch (standard) {
    case SPN_C89: return sp_str_lit("-std=c89");
    case SPN_C99: return sp_str_lit("-std=c99");
    case SPN_C11: return sp_str_lit("-std=c11");
    case SPN_C_STANDARD_NONE: return sp_str_lit("-std=c99");
  }
  sp_unreachable(); return sp_str_lit("");
}

sp_str_t spn_cc_cxx_standard_to_switch(spn_cxx_standard_t standard) {
  switch (standard) {
    case SPN_CXX11: return sp_str_lit("-std=c++11");
    case SPN_CXX14: return sp_str_lit("-std=c++14");
    case SPN_CXX17: return sp_str_lit("-std=c++17");
    case SPN_CXX20: return sp_str_lit("-std=c++20");
    case SPN_CXX23: return sp_str_lit("-std=c++23");
    case SPN_CXX_STANDARD_NONE: return sp_str_lit("-std=c++17");
  }
  sp_unreachable(); return sp_str_lit("");
}

sp_str_t spn_cc_build_mode_to_switch(spn_build_mode_t mode, spn_cc_driver_t driver) {
  switch (mode) {
    case SPN_BUILD_MODE_DEBUG: {
      return driver == SPN_CC_DRIVER_MSVC ? sp_str_lit("/Zi") : sp_str_lit("-g");
    }
    case SPN_BUILD_MODE_RELEASE:
    case SPN_BUILD_MODE_NONE: return sp_str_lit("");
  }
  sp_unreachable(); return sp_str_lit("");
}

sp_str_t spn_cc_opt_level_to_switch(spn_opt_level_t level, spn_cc_driver_t driver) {
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
      sp_unreachable(); return sp_str_lit("");
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
      sp_unreachable(); return sp_str_lit("");
    }
  }
  sp_unreachable(); return sp_str_lit("");
}

spn_sanitizer_set_t spn_cc_driver_supported_sanitizers(spn_cc_driver_t driver) {
  switch (driver) {
    case SPN_CC_DRIVER_NONE:
    case SPN_CC_DRIVER_GCC: {
      return SPN_SANITIZER_ADDRESS | SPN_SANITIZER_THREAD | SPN_SANITIZER_UNDEFINED | SPN_SANITIZER_LEAK;
    }
    case SPN_CC_DRIVER_CLANG: {
      return SPN_SANITIZER_ADDRESS | SPN_SANITIZER_THREAD | SPN_SANITIZER_UNDEFINED | SPN_SANITIZER_MEMORY | SPN_SANITIZER_LEAK;
    }
    case SPN_CC_DRIVER_MSVC: {
      return SPN_SANITIZER_ADDRESS;
    }
  }
  sp_unreachable(); return 0;
}

sp_str_t spn_cc_sanitizers_to_switch(sp_mem_t mem, spn_sanitizer_set_t sanitizers, spn_cc_driver_t driver) {
  if (!sanitizers) {
    return sp_str_lit("");
  }
  switch (driver) {
    case SPN_CC_DRIVER_NONE:
    case SPN_CC_DRIVER_GCC:
    case SPN_CC_DRIVER_CLANG: {
      return sp_fmt(mem, "-fsanitize={}", sp_fmt_str(spn_sanitizer_set_to_str(mem, sanitizers))).value;
    }
    case SPN_CC_DRIVER_MSVC: {
      return sp_fmt(mem, "/fsanitize={}", sp_fmt_str(spn_sanitizer_set_to_str(mem, sanitizers))).value;
    }
  }
  sp_unreachable(); return sp_str_lit("");
}

spn_sanitizer_set_t spn_cc_target_supported_sanitizers(spn_os_t os, spn_abi_t abi) {
  spn_sanitizer_set_t all = SPN_SANITIZER_ADDRESS | SPN_SANITIZER_THREAD | SPN_SANITIZER_UNDEFINED | SPN_SANITIZER_MEMORY | SPN_SANITIZER_LEAK;
  switch (os) {
    case SPN_OS_WASI: {
      return 0;
    }
    case SPN_OS_WINDOWS: {
      return abi == SPN_ABI_MSVC ? SPN_SANITIZER_ADDRESS : 0;
    }
    case SPN_OS_LINUX:
    case SPN_OS_MACOS:
    case SPN_OS_NONE: {
      return all;
    }
  }
  sp_unreachable(); return 0;
}

sp_str_t spn_cc_profile_to_flags(sp_mem_t mem, const spn_profile_info_t* profile, spn_cc_driver_t driver) {
  sp_str_t flags [] = {
    spn_cc_build_mode_to_switch(profile->mode, driver),
    spn_cc_opt_level_to_switch(profile->opt, driver),
    spn_cc_sanitizers_to_switch(mem, profile->sanitizers, driver),
  };

  sp_io_dyn_mem_writer_t out;
  sp_io_dyn_mem_writer_init(mem, &out);
  bool first = true;
  sp_carr_for(flags, it) {
    if (sp_str_empty(flags[it])) {
      continue;
    }
    sp_fmt_io(&out.base, first ? "{}" : " {}", sp_fmt_str(flags[it]));
    first = false;
  }
  return sp_io_dyn_mem_writer_as_str(&out);
}

sp_str_t spn_cc_kind_to_executable(spn_cc_kind_t compiler) {
  switch (compiler) {
    case SPN_CC_TCC: return sp_str_lit("tcc");
    case SPN_CC_GCC: return sp_str_lit("gcc");
    case SPN_CC_CLANG: return sp_str_lit("clang");
    case SPN_CC_MUSL_GCC: return sp_str_lit("musl-gcc");
    case SPN_CC_COSMOCC: return sp_str_lit("cosmocc");
    case SPN_CC_ZIG: return sp_str_lit("zcc");
    case SPN_CC_CUSTOM: SP_FALLTHROUGH();
    case SPN_CC_NONE: return sp_str_lit("gcc");
  }
  sp_unreachable(); return sp_str_lit("");
}

sp_str_t spn_gen_format_entry_kernel(sp_str_map_context_t* context) {
  spn_gen_format_context_t* format = (spn_gen_format_context_t*)context->user_data;
  return spn_gen_format_entry(context->mem, context->str, format->kind, format->driver);
}

sp_str_t spn_gen_format_entry(sp_mem_t mem, sp_str_t entry, spn_gen_entry_t kind, spn_cc_driver_t driver) {
  switch (driver) {
    case SPN_CC_DRIVER_NONE: // @toolchain
    case SPN_CC_DRIVER_CLANG:
    case SPN_CC_DRIVER_GCC: {
      switch (kind) {
        case SPN_GEN_INCLUDE: return sp_fmt(mem, "-I{}", SP_FMT_STR(entry)).value;
        case SPN_GEN_LIB_INCLUDE: return sp_fmt(mem, "-L{}", SP_FMT_STR(entry)).value;
        case SPN_GEN_LIBS: return sp_fmt(mem, "{}", SP_FMT_STR(entry)).value;
        case SPN_GEN_SYSTEM_LIBS: return sp_fmt(mem, "-l{}", SP_FMT_STR(entry)).value;
        case SPN_GEN_RPATH: return sp_fmt(mem, "-Wl,-rpath,{}", SP_FMT_STR(entry)).value;
        case SPN_GEN_DEFINE: return sp_fmt(mem, "-D{}", SP_FMT_STR(entry)).value;
        case SPN_GEN_NONE:
        case SPN_GEN_ALL: {
          sp_unreachable_case();
        }
      }
      case SPN_CC_DRIVER_MSVC: {
        sp_unreachable_case();
      }
    }
  }

  sp_unreachable(); return sp_str_lit("");
}

