#include "gen.h"

#include "spn.h"
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

sp_str_t spn_cc_lib_kind_to_switch(spn_linkage_t kind) {
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

sp_str_t spn_cc_build_mode_to_switch(spn_build_mode_t mode) {
  switch (mode) {
    case SPN_BUILD_MODE_DEBUG: return sp_str_lit("-g");
    case SPN_BUILD_MODE_RELEASE:
    case SPN_BUILD_MODE_NONE: return sp_str_lit("");
  }
  sp_unreachable(); return sp_str_lit("");
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
  return spn_gen_format_entry(context->str, format->kind, format->driver);
}

sp_str_t spn_gen_format_entry(sp_str_t entry, spn_gen_entry_t kind, spn_cc_driver_t driver) {
  switch (driver) {
    case SPN_CC_DRIVER_NONE: // @toolchain
    case SPN_CC_DRIVER_CLANG:
    case SPN_CC_DRIVER_GCC: {
      switch (kind) {
        case SPN_GEN_INCLUDE: return sp_format("-I{}", SP_FMT_STR(entry));
        case SPN_GEN_LIB_INCLUDE: return sp_format("-L{}", SP_FMT_STR(entry));
        case SPN_GEN_LIBS: return sp_format("{}", SP_FMT_STR(entry));
        case SPN_GEN_SYSTEM_LIBS: return sp_format("-l{}", SP_FMT_STR(entry));
        case SPN_GEN_RPATH: return sp_format("-Wl,-rpath,{}", SP_FMT_STR(entry));
        case SPN_GEN_DEFINE: return sp_format("-D{}", SP_FMT_STR(entry));
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

