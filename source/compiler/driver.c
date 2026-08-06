#include "compiler/driver.h"
#include "compiler/types.h"

spn_sanitizer_set_t get_supported_sanitizers(const spn_cc_toolchain_t* toolchain, spn_triple_t target) {
  spn_sanitizer_set_t set = sp_zero;
  switch (toolchain->driver) {
    case SPN_CC_DRIVER_GCC: set = spn_gcc_supported_sanitizers(target); break;
    case SPN_CC_DRIVER_CLANG: set = spn_clang_supported_sanitizers(target); break;
    case SPN_CC_DRIVER_MSVC: set = spn_msvc_supported_sanitizers(target); break;
    case SPN_CC_DRIVER_NONE: sp_unreachable_case();
  }

  if (sp_str_equal_cstr(toolchain->name, "zig")) { // @spader Give zig its own driver
    set &= SPN_SANITIZER_UNDEFINED;
  }
  return set;
}

spn_err_union_t spn_cc_validate_profile(const spn_cc_toolchain_t* toolchain, const spn_profile_info_t* profile) {
  spn_triple_t target = { profile->arch, profile->os, profile->abi };
  spn_sanitizer_set_t unsupported = profile->sanitizers & ~get_supported_sanitizers(toolchain, target);
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

  // @spader Not totally sure about this
  spn_sanitizer_set_t ubsan = profile->sanitizers & ~SPN_SANITIZER_UNDEFINED;
  bool renders_static = profile->linkage == SPN_LIB_KIND_STATIC && profile->os != SPN_OS_MACOS && toolchain->driver != SPN_CC_DRIVER_MSVC;
  if (ubsan && renders_static) {
    return (spn_err_union_t) {
      .kind = SPN_ERR_SANITIZER_STATIC,
      .sanitizer = {
        .toolchain = toolchain->name,
        .target = target,
        .unsupported = ubsan,
      },
    };
  }
  return spn_result(SPN_OK);
}

spn_err_union_t spn_cc_render_flags(sp_mem_t mem, const spn_cc_toolchain_t* toolchain, const spn_profile_info_t* profile, spn_cc_flags_t* flags) {
  sp_da_init(mem, flags->compile);
  sp_da_init(mem, flags->link);
  spn_try_union(spn_cc_validate_profile(toolchain, profile));
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

spn_err_union_t spn_cc_render_compile(sp_mem_t mem, const spn_cc_toolchain_t* toolchain, const spn_profile_info_t* profile, const spn_cc_compile_t* compile, spn_invocation_t* invocation) {
  spn_try_union(spn_cc_validate_profile(toolchain, profile));
  *invocation = sp_zero_s(spn_invocation_t);
  switch (toolchain->driver) {
    case SPN_CC_DRIVER_GCC:
    case SPN_CC_DRIVER_CLANG: {
      spn_gnu_render_compile(mem, toolchain, profile, compile, invocation);
      return spn_result(SPN_OK);
    }
    case SPN_CC_DRIVER_MSVC: {
      spn_msvc_render_compile(mem, toolchain, profile, compile, invocation);
      return spn_result(SPN_OK);
    }
    case SPN_CC_DRIVER_NONE: {
      sp_unreachable_case();
    }
  }
  SP_UNREACHABLE_RETURN(spn_result(SPN_ERROR));
}

spn_invocation_t spn_cc_render_compile_command(sp_mem_t mem, const spn_cc_toolchain_t* toolchain, const spn_invocation_t* base, const spn_cc_compile_files_t* files) {
  sp_assert(!sp_str_empty(base->program));
  sp_assert(!sp_str_empty(files->source));
  sp_assert(!sp_str_empty(files->output));

  spn_invocation_t invocation = {
    .program = base->program,
    .cwd = base->cwd,
  };
  sp_da_init(mem, invocation.args);
  sp_da_reserve(invocation.args, sp_da_size(base->args));
  sp_da_for(base->args, it) {
    sp_da_push(invocation.args, base->args[it]);
  }

  switch (toolchain->driver) {
    case SPN_CC_DRIVER_GCC:
    case SPN_CC_DRIVER_CLANG: {
      spn_gnu_render_compile_files(mem, files, &invocation);
      break;
    }
    case SPN_CC_DRIVER_MSVC: {
      spn_msvc_render_compile_files(mem, files, &invocation);
      break;
    }
    case SPN_CC_DRIVER_NONE: {
      sp_unreachable_case();
    }
  }
  return invocation;
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

spn_err_union_t spn_cc_validate_link(const spn_cc_toolchain_t* toolchain, const spn_profile_info_t* profile, spn_cc_output_kind_t kind, bool frameworks) {
  spn_try_union(spn_cc_validate_profile(toolchain, profile));
  spn_cc_feature_t feature = link_feature(kind);

  spn_err_union_t err = {
    .kind = SPN_ERR_COMPILER_FEATURE_UNSUPPORTED,
    .compiler = {
      .toolchain = toolchain->name,
      .target = { profile->arch, profile->os, profile->abi },
      .feature = feature,
    },
  };
  if (kind == SPN_CC_OUTPUT_REACTOR && profile->os != SPN_OS_WASI) {
    return err;
  }
  if (kind == SPN_CC_OUTPUT_SHARED_LIB && profile->os == SPN_OS_WASI) {
    return err;
  }
  if (profile->os == SPN_OS_MACOS && frameworks && sp_str_empty(profile->sysroot)) {
    err.compiler.feature = SPN_CC_FEATURE_FRAMEWORKS;
    return err;
  }
  if (toolchain->driver == SPN_CC_DRIVER_MSVC) {
    if (kind == SPN_CC_OUTPUT_REACTOR) {
      return err;
    }
  }
  return spn_result(SPN_OK);
}

spn_err_union_t spn_cc_render_link(sp_mem_t mem, const spn_cc_toolchain_t* toolchain, const spn_profile_info_t* profile, const spn_cc_link_t* link, const spn_cc_link_files_t* files, spn_invocation_t* invocation) {
  sp_assert(!sp_str_empty(files->output));
  if (!sp_str_empty(files->exports.path)) sp_assert(sp_da_empty(files->exports.symbols));
  spn_try_union(spn_cc_validate_link(toolchain, profile, link->kind, !sp_da_empty(link->frameworks)));
  *invocation = sp_zero_s(spn_invocation_t);
  switch (toolchain->driver) {
    case SPN_CC_DRIVER_GCC:
    case SPN_CC_DRIVER_CLANG: {
      spn_gnu_render_link(mem, toolchain, profile, link, files, invocation);
      return spn_result(SPN_OK);
    }
    case SPN_CC_DRIVER_MSVC: {
      spn_msvc_render_link(mem, toolchain, profile, link, files, invocation);
      return spn_result(SPN_OK);
    }
    case SPN_CC_DRIVER_NONE: {
      sp_unreachable_case();
    }
  }
  SP_UNREACHABLE_RETURN(spn_result(SPN_ERROR));
}

spn_err_union_t spn_cc_validate_archive(const spn_cc_toolchain_t* toolchain, const spn_profile_info_t* profile) {
  SP_UNUSED(toolchain);
  SP_UNUSED(profile);
  return spn_result(SPN_OK);
}

spn_err_union_t spn_cc_render_archive(sp_mem_t mem, const spn_cc_toolchain_t* toolchain, const spn_profile_info_t* profile, const spn_cc_archive_files_t* files, spn_invocation_t* invocation) {
  sp_assert(!sp_str_empty(files->output));
  spn_try_union(spn_cc_validate_archive(toolchain, profile));
  *invocation = sp_zero_s(spn_invocation_t);

  switch (toolchain->archiver_driver) {
    case SPN_AR_DRIVER_GNU: {
      spn_gnu_render_archive(mem, toolchain, files, invocation);
      return spn_result(SPN_OK);
    }
    case SPN_AR_DRIVER_MSVC: {
      spn_msvc_render_archive(mem, toolchain, files, invocation);
      return spn_result(SPN_OK);
    }
  }
  SP_UNREACHABLE_RETURN(spn_result(SPN_ERROR));
}

spn_cc_exports_format_t spn_cc_exports_format(spn_cc_output_kind_t kind, spn_os_t os) {
  switch (kind) {
    case SPN_CC_OUTPUT_REACTOR: {
      return SPN_CC_EXPORTS_WASM;
    }
    case SPN_CC_OUTPUT_SHARED_LIB: {
      switch (os) {
        case SPN_OS_MACOS: return SPN_CC_EXPORTS_SYMBOL_LIST;
        case SPN_OS_WINDOWS: return SPN_CC_EXPORTS_DEF;
        case SPN_OS_LINUX:
        case SPN_OS_WASI:
        case SPN_OS_NONE: return SPN_CC_EXPORTS_VERSION_SCRIPT;
      }
      SP_UNREACHABLE_RETURN(SPN_CC_EXPORTS_VERSION_SCRIPT);
    }
    case SPN_CC_OUTPUT_EXE:
    case SPN_CC_OUTPUT_OBJECT:
    case SPN_CC_OUTPUT_STATIC_LIB: {
      sp_unreachable_case();
    }
  }
  SP_UNREACHABLE_RETURN(SPN_CC_EXPORTS_VERSION_SCRIPT);
}

const c8* spn_cc_exports_extension(spn_cc_exports_format_t format) {
  switch (format) {
    case SPN_CC_EXPORTS_VERSION_SCRIPT: return "map";
    case SPN_CC_EXPORTS_SYMBOL_LIST: return "exp";
    case SPN_CC_EXPORTS_DEF: return "def";
    case SPN_CC_EXPORTS_WASM: return "sym";
  }
  SP_UNREACHABLE_RETURN("map");
}
