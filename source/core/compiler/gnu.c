#include "compiler/driver.h"
#include "compiler/push.h"

#include "enum/enum.h"
#include "sp/macro.h"
#include "paths/paths.h"
#include "triple/triple.h"

static void push_flag(sp_da(sp_str_t)* flags, sp_str_t flag) {
  if (!sp_str_empty(flag)) {
    sp_da_push(*flags, flag);
  }
}

static sp_str_t render_define(sp_mem_t mem, sp_str_t value) {
  return sp_fmt(mem, "-D{}", sp_fmt_str(value)).value;
}

static sp_str_t render_define_c(sp_mem_t mem, const c8* value) {
  return render_define(mem, sp_cstr_as_str(value));
}

static sp_str_t opt_switch(spn_opt_level_t level) {
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

static sp_str_t c_standard_to_flag(spn_c_standard_t standard) {
  switch (standard) {
    case SPN_C89: return sp_str_lit("-std=c89");
    case SPN_C99: return sp_str_lit("-std=c99");
    case SPN_C11: return sp_str_lit("-std=c11");
    case SPN_C_STANDARD_NONE: return sp_str_lit("-std=c99");
  }
  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

static sp_str_t cxx_standard_to_flag(spn_cxx_standard_t standard) {
  switch (standard) {
    case SPN_CXX11: return sp_str_lit("-std=c++11");
    case SPN_CXX14: return sp_str_lit("-std=c++14");
    case SPN_CXX17: return sp_str_lit("-std=c++17");
    case SPN_CXX20: return sp_str_lit("-std=c++20");
    case SPN_CXX23: return sp_str_lit("-std=c++23");
    case SPN_CXX_STANDARD_NONE: return sp_str_lit("-std=c++17");
  }
  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

static bool is_os_version_present(spn_os_version_t version) {
  return version.major || version.minor;
}

spn_sanitizer_set_t spn_gcc_supported_sanitizers(spn_triple_t target) {
  switch (target.os) {
    case SPN_OS_WASI:
    case SPN_OS_WINDOWS: return 0;
    case SPN_OS_MACOS: return SPN_SANITIZER_ADDRESS | SPN_SANITIZER_THREAD | SPN_SANITIZER_UNDEFINED;
    case SPN_OS_LINUX:
    case SPN_OS_NONE: return SPN_SANITIZER_ADDRESS | SPN_SANITIZER_THREAD | SPN_SANITIZER_UNDEFINED | SPN_SANITIZER_LEAK;
  }
  SP_UNREACHABLE_RETURN(0);
}

spn_sanitizer_set_t spn_clang_supported_sanitizers(spn_triple_t target) {
  switch (target.os) {
    case SPN_OS_WASI: return 0;
    case SPN_OS_WINDOWS: return target.abi == SPN_ABI_MSVC && target.arch == SPN_ARCH_X64 ? SPN_SANITIZER_ADDRESS : 0;
    case SPN_OS_MACOS: return SPN_SANITIZER_ADDRESS | SPN_SANITIZER_THREAD | SPN_SANITIZER_UNDEFINED | SPN_SANITIZER_LEAK;
    case SPN_OS_LINUX:
    case SPN_OS_NONE: return SPN_SANITIZER_ADDRESS | SPN_SANITIZER_THREAD | SPN_SANITIZER_UNDEFINED | SPN_SANITIZER_MEMORY | SPN_SANITIZER_LEAK;
  }
  SP_UNREACHABLE_RETURN(0);
}

spn_sanitizer_set_t spn_zig_supported_sanitizers(spn_triple_t target) {
  switch (target.os) {
    case SPN_OS_WASI: return 0;
    case SPN_OS_WINDOWS: return SPN_SANITIZER_UNDEFINED;
    case SPN_OS_MACOS:
    case SPN_OS_LINUX:
    case SPN_OS_NONE: return SPN_SANITIZER_UNDEFINED | SPN_SANITIZER_THREAD;
  }
  SP_UNREACHABLE_RETURN(0);
}

void spn_gnu_render_flags(sp_mem_t mem, const spn_profile_info_t* profile, spn_cc_flags_t* flags) {
  if (profile->mode == SPN_BUILD_MODE_DEBUG) {
    sp_da_push(flags->compile, sp_str_lit("-g"));
  }
  push_flag(&flags->compile, opt_switch(profile->opt));
  if (profile->mode == SPN_BUILD_MODE_RELEASE) {
    sp_da_push(flags->compile, render_define_c(mem, "NDEBUG"));
  }
  if (profile->sanitizers) {
    sp_str_t sanitizer = sp_fmt(mem, "-fsanitize={}", sp_fmt_str(spn_sanitizer_set_to_str(mem, profile->sanitizers))).value;
    sp_da_push(flags->compile, sanitizer);
    sp_da_push(flags->link, sanitizer);
    sp_da_push(flags->compile, sp_str_lit("-fno-sanitize-recover=all"));
    sp_da_push(flags->compile, sp_str_lit("-fno-omit-frame-pointer"));
  }
}

static void add_launcher(sp_mem_t mem, const spn_cc_toolchain_t* toolchain, const spn_profile_info_t* profile, spn_lang_t lang, spn_invocation_t* invocation) {
  spn_toolchain_launcher_t launcher = lang == SPN_LANG_CXX ? toolchain->cxx : toolchain->compiler;
  sp_assert(!spn_arg_empty(launcher.program));
  invocation->program = launcher.program;
  spn_cc_push_strs(mem, invocation, launcher.args);
  if (spn_cc_has(toolchain, SPN_CC_CAP_TARGET_TRIPLE)) {
    spn_triple_t triple = { profile->arch, profile->os, profile->abi };
    sp_str_t target = spn_triple_to_cc_target(mem, triple);
    if (!sp_str_empty(target)) {
      spn_cc_push_fmt(mem, invocation, "--target={}", sp_fmt_str(target));
    }
  }
}

void spn_gnu_render_compile(sp_mem_t mem, const spn_cc_toolchain_t* toolchain, const spn_profile_info_t* profile, const spn_cc_compile_t* compile, spn_invocation_t* invocation) {
  add_launcher(mem, toolchain, profile, compile->lang, invocation);
  spn_cc_flags_t flags = sp_zero;
  sp_da_init(mem, flags.compile);
  sp_da_init(mem, flags.link);
  spn_gnu_render_flags(mem, profile, &flags);
  if (compile->lang == SPN_LANG_C) {
    spn_cc_push_str(mem, invocation, c_standard_to_flag(profile->standard));
  } else if (compile->lang == SPN_LANG_CXX) {
    spn_cc_push_str(mem, invocation, cxx_standard_to_flag(compile->cxx.standard));
  }
  spn_cc_push_strs(mem, invocation, flags.compile);
  spn_cc_push_c(mem, invocation, "-c");
  sp_da_for(compile->include, it) {
    spn_cc_push_glued(mem, invocation, "-I", compile->include[it]);
  }
  sp_da_for(compile->define, it) {
    spn_cc_push_str(mem, invocation, render_define(mem, compile->define[it]));
  }
  if (compile->lang == SPN_LANG_CXX) {
    if (compile->cxx.no_exceptions) {
      spn_cc_push_c(mem, invocation, "-fno-exceptions");
    }
    if (compile->cxx.no_rtti) {
      spn_cc_push_c(mem, invocation, "-fno-rtti");
    }
  }
  if (compile->pic) {
    spn_cc_push_c(mem, invocation, "-fPIC");
  }
  if (profile->os == SPN_OS_MACOS) {
    if (!spn_path_empty(profile->sysroot)) {
      spn_cc_push_c(mem, invocation, "-isysroot");
      spn_cc_push_path(mem, invocation, profile->sysroot);
      spn_cc_push_c(mem, invocation, "-iframework");
      spn_cc_push_path(mem, invocation, spn_path_join(mem, profile->sysroot, sp_str_lit("System/Library/Frameworks")));
    }
    if (is_os_version_present(compile->min_os)) {
      spn_cc_push_fmt(mem, invocation, "-mmacosx-version-min={}.{}", sp_fmt_uint(compile->min_os.major), sp_fmt_uint(compile->min_os.minor));
    }
  }
  if (profile->os == SPN_OS_WINDOWS && spn_cc_has(toolchain, SPN_CC_CAP_CLANG_FRONTEND)) {
    spn_cc_push_c(mem, invocation, "-gno-codeview-command-line");
  }
  spn_cc_push_strs(mem, invocation, compile->args);
  spn_cc_push_c(mem, invocation, "-Werror=return-type");
}

void spn_gnu_render_compile_files(sp_mem_t mem, const spn_cc_toolchain_t* toolchain, const spn_profile_info_t* profile, const spn_cc_compile_files_t* files, spn_invocation_t* invocation) {
  spn_cc_push_path(mem, invocation, files->source);
  if (!spn_path_empty(files->depfile)) {
    spn_cc_push_c(mem, invocation, "-MD");
    spn_cc_push_c(mem, invocation, "-MF");
    spn_cc_push_path(mem, invocation, files->depfile);
  }
  if (profile->os == SPN_OS_WINDOWS && spn_cc_has(toolchain, SPN_CC_CAP_CLANG_FRONTEND)) {
    spn_cc_push_c(mem, invocation, "-Xclang");
    spn_cc_push_fmt(mem, invocation, "-object-file-name={}", sp_fmt_str(sp_fs_get_name(files->output.sub)));
  }
  spn_cc_push_c(mem, invocation, "-o");
  spn_cc_push_path(mem, invocation, files->output);
}

void spn_gnu_render_link(sp_mem_t mem, const spn_cc_toolchain_t* toolchain, const spn_profile_info_t* profile, const spn_cc_link_t* link, const spn_cc_link_files_t* files, spn_invocation_t* invocation) {
  add_launcher(mem, toolchain, profile, link->lang, invocation);
  spn_cc_flags_t flags = sp_zero;
  sp_da_init(mem, flags.compile);
  sp_da_init(mem, flags.link);
  spn_gnu_render_flags(mem, profile, &flags);
  spn_cc_push_strs(mem, invocation, flags.link);
  switch (link->kind) {
    case SPN_CC_OUTPUT_REACTOR: {
      spn_cc_push_c(mem, invocation, "-mexec-model=reactor");
      spn_cc_push_c(mem, invocation, "-Wl,--no-entry");
      spn_cc_push_c(mem, invocation, "-Wl,--import-symbols");
      sp_da_for(files->exports.symbols, it) {
        spn_cc_push_fmt(mem, invocation, "-Wl,--export={}", sp_fmt_str(files->exports.symbols[it]));
      }
      break;
    }
    case SPN_CC_OUTPUT_SHARED_LIB: {
      spn_cc_push_c(mem, invocation, "-shared");
      if (profile->os == SPN_OS_MACOS) {
        spn_cc_push_fmt(mem, invocation, "-Wl,-install_name,@rpath/{}", sp_fmt_str(sp_fs_get_name(files->output.sub)));
      }
      if (!spn_path_empty(files->exports.path)) {
        switch (spn_cc_exports_format(link->kind, profile->os)) {
          case SPN_CC_EXPORTS_SYMBOL_LIST: {
            spn_cc_push_glued(mem, invocation, "-Wl,-exported_symbols_list,", files->exports.path);
            break;
          }
          case SPN_CC_EXPORTS_DEF: {
            spn_cc_push_path(mem, invocation, files->exports.path);
            break;
          }
          case SPN_CC_EXPORTS_VERSION_SCRIPT: {
            spn_cc_push_glued(mem, invocation, "-Wl,--version-script,", files->exports.path);
            break;
          }
          case SPN_CC_EXPORTS_WASM: {
            sp_unreachable_case();
          }
        }
      }
      break;
    }
    case SPN_CC_OUTPUT_EXE: {
      if (profile->linkage == SPN_LIB_KIND_STATIC && profile->os != SPN_OS_MACOS) {
        spn_cc_push_c(mem, invocation, "-static");
      }
      if (profile->os == SPN_OS_WINDOWS && link->subsystem == SPN_WIN_SUBSYSTEM_WINDOWS) {
        spn_cc_push_c(mem, invocation, "-Wl,--subsystem,windows");
      }
      break;
    }
    case SPN_CC_OUTPUT_OBJECT:
    case SPN_CC_OUTPUT_STATIC_LIB: {
      sp_unreachable_case();
    }
  }
  spn_cc_push_paths(mem, invocation, files->objects);
  if (!sp_da_empty(files->whole_archives)) {
    if (profile->os == SPN_OS_MACOS) {
      sp_da_for(files->whole_archives, it) {
        spn_cc_push_glued(mem, invocation, "-Wl,-force_load,", files->whole_archives[it]);
      }
    } else {
      spn_cc_push_c(mem, invocation, "-Wl,--whole-archive");
      spn_cc_push_paths(mem, invocation, files->whole_archives);
      spn_cc_push_c(mem, invocation, "-Wl,--no-whole-archive");
    }
  }
  sp_da_for(link->lib_dirs, it) {
    spn_cc_push_glued(mem, invocation, "-L", link->lib_dirs[it]);
  }
  sp_da_for(link->private_libs, it) {
    spn_cc_push_fmt(mem, invocation, "-l{}", sp_fmt_str(link->private_libs[it]));
    if (profile->os == SPN_OS_WINDOWS && spn_cc_has(toolchain, SPN_CC_CAP_EXCLUDE_LIBS)) {
      spn_triple_t triple = { profile->arch, profile->os, profile->abi };
      sp_str_t archive = spn_triple_lib_file_name(mem, triple, link->private_libs[it], SP_OS_LIB_STATIC);
      spn_cc_push_fmt(mem, invocation, "-Wl,--exclude-libs,{}", sp_fmt_str(archive));
    }
  }
  sp_da_for(link->libs, it) {
    spn_cc_push_fmt(mem, invocation, "-l{}", sp_fmt_str(link->libs[it]));
  }
  sp_da_for(link->system_libs, it) {
    spn_cc_push_fmt(mem, invocation, "-l{}", sp_fmt_str(link->system_libs[it]));
  }
  if (profile->os == SPN_OS_MACOS) {
    if (!spn_path_empty(profile->sysroot)) {
      spn_cc_push_c(mem, invocation, "-isysroot");
      spn_cc_push_path(mem, invocation, profile->sysroot);
      spn_cc_push_glued(mem, invocation, "-F", spn_path_join(mem, profile->sysroot, sp_str_lit("System/Library/Frameworks")));
      spn_cc_push_glued(mem, invocation, "-L", spn_path_join(mem, profile->sysroot, sp_str_lit("usr/lib")));
    }
    if (is_os_version_present(link->min_os)) {
      spn_cc_push_fmt(mem, invocation, "-mmacosx-version-min={}.{}", sp_fmt_uint(link->min_os.major), sp_fmt_uint(link->min_os.minor));
    }
    sp_da_for(link->frameworks, it) {
      spn_cc_push_c(mem, invocation, "-framework");
      spn_cc_push_str(mem, invocation, link->frameworks[it]);
    }
  }
  if (link->rpath) {
    switch (profile->os) {
      case SPN_OS_LINUX: {
        spn_cc_push_c(mem, invocation, "-Wl,-rpath,$ORIGIN");
        break;
      }
      case SPN_OS_MACOS: {
        spn_cc_push_c(mem, invocation, "-Wl,-rpath,@loader_path");
        break;
      }
      case SPN_OS_WINDOWS:
      case SPN_OS_WASI:
      case SPN_OS_NONE: {
        break;
      }
    }
  }
  spn_cc_push_c(mem, invocation, "-o");
  spn_cc_push_path(mem, invocation, files->output);
}

void spn_gnu_render_archive(sp_mem_t mem, const spn_cc_toolchain_t* toolchain, const spn_cc_archive_files_t* files, spn_invocation_t* invocation) {
  invocation->program = toolchain->archiver.program;
  spn_cc_push_strs(mem, invocation, toolchain->archiver.args);
  spn_cc_push_c(mem, invocation, "rcs");
  spn_cc_push_path(mem, invocation, files->output);
  spn_cc_push_paths(mem, invocation, files->objects);
}
