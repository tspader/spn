#include "compiler/driver.h"

static spn_err_union_t unsupported(const spn_cc_toolchain_t* toolchain, const spn_profile_info_t* profile, spn_cc_feature_t feature) {
  return (spn_err_union_t) {
    .kind = SPN_ERR_COMPILER_FEATURE_UNSUPPORTED,
    .compiler = {
      .toolchain = toolchain->name,
      .target = { profile->arch, profile->os, profile->abi },
      .feature = feature,
    },
  };
}

sp_str_t spn_cc_feature_to_str(spn_cc_feature_t feature) {
  switch (feature) {
    case SPN_CC_FEATURE_COMPILE: return sp_str_lit("direct compilation");
    case SPN_CC_FEATURE_LINK_EXE: return sp_str_lit("executable linking");
    case SPN_CC_FEATURE_LINK_SHARED: return sp_str_lit("shared library linking");
    case SPN_CC_FEATURE_LINK_REACTOR: return sp_str_lit("reactor module linking");
    case SPN_CC_FEATURE_ARCHIVE: return sp_str_lit("static archiving");
    case SPN_CC_FEATURE_FRAMEWORKS: return sp_str_lit("framework linking without a macOS SDK");
  }
  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

spn_sanitizer_set_t spn_cc_supported_sanitizers(spn_cc_driver_t driver, spn_triple_t target) {
  switch (driver) {
    case SPN_CC_DRIVER_GCC: return spn_gcc_supported_sanitizers(target);
    case SPN_CC_DRIVER_CLANG: return spn_clang_supported_sanitizers(target);
    case SPN_CC_DRIVER_MSVC: return spn_msvc_supported_sanitizers(target);
    case SPN_CC_DRIVER_NONE: {
      sp_unreachable_case();
    }
  }
  SP_UNREACHABLE_RETURN(0);
}

// Zig compiles every sanitizer clang does, but only bundles the UBSan
// runtime; anything else dies at link with undefined __asan/__tsan symbols
spn_sanitizer_set_t spn_toolchain_supported_sanitizers(const spn_cc_toolchain_t* toolchain, spn_triple_t target) {
  spn_sanitizer_set_t set = spn_cc_supported_sanitizers(toolchain->driver, target);
  if (sp_str_equal_cstr(toolchain->name, "zig")) {
    set &= SPN_SANITIZER_UNDEFINED;
  }
  return set;
}

static spn_err_union_t validate_profile(const spn_cc_toolchain_t* toolchain, const spn_profile_info_t* profile) {
  spn_triple_t target = { profile->arch, profile->os, profile->abi };
  spn_sanitizer_set_t unsupported_set = profile->sanitizers & ~spn_toolchain_supported_sanitizers(toolchain, target);
  if (unsupported_set) {
    return (spn_err_union_t) {
      .kind = SPN_ERR_SANITIZER_UNSUPPORTED,
      .sanitizer = {
        .toolchain = toolchain->name,
        .target = target,
        .unsupported = unsupported_set,
      },
    };
  }

  // The runtime-backed sanitizers need a dynamic executable; -static links
  // fail with undefined _DYNAMIC. UBSan's runtime is the exception.
  spn_sanitizer_set_t static_set = profile->sanitizers & ~SPN_SANITIZER_UNDEFINED;
  bool renders_static = profile->linkage == SPN_LIB_KIND_STATIC && profile->os != SPN_OS_MACOS && toolchain->driver != SPN_CC_DRIVER_MSVC;
  if (static_set && renders_static) {
    return (spn_err_union_t) {
      .kind = SPN_ERR_SANITIZER_STATIC,
      .sanitizer = {
        .toolchain = toolchain->name,
        .target = target,
        .unsupported = static_set,
      },
    };
  }
  return spn_result(SPN_OK);
}

spn_err_union_t spn_cc_render_flags(sp_mem_t mem, const spn_cc_toolchain_t* toolchain, const spn_profile_info_t* profile, spn_cc_flags_t* flags) {
  sp_da_init(mem, flags->compile);
  sp_da_init(mem, flags->link);
  try_union(validate_profile(toolchain, profile));
  switch (toolchain->driver) {
    case SPN_CC_DRIVER_GCC:
    case SPN_CC_DRIVER_CLANG: {
      spn_gnu_render_flags(mem, profile, flags);
      break;
    }
    case SPN_CC_DRIVER_MSVC: {
      spn_msvc_render_flags(mem, profile, flags);
      break;
    }
    case SPN_CC_DRIVER_NONE: {
      sp_unreachable_case();
    }
  }
  return spn_result(SPN_OK);
}

spn_err_union_t spn_cc_render_compile(sp_mem_t mem, const spn_cc_toolchain_t* toolchain, const spn_profile_info_t* profile, const spn_cc_compile_t* compile, sp_ps_config_t* ps) {
  try_union(validate_profile(toolchain, profile));
  switch (toolchain->driver) {
    case SPN_CC_DRIVER_GCC:
    case SPN_CC_DRIVER_CLANG: {
      spn_gnu_render_compile(mem, toolchain, profile, compile, ps);
      return spn_result(SPN_OK);
    }
    case SPN_CC_DRIVER_MSVC: {
      spn_msvc_render_compile(mem, toolchain, profile, compile, ps);
      return spn_result(SPN_OK);
    }
    case SPN_CC_DRIVER_NONE: {
      sp_unreachable_case();
    }
  }
  SP_UNREACHABLE_RETURN(spn_result(SPN_ERROR));
}

static spn_cc_feature_t link_feature(spn_cc_output_kind_t kind) {
  switch (kind) {
    case SPN_CC_OUTPUT_EXE: return SPN_CC_FEATURE_LINK_EXE;
    case SPN_CC_OUTPUT_SHARED_LIB: return SPN_CC_FEATURE_LINK_SHARED;
    case SPN_CC_OUTPUT_REACTOR: return SPN_CC_FEATURE_LINK_REACTOR;
    case SPN_CC_OUTPUT_OBJECT:
    case SPN_CC_OUTPUT_STATIC_LIB: {
      sp_unreachable_case();
    }
  }
  SP_UNREACHABLE_RETURN(SPN_CC_FEATURE_LINK_EXE);
}

spn_err_union_t spn_cc_render_link(sp_mem_t mem, const spn_cc_toolchain_t* toolchain, const spn_profile_info_t* profile, const spn_cc_link_t* link, sp_ps_config_t* ps) {
  try_union(validate_profile(toolchain, profile));
  spn_cc_feature_t feature = link_feature(link->kind);
  if (link->kind == SPN_CC_OUTPUT_REACTOR && profile->os != SPN_OS_WASI) {
    return unsupported(toolchain, profile, feature);
  }
  if (link->kind == SPN_CC_OUTPUT_SHARED_LIB && profile->os == SPN_OS_WASI) {
    return unsupported(toolchain, profile, feature);
  }
  if (profile->os == SPN_OS_MACOS && !sp_da_empty(link->frameworks) && sp_str_empty(profile->sysroot)) {
    return unsupported(toolchain, profile, SPN_CC_FEATURE_FRAMEWORKS);
  }
  switch (toolchain->driver) {
    case SPN_CC_DRIVER_GCC:
    case SPN_CC_DRIVER_CLANG: {
      spn_gnu_render_link(mem, toolchain, profile, link, ps);
      return spn_result(SPN_OK);
    }
    case SPN_CC_DRIVER_MSVC: {
      if (link->kind == SPN_CC_OUTPUT_REACTOR) {
        return unsupported(toolchain, profile, feature);
      }
      spn_msvc_render_link(mem, toolchain, profile, link, ps);
      return spn_result(SPN_OK);
    }
    case SPN_CC_DRIVER_NONE: {
      sp_unreachable_case();
    }
  }
  SP_UNREACHABLE_RETURN(spn_result(SPN_ERROR));
}

spn_err_union_t spn_cc_render_archive(sp_mem_t mem, const spn_cc_toolchain_t* toolchain, const spn_profile_info_t* profile, const spn_cc_archive_t* archive, sp_ps_config_t* ps) {
  switch (toolchain->archiver_driver) {
    case SPN_AR_DRIVER_GNU: {
      spn_gnu_render_archive(mem, toolchain, archive, ps);
      return spn_result(SPN_OK);
    }
    case SPN_AR_DRIVER_MSVC: {
      spn_msvc_render_archive(mem, toolchain, archive, ps);
      return spn_result(SPN_OK);
    }
  }
  SP_UNREACHABLE_RETURN(spn_result(SPN_ERROR));
}
