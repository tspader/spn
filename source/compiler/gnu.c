#include "compiler/driver.h"

#include "enum/enum.h"
#include "sp/macro.h"
#include "triple/triple.h"

static void push_flag(sp_da(sp_str_t)* flags, sp_str_t flag) {
  if (!sp_str_empty(flag)) {
    sp_da_push(*flags, flag);
  }
}
static void add_arg(sp_mem_t mem, spn_invocation_t* invocation, sp_str_t arg) {
  if (!invocation->args) sp_da_init(mem, invocation->args);
  if (!sp_str_empty(arg)) {
    sp_da_push(invocation->args, arg);
  }
}

static void push_arg_fmt(sp_mem_t mem, spn_invocation_t* invocation, const c8* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  sp_str_r str = sp_fmt_mem_v(mem, sp_cstr_as_str(fmt), args);
  va_end(args);

  add_arg(mem, invocation, str.value);
}

static void push_arg(sp_mem_t mem, spn_invocation_t* invocation, const c8* arg) {
  add_arg(mem, invocation, sp_cstr_as_str(arg));
}

static void push_arg_str(sp_mem_t mem, spn_invocation_t* invocation, sp_str_t arg) {
  add_arg(mem, invocation, arg);
}

static void push_args(sp_mem_t mem, spn_invocation_t* invocation, sp_da(sp_str_t) args) {
  sp_da_for(args, it) {
    add_arg(mem, invocation, args[it]);
  }
}


static sp_str_t render_define(sp_mem_t mem, sp_str_t value) {
  return sp_fmt(mem, "-D{}", sp_fmt_str(value)).value;
}

static sp_str_t render_define_c(sp_mem_t mem, const c8* value) {
  return render_define(mem, sp_cstr_as_str(value));
}

static sp_str_t render_include(sp_mem_t mem, sp_str_t value) {
  return sp_fmt(mem, "-I{}", sp_fmt_str(value)).value;
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

static sp_str_t c_standard_switch(spn_c_standard_t standard) {
  switch (standard) {
    case SPN_C89: return sp_str_lit("-std=c89");
    case SPN_C99: return sp_str_lit("-std=c99");
    case SPN_C11: return sp_str_lit("-std=c11");
    case SPN_C_STANDARD_NONE: return sp_str_lit("-std=c99");
  }
  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

static sp_str_t cxx_standard_switch(spn_cxx_standard_t standard) {
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

// @spader Same hack as the bootstrap CMakeLists: zig cc turns on UBSan traps
// by default in debug builds, and we have UB to clean up before it can stay on
static void zig_default_sanitizer_off(const spn_cc_toolchain_t* toolchain, const spn_profile_info_t* profile, spn_cc_flags_t* flags) {
  if (!sp_str_equal_cstr(toolchain->name, "zig")) return;
  if (profile->sanitizers) return;
  sp_da_push(flags->compile, sp_str_lit("-fno-sanitize=undefined"));
  sp_da_push(flags->link, sp_str_lit("-fno-sanitize=undefined"));
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
  sp_assert(!sp_str_empty(launcher.program));
  invocation->program = launcher.program;
  push_args(mem, invocation, launcher.args);
  if (toolchain->driver == SPN_CC_DRIVER_CLANG) {
    spn_triple_t triple = { profile->arch, profile->os, profile->abi };
    sp_str_t target = spn_triple_to_cc_target(mem, triple);
    if (!sp_str_empty(target)) {
      push_arg_fmt(mem, invocation, "--target={}", sp_fmt_str(target));
    }
  }
}

static void add_include(sp_mem_t mem, spn_invocation_t* invocation, sp_str_t value) {
  push_arg_str(mem, invocation, render_include(mem, value));
}

static void add_define(sp_mem_t mem, spn_invocation_t* invocation, sp_str_t value) {
  push_arg_str(mem, invocation, render_define(mem, value));
}

void spn_gnu_render_compile(sp_mem_t mem, const spn_cc_toolchain_t* toolchain, const spn_profile_info_t* profile, const spn_cc_compile_t* compile, spn_invocation_t* invocation) {
  add_launcher(mem, toolchain, profile, compile->lang, invocation);
  spn_cc_flags_t flags = sp_zero;
  sp_da_init(mem, flags.compile);
  sp_da_init(mem, flags.link);
  spn_gnu_render_flags(mem, profile, &flags);
  zig_default_sanitizer_off(toolchain, profile, &flags);
  if (compile->lang == SPN_LANG_C) {
    push_arg_str(mem, invocation, c_standard_switch(profile->standard));
  } else if (compile->lang == SPN_LANG_CXX) {
    push_arg_str(mem, invocation, cxx_standard_switch(compile->cxx.standard));
  }
  push_args(mem, invocation, flags.compile);
  push_arg(mem, invocation, "-c");
  push_arg_str(mem, invocation, compile->source);
  sp_da_for(compile->include, it) {
    add_include(mem, invocation, compile->include[it]);
  }
  sp_da_for(compile->define, it) {
    add_define(mem, invocation, compile->define[it]);
  }
  if (compile->lang == SPN_LANG_CXX) {
    if (compile->cxx.no_exceptions) {
      push_arg(mem, invocation, "-fno-exceptions");
    }
    if (compile->cxx.no_rtti) {
      push_arg(mem, invocation, "-fno-rtti");
    }
  }
  if (compile->pic) {
    push_arg(mem, invocation, "-fPIC");
  }
  if (profile->os == SPN_OS_MACOS) {
    if (!sp_str_empty(profile->sysroot)) {
      push_arg(mem, invocation, "-isysroot");
      push_arg_str(mem, invocation, profile->sysroot);
    }
    if (spn_os_version_present(compile->min_os)) {
      push_arg_fmt(mem, invocation, "-mmacosx-version-min={}.{}", sp_fmt_uint(compile->min_os.major), sp_fmt_uint(compile->min_os.minor));
    }
  }
  push_args(mem, invocation, compile->args);
  push_arg(mem, invocation, "-Werror=return-type");
  if (!sp_str_empty(compile->depfile)) {
    push_arg(mem, invocation, "-MD");
    push_arg(mem, invocation, "-MF");
    push_arg_str(mem, invocation, compile->depfile);
  }
  push_arg(mem, invocation, "-o");
  push_arg_str(mem, invocation, compile->output);
}

void spn_gnu_render_link(sp_mem_t mem, const spn_cc_toolchain_t* toolchain, const spn_profile_info_t* profile, const spn_cc_link_t* link, spn_invocation_t* invocation) {
  add_launcher(mem, toolchain, profile, link->lang, invocation);
  spn_cc_flags_t flags = sp_zero;
  sp_da_init(mem, flags.compile);
  sp_da_init(mem, flags.link);
  spn_gnu_render_flags(mem, profile, &flags);
  zig_default_sanitizer_off(toolchain, profile, &flags);
  push_args(mem, invocation, flags.link);
  switch (link->kind) {
    case SPN_CC_OUTPUT_REACTOR: {
      push_arg(mem, invocation, "-mexec-model=reactor");
      push_arg(mem, invocation, "-Wl,--no-entry");
      push_arg(mem, invocation, "-Wl,--import-symbols");
      sp_da_for(link->exports.symbols, it) {
        push_arg_fmt(mem, invocation, "-Wl,--export={}", sp_fmt_str(link->exports.symbols[it]));
      }
      break;
    }
    case SPN_CC_OUTPUT_SHARED_LIB: {
      push_arg(mem, invocation, "-shared");
      if (!sp_str_empty(link->exports.path)) {
        switch (profile->os) {
          case SPN_OS_MACOS: {
            push_arg_fmt(mem, invocation, "-Wl,-exported_symbols_list,{}", sp_fmt_str(link->exports.path));
            break;
          }
          case SPN_OS_WINDOWS: {
            push_arg_str(mem, invocation, link->exports.path);
            break;
          }
          case SPN_OS_LINUX:
          case SPN_OS_WASI:
          case SPN_OS_NONE: {
            push_arg_fmt(mem, invocation, "-Wl,--version-script,{}", sp_fmt_str(link->exports.path));
            break;
          }
        }
      }
      break;
    }
    case SPN_CC_OUTPUT_EXE: {
      if (profile->linkage == SPN_LIB_KIND_STATIC && profile->os != SPN_OS_MACOS) {
        push_arg(mem, invocation, "-static");
      }
      if (profile->os == SPN_OS_WINDOWS && link->subsystem == SPN_WIN_SUBSYSTEM_WINDOWS) {
        push_arg(mem, invocation, "-Wl,--subsystem,windows");
      }
      break;
    }
    case SPN_CC_OUTPUT_OBJECT:
    case SPN_CC_OUTPUT_STATIC_LIB: {
      sp_unreachable_case();
    }
  }
  push_args(mem, invocation, link->objects);
  if (!sp_da_empty(link->whole_archives)) {
    if (profile->os == SPN_OS_MACOS) {
      sp_da_for(link->whole_archives, it) {
        push_arg_fmt(mem, invocation, "-Wl,-force_load,{}", sp_fmt_str(link->whole_archives[it]));
      }
    } else {
      push_arg(mem, invocation, "-Wl,--whole-archive");
      push_args(mem, invocation, link->whole_archives);
      push_arg(mem, invocation, "-Wl,--no-whole-archive");
    }
  }
  push_args(mem, invocation, link->args);
  sp_da_for(link->lib_dirs, it) {
    push_arg_fmt(mem, invocation, "-L{}", sp_fmt_str(link->lib_dirs[it]));
  }
  push_args(mem, invocation, link->libs);
  sp_da_for(link->private_libs, it) {
    push_arg_fmt(mem, invocation, "-l{}", sp_fmt_str(link->private_libs[it]));
    if (profile->os == SPN_OS_WINDOWS) {
      spn_triple_t triple = { profile->arch, profile->os, profile->abi };
      sp_str_t archive = spn_triple_lib_file_name(mem, triple, link->private_libs[it], SP_OS_LIB_STATIC);
      push_arg_fmt(mem, invocation, "-Wl,--exclude-libs,{}", sp_fmt_str(archive));
    }
  }
  sp_da_for(link->system_libs, it) {
    push_arg_fmt(mem, invocation, "-l{}", sp_fmt_str(link->system_libs[it]));
  }
  if (profile->os == SPN_OS_MACOS) {
    if (!sp_str_empty(profile->sysroot)) {
      push_arg(mem, invocation, "-isysroot");
      push_arg_str(mem, invocation, profile->sysroot);
    }
    if (spn_os_version_present(link->min_os)) {
      push_arg_fmt(mem, invocation, "-mmacosx-version-min={}.{}", sp_fmt_uint(link->min_os.major), sp_fmt_uint(link->min_os.minor));
    }
    sp_da_for(link->frameworks, it) {
      push_arg(mem, invocation, "-framework");
      push_arg_str(mem, invocation, link->frameworks[it]);
    }
  }
  sp_da_for(link->rpath, it) {
    push_arg_fmt(mem, invocation, "-Wl,-rpath,{}", sp_fmt_str(link->rpath[it]));
  }
  push_arg(mem, invocation, "-o");
  push_arg_str(mem, invocation, link->output);
}

void spn_gnu_render_archive(sp_mem_t mem, const spn_cc_toolchain_t* toolchain, const spn_cc_archive_t* archive, spn_invocation_t* invocation) {
  invocation->program = toolchain->archiver.program;
  push_args(mem, invocation, toolchain->archiver.args);
  push_arg(mem, invocation, "rcs");
  push_args(mem, invocation, archive->args);
  push_arg_str(mem, invocation, archive->output);
  push_args(mem, invocation, archive->objects);
}
