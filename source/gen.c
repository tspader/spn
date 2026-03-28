#include "gen.h"

#include "ctx/ctx.h"
#include "spn.h"
#include "toolchain/types.h"
#include "unit/build.h"

spn_gen_entry_t spn_gen_entry_from_str(sp_str_t str) {
  if      (sp_str_equal_cstr(str, "")) return SPN_GEN_ALL;
  else if (sp_str_equal_cstr(str, "include")) return SPN_GEN_INCLUDE;
  else if (sp_str_equal_cstr(str, "lib-include")) return SPN_GEN_LIB_INCLUDE;
  else if (sp_str_equal_cstr(str, "libs")) return SPN_GEN_LIBS;
  else if (sp_str_equal_cstr(str, "system-libs")) return SPN_GEN_SYSTEM_LIBS;

  SP_FATAL("Unknown flag {:fg brightyellow}; options are [include, lib-include, libs, system-libs]", SP_FMT_QUOTED_STR(str));
  SP_UNREACHABLE_RETURN(SPN_GEN_ALL);
}

spn_gen_kind_t spn_gen_kind_from_str(sp_str_t str) {
  if      (sp_str_equal_cstr(str, "")) return SPN_GEN_KIND_RAW;
  else if (sp_str_equal_cstr(str, "shell")) return SPN_GEN_KIND_SHELL;
  else if (sp_str_equal_cstr(str, "make")) return SPN_GEN_KIND_MAKE;
  else if (sp_str_equal_cstr(str, "cmake")) return SPN_GEN_KIND_CMAKE;
  else if (sp_str_equal_cstr(str, "pkgconfig")) return SPN_GEN_KIND_PKGCONFIG;

  SP_FATAL("Unknown generator {:fg brightyellow}; options are [[empty], shell, make, cmake, pkgconfig]", SP_FMT_STR(str));
  SP_UNREACHABLE_RETURN(SPN_GEN_KIND_RAW);
}

sp_str_t spn_cc_lib_kind_to_switch(spn_linkage_t kind) {
  switch (kind) {
    case SPN_LIB_KIND_STATIC: return sp_str_lit("-static");
    case SPN_LIB_KIND_SHARED:
    case SPN_LIB_KIND_SOURCE: {
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
    default: return sp_str_lit("");
  }
}

sp_str_t spn_cc_build_mode_to_switch(spn_build_mode_t mode) {
  switch (mode) {
    case SPN_DEP_BUILD_MODE_DEBUG: return sp_str_lit("-g");
    case SPN_DEP_BUILD_MODE_RELEASE:
    default: return sp_str_lit("");
  }
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
  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

sp_str_t spn_gen_format_entry_kernel(sp_str_map_context_t* context) {
  spn_gen_format_context_t* format = (spn_gen_format_context_t*)context->user_data;
  return spn_gen_format_entry(context->str, format->kind, format->driver);
}

sp_str_t spn_gen_format_entry(sp_str_t entry, spn_gen_entry_t kind, spn_cc_driver_t driver) {
  switch (driver) {
    case SPN_CC_DRIVER_NONE: // @toolchain
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

  sp_unreachable();
  return sp_str_lit("");
}

sp_da(sp_str_t) spn_gen_build_entry(spn_build_ctx_t* build, spn_gen_entry_t kind, spn_cc_driver_t driver) {
  sp_da(sp_str_t) entries = SP_NULLPTR;

  switch (kind) {
    case SPN_GEN_INCLUDE: {
      sp_da_push(entries, spn_ctx_build_include_dir(build));
      break;
    }
    case SPN_GEN_RPATH: {
      switch (spn_ctx_build_linkage(build)) {
        case SPN_LIB_KIND_SHARED: {
          sp_da_push(entries, spn_ctx_build_lib_dir(build));
          break;
        }
        case SPN_LIB_KIND_STATIC:
        case SPN_LIB_KIND_SOURCE: {
          return entries;
        }
      }
      break;
    }
    case SPN_GEN_LIB_INCLUDE: {
      switch (spn_ctx_build_linkage(build)) {
        case SPN_LIB_KIND_SHARED: {
          sp_da_push(entries, spn_ctx_build_lib_dir(build));
          break;
        }
        case SPN_LIB_KIND_STATIC:
        case SPN_LIB_KIND_SOURCE: {
          return entries;
        }
      }
      break;
    }
    case SPN_GEN_LIBS: {
      entries = spn_ctx_build_lib_entries(build);
      break;
    }
    case SPN_GEN_SYSTEM_LIBS: {
      break;
    }
    case SPN_GEN_NONE:
    case SPN_GEN_DEFINE:
    case SPN_GEN_ALL: {
      SP_UNREACHABLE_CASE();
    }
  }

  spn_gen_format_context_t context = {
    .driver = driver,
    .kind = kind
  };
  entries = sp_str_map(entries, sp_da_size(entries), &context, spn_gen_format_entry_kernel);

  return entries;
}

sp_str_t spn_gen_build_entries_for_all(sp_da(spn_build_ctx_t*) builds, spn_gen_entry_t kind, spn_cc_driver_t driver) {
  sp_da(sp_str_t) entries = SP_NULLPTR;

  sp_da_for(builds, it) {
    spn_build_ctx_t* build = builds[it];
    sp_da(sp_str_t) dep_entries = spn_gen_build_entry(build, kind, driver);
    sp_str_t dep_flags = sp_str_join_n(dep_entries, sp_da_size(dep_entries), sp_str_lit(" "));
    if (!sp_str_empty(dep_flags)) {
      sp_da_push(entries, dep_flags);
    }
  }

  return sp_str_join_n(entries, sp_da_size(entries), sp_str_lit(" "));
}
