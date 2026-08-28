#include "compiler/driver.h"
#include "compiler/push.h"
#include "compiler/types.h"
#include "ctx/types.h"

#include "error/error.h"
#include "paths/paths.h"

void spn_cc_push(sp_mem_t mem, spn_invocation_t* invocation, spn_arg_t arg) {
  if (!invocation->args) sp_da_init(mem, invocation->args);
  if (!spn_arg_empty(arg)) {
    sp_da_push(invocation->args, arg);
  }
}

void spn_cc_push_c(sp_mem_t mem, spn_invocation_t* invocation, const c8* value) {
  spn_cc_push(mem, invocation, spn_arg_lit(sp_cstr_as_str(value)));
}

void spn_cc_push_str(sp_mem_t mem, spn_invocation_t* invocation, sp_str_t value) {
  spn_cc_push(mem, invocation, spn_arg_lit(value));
}

void spn_cc_push_fmt(sp_mem_t mem, spn_invocation_t* invocation, const c8* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  sp_str_r str = sp_fmt_mem_v(mem, sp_cstr_as_str(fmt), args);
  va_end(args);

  spn_cc_push(mem, invocation, spn_arg_lit(str.value));
}

void spn_cc_push_path(sp_mem_t mem, spn_invocation_t* invocation, spn_path_t path) {
  spn_cc_push(mem, invocation, spn_arg_path(path));
}

void spn_cc_push_glued(sp_mem_t mem, spn_invocation_t* invocation, const c8* prefix, spn_path_t path) {
  spn_cc_push(mem, invocation, spn_arg_glue(sp_cstr_as_str(prefix), path));
}

void spn_cc_push_strs(sp_mem_t mem, spn_invocation_t* invocation, sp_da(sp_str_t) values) {
  sp_da_for(values, it) {
    spn_cc_push(mem, invocation, spn_arg_lit(values[it]));
  }
}

void spn_cc_push_paths(sp_mem_t mem, spn_invocation_t* invocation, sp_da(spn_path_t) paths) {
  sp_da_for(paths, it) {
    spn_cc_push(mem, invocation, spn_arg_path(paths[it]));
  }
}

void spn_cc_push_args(sp_mem_t mem, spn_invocation_t* invocation, sp_da(spn_arg_t) args) {
  sp_da_for(args, it) {
    spn_cc_push(mem, invocation, args[it]);
  }
}

spn_cc_cap_set_t spn_cc_driver_caps(spn_cc_driver_t driver) {
  switch (driver) {
    case SPN_CC_DRIVER_GCC: return SPN_CC_CAP_EXCLUDE_LIBS;
    case SPN_CC_DRIVER_CLANG: return SPN_CC_CAP_TARGET_TRIPLE | SPN_CC_CAP_CLANG_FRONTEND | SPN_CC_CAP_EXCLUDE_LIBS;
    case SPN_CC_DRIVER_ZIG: return SPN_CC_CAP_TARGET_TRIPLE | SPN_CC_CAP_CLANG_FRONTEND;
    case SPN_CC_DRIVER_MSVC: return 0;
    case SPN_CC_DRIVER_NONE: sp_unreachable_case();
  }
  SP_UNREACHABLE_RETURN(0);
}

bool spn_cc_has(const spn_cc_toolchain_t* toolchain, spn_cc_cap_t cap) {
  return (spn_cc_driver_caps(toolchain->driver) & cap) == (spn_cc_cap_set_t)cap;
}

spn_cc_depfile_t spn_cc_depfile(const spn_cc_toolchain_t* toolchain, spn_lang_t lang) {
  switch (toolchain->driver) {
    case SPN_CC_DRIVER_GCC:
    case SPN_CC_DRIVER_CLANG:
    case SPN_CC_DRIVER_ZIG: {
      return lang == SPN_LANG_ASM ? SPN_CC_DEPFILE_OPTIONAL : SPN_CC_DEPFILE_REQUIRED;
    }
    case SPN_CC_DRIVER_MSVC: {
      return lang == SPN_LANG_ASM ? SPN_CC_DEPFILE_NONE : SPN_CC_DEPFILE_REQUIRED;
    }
    case SPN_CC_DRIVER_NONE: {
      sp_unreachable_case();
    }
  }
  SP_UNREACHABLE_RETURN(SPN_CC_DEPFILE_NONE);
}

spn_err_t spn_cc_parse_depfile(sp_mem_t mem, const spn_cc_toolchain_t* toolchain, sp_str_t content, sp_da(sp_str_t)* prereqs) {
  sp_da_init(mem, *prereqs);
  switch (toolchain->driver) {
    case SPN_CC_DRIVER_GCC:
    case SPN_CC_DRIVER_CLANG:
    case SPN_CC_DRIVER_ZIG: {
      return spn_gnu_parse_depfile(mem, content, prereqs);
    }
    case SPN_CC_DRIVER_MSVC: {
      return spn_msvc_parse_depfile(mem, content, prereqs);
    }
    case SPN_CC_DRIVER_NONE: {
      sp_unreachable_case();
    }
  }
  SP_UNREACHABLE_RETURN(SPN_ERROR);
}

spn_sanitizer_set_t get_supported_sanitizers(const spn_cc_toolchain_t* toolchain, spn_triple_t target) {
  switch (toolchain->driver) {
    case SPN_CC_DRIVER_GCC: return spn_gcc_supported_sanitizers(target);
    case SPN_CC_DRIVER_CLANG: return spn_clang_supported_sanitizers(target);
    case SPN_CC_DRIVER_MSVC: return spn_msvc_supported_sanitizers(target);
    case SPN_CC_DRIVER_ZIG: return spn_zig_supported_sanitizers(target);
    case SPN_CC_DRIVER_NONE: sp_unreachable_case();
  }
  SP_UNREACHABLE_RETURN(0);
}

spn_err_t spn_cc_validate_profile(const spn_cc_toolchain_t* toolchain, const spn_profile_info_t* profile) {
  spn_triple_t target = { profile->arch, profile->os, profile->abi };
  spn_sanitizer_set_t supported = get_supported_sanitizers(toolchain, target);
  spn_sanitizer_set_t unsupported = profile->sanitizers & ~supported;
  if (unsupported) {
    return spn_err_emit(&spn, (spn_err_union_t) {
      .kind = SPN_ERR_SANITIZER_UNSUPPORTED,
      .sanitizer = {
        .toolchain = toolchain->name,
        .target = target,
        .unsupported = unsupported,
        .supported = supported,
      },
    });
  }

  // @spader Not totally sure about this
  spn_sanitizer_set_t ubsan = profile->sanitizers & ~SPN_SANITIZER_UNDEFINED;
  bool renders_static = profile->linkage == SPN_LIB_KIND_STATIC && profile->os != SPN_OS_MACOS && toolchain->driver != SPN_CC_DRIVER_MSVC;
  if (ubsan && renders_static) {
    return spn_err_emit(&spn, (spn_err_union_t) {
      .kind = SPN_ERR_SANITIZER_STATIC,
      .sanitizer = {
        .toolchain = toolchain->name,
        .target = target,
        .unsupported = ubsan,
        .supported = supported,
      },
    });
  }
  return SPN_OK;
}

spn_err_t spn_cc_render_flags(sp_mem_t mem, const spn_cc_toolchain_t* toolchain, const spn_profile_info_t* profile, spn_cc_flags_t* flags) {
  sp_da_init(mem, flags->compile);
  sp_da_init(mem, flags->link);
  spn_try(spn_cc_validate_profile(toolchain, profile));
  switch (toolchain->driver) {
    case SPN_CC_DRIVER_GCC:
    case SPN_CC_DRIVER_CLANG:
    case SPN_CC_DRIVER_ZIG: {
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
  return SPN_OK;
}

spn_err_t spn_cc_render_compile(sp_mem_t mem, const spn_cc_toolchain_t* toolchain, const spn_profile_info_t* profile, const spn_cc_compile_t* compile, spn_invocation_t* invocation) {
  spn_try(spn_cc_validate_profile(toolchain, profile));
  *invocation = sp_zero_s(spn_invocation_t);
  switch (toolchain->driver) {
    case SPN_CC_DRIVER_GCC:
    case SPN_CC_DRIVER_CLANG:
    case SPN_CC_DRIVER_ZIG: {
      spn_gnu_render_compile(mem, toolchain, profile, compile, invocation);
      return SPN_OK;
    }
    case SPN_CC_DRIVER_MSVC: {
      spn_msvc_render_compile(mem, toolchain, profile, compile, invocation);
      return SPN_OK;
    }
    case SPN_CC_DRIVER_NONE: {
      sp_unreachable_case();
    }
  }
  SP_UNREACHABLE_RETURN(SPN_ERROR);
}

spn_invocation_t spn_cc_render_compile_command(sp_mem_t mem, const spn_cc_toolchain_t* toolchain, const spn_profile_info_t* profile, const spn_invocation_t* base, const spn_cc_compile_files_t* files) {
  sp_assert(!spn_arg_empty(base->program));
  sp_assert(!spn_path_empty(files->source));
  sp_assert(!spn_path_empty(files->output));

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
    case SPN_CC_DRIVER_CLANG:
    case SPN_CC_DRIVER_ZIG: {
      spn_gnu_render_compile_files(mem, toolchain, profile, files, &invocation);
      break;
    }
    case SPN_CC_DRIVER_MSVC: {
      spn_msvc_render_compile_files(mem, toolchain, profile, files, &invocation);
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

static spn_err_t feature_unsupported(const spn_cc_toolchain_t* toolchain, const spn_profile_info_t* profile, spn_cc_feature_t feature) {
  return spn_err_emit(&spn, (spn_err_union_t) {
    .kind = SPN_ERR_COMPILER_FEATURE_UNSUPPORTED,
    .compiler = {
      .toolchain = toolchain->name,
      .target = { profile->arch, profile->os, profile->abi },
      .feature = feature,
    },
  });
}

spn_err_t spn_cc_validate_link(const spn_cc_toolchain_t* toolchain, const spn_profile_info_t* profile, spn_cc_output_kind_t kind, bool frameworks) {
  spn_try(spn_cc_validate_profile(toolchain, profile));
  spn_cc_feature_t feature = link_feature(kind);

  if (kind == SPN_CC_OUTPUT_REACTOR && profile->os != SPN_OS_WASI) {
    return feature_unsupported(toolchain, profile, feature);
  }
  if (kind == SPN_CC_OUTPUT_SHARED_LIB && profile->os == SPN_OS_WASI) {
    return feature_unsupported(toolchain, profile, feature);
  }
  if (profile->os == SPN_OS_MACOS && frameworks && spn_path_empty(profile->sysroot)) {
    return feature_unsupported(toolchain, profile, SPN_CC_FEATURE_FRAMEWORKS);
  }
  if (toolchain->driver == SPN_CC_DRIVER_MSVC) {
    if (kind == SPN_CC_OUTPUT_REACTOR) {
      return feature_unsupported(toolchain, profile, feature);
    }
  }
  return SPN_OK;
}

spn_err_t spn_cc_render_link(sp_mem_t mem, const spn_cc_toolchain_t* toolchain, const spn_profile_info_t* profile, const spn_cc_link_t* link, const spn_cc_link_files_t* files, spn_invocation_t* invocation) {
  sp_assert(!spn_path_empty(files->output));
  if (!spn_path_empty(files->exports.path)) sp_assert(sp_da_empty(files->exports.symbols));
  spn_try(spn_cc_validate_link(toolchain, profile, link->kind, !sp_da_empty(link->frameworks)));
  *invocation = sp_zero_s(spn_invocation_t);
  switch (toolchain->driver) {
    case SPN_CC_DRIVER_GCC:
    case SPN_CC_DRIVER_CLANG:
    case SPN_CC_DRIVER_ZIG: {
      spn_gnu_render_link(mem, toolchain, profile, link, files, invocation);
      return SPN_OK;
    }
    case SPN_CC_DRIVER_MSVC: {
      spn_msvc_render_link(mem, toolchain, profile, link, files, invocation);
      return SPN_OK;
    }
    case SPN_CC_DRIVER_NONE: {
      sp_unreachable_case();
    }
  }
  SP_UNREACHABLE_RETURN(SPN_ERROR);
}

spn_err_t spn_cc_validate_archive(const spn_cc_toolchain_t* toolchain, const spn_profile_info_t* profile) {
  return SPN_OK;
}

spn_err_t spn_cc_render_archive(sp_mem_t mem, const spn_cc_toolchain_t* toolchain, const spn_profile_info_t* profile, const spn_cc_archive_files_t* files, spn_invocation_t* invocation) {
  sp_assert(!spn_path_empty(files->output));
  spn_try(spn_cc_validate_archive(toolchain, profile));
  *invocation = sp_zero_s(spn_invocation_t);

  switch (toolchain->archiver_driver) {
    case SPN_AR_DRIVER_GNU: {
      spn_gnu_render_archive(mem, toolchain, files, invocation);
      return SPN_OK;
    }
    case SPN_AR_DRIVER_MSVC: {
      spn_msvc_render_archive(mem, toolchain, files, invocation);
      return SPN_OK;
    }
  }
  SP_UNREACHABLE_RETURN(SPN_ERROR);
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
