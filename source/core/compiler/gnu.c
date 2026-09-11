#include "compiler/driver.h"
#include "compiler/occ.h"
#include "compiler/push.h"

#include "enum/enum.h"
#include "spn/core.h"
#include "toolchain/linker.h"
#include "toolchain/toolchain.h"
#include "macro/macro.h"
#include "paths/paths.h"
#include "profile/types.h"
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
    case SPN_GNU89: return sp_str_lit("-std=gnu89");
    case SPN_GNU99: return sp_str_lit("-std=gnu99");
    case SPN_GNU11: return sp_str_lit("-std=gnu11");
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

// clang writes CodeView for the msvc abi and DWARF for mingw; zig writes
// CodeView for every Windows target. Only CodeView records the command line
// and object name, and only clang 15 and later knows the flag that drops them
static bool codeview(const spn_cc_toolchain_t* toolchain, const spn_profile_info_t* profile) {
  if (profile->os != SPN_OS_WINDOWS || !spn_cc_has(toolchain, SPN_CC_CAP_CLANG_FRONTEND)) {
    return false;
  }
  return profile->abi == SPN_ABI_MSVC || spn_cc_has(toolchain, SPN_CC_CAP_CODEVIEW);
}

static sp_str_t render_wasi(spn_wasi_spelling_t spelling) {
  switch (spelling) {
    case SPN_WASI_SPELLING_WASI: return sp_str_lit("wasm32-wasi");
    case SPN_WASI_SPELLING_WASIP1: return sp_str_lit("wasm32-wasip1");
  }
  sp_unreachable_return(sp_str_lit(""));
}

static sp_str_t render_target(sp_mem_t mem, const spn_cc_toolchain_t* toolchain, spn_triple_t triple) {
  switch (triple.os) {
    case SPN_OS_MACOS: {
      triple.abi = SPN_ABI_NONE;
      return spn_triple_to_str(mem, triple);
    }
    case SPN_OS_WASI: {
      return render_wasi(toolchain->wasi);
    }
    case SPN_OS_FREESTANDING: {
      if (spn_cc_has(toolchain, SPN_CC_CAP_LLVM_TRIPLE)) {
        return sp_fmt(mem, "{}-none-elf", sp_fmt_str(spn_arch_to_str(triple.arch))).value;
      }
      return spn_triple_to_str(mem, triple);
    }
    case SPN_OS_LINUX:
    case SPN_OS_WINDOWS:
    case SPN_OS_NONE: return spn_triple_to_str(mem, triple);
  }
  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

void spn_gnu_render_flags(sp_mem_t mem, const spn_cc_toolchain_t* toolchain, const spn_profile_info_t* profile, spn_cc_flags_t* flags) {
  if (profile->mode == SPN_MODE_DEBUG) {
    sp_da_push(flags->compile, sp_str_lit("-g"));
  }
  push_flag(&flags->compile, opt_switch(profile->opt));
  if (profile->mode == SPN_MODE_RELEASE) {
    sp_da_push(flags->compile, render_define_c(mem, "NDEBUG"));
  }
  if (profile->sanitizers) {
    sp_str_t sanitizer = sp_fmt(mem, "-fsanitize={}", sp_fmt_str(spn_sanitizer_set_to_str(mem, profile->sanitizers))).value;
    sp_da_push(flags->compile, sanitizer);
    sp_da_push(flags->link, sanitizer);
    sp_da_push(flags->compile, sp_str_lit("-fno-sanitize-recover=all"));
    sp_da_push(flags->compile, sp_str_lit("-fno-omit-frame-pointer"));
  }
  if (profile->abi == SPN_ABI_BARE) {
    sp_da_push(flags->compile, sp_str_lit("-ffreestanding"));
    sp_da_push(flags->compile, sp_str_lit("-fno-stack-protector"));
    if (spn_cc_has(toolchain, SPN_CC_CAP_DEFAULT_UBSAN)) {
      sp_da_push(flags->compile, sp_str_lit("-fno-sanitize=undefined"));
    }
    sp_da_push(flags->link, sp_str_lit("-nostartfiles"));
    sp_da_push(flags->link, sp_str_lit("-nolibc"));
  }
}

static void add_libc(sp_mem_t mem, const spn_profile_info_t* profile, spn_invocation_t* invocation) {
  sp_assert(!spn_path_empty(profile->libc));
  spn_cc_push_env(mem, invocation, SPN_ENV_ZIG_LIBC, spn_arg_path(profile->libc));
}

static void add_sdk_compile(sp_mem_t mem, const spn_cc_toolchain_t* toolchain, const spn_profile_info_t* profile, spn_invocation_t* invocation) {
  const spn_sdk_t* sdk = &profile->sdk;
  switch (sdk->kind) {
    case SPN_SDK_NONE: {
      break;
    }
    case SPN_SDK_SYSROOT: {
      spn_cc_push_glued(mem, invocation, "--sysroot=", sdk->root);
      break;
    }
    case SPN_SDK_MACOS: {
      if (spn_cc_has(toolchain, SPN_CC_CAP_LIBC_FILE)) {
        add_libc(mem, profile, invocation);
      } else {
        spn_cc_push_c(mem, invocation, "-isysroot");
        spn_cc_push_path(mem, invocation, sdk->macos.root);
      }
      break;
    }
    case SPN_SDK_MSVC: {
      if (spn_cc_has(toolchain, SPN_CC_CAP_LIBC_FILE)) {
        add_libc(mem, profile, invocation);
      } else {
        spn_cc_push_c(mem, invocation, "-nostdlibinc");
        spn_cc_push_c(mem, invocation, "-isystem");
        spn_cc_push_path(mem, invocation, sdk->msvc.include.vc);
        spn_cc_push_c(mem, invocation, "-isystem");
        spn_cc_push_path(mem, invocation, sdk->msvc.include.ucrt);
        spn_cc_push_c(mem, invocation, "-isystem");
        spn_cc_push_path(mem, invocation, sdk->msvc.include.um);
        spn_cc_push_c(mem, invocation, "-isystem");
        spn_cc_push_path(mem, invocation, sdk->msvc.include.shared);
      }
      break;
    }
  }
}

static void add_sdk_link(sp_mem_t mem, const spn_cc_toolchain_t* toolchain, const spn_profile_info_t* profile, spn_invocation_t* invocation) {
  const spn_sdk_t* sdk = &profile->sdk;
  switch (sdk->kind) {
    case SPN_SDK_NONE: {
      break;
    }
    case SPN_SDK_SYSROOT: {
      spn_cc_push_glued(mem, invocation, "--sysroot=", sdk->root);
      break;
    }
    case SPN_SDK_MACOS: {
      if (spn_cc_has(toolchain, SPN_CC_CAP_LIBC_FILE)) {
        add_libc(mem, profile, invocation);
        spn_cc_push_c(mem, invocation, "-F");
        spn_cc_push_path(mem, invocation, sdk->macos.frameworks);
      } else {
        spn_cc_push_c(mem, invocation, "-isysroot");
        spn_cc_push_path(mem, invocation, sdk->macos.root);
      }
      break;
    }
    case SPN_SDK_MSVC: {
      if (spn_cc_has(toolchain, SPN_CC_CAP_LIBC_FILE)) {
        add_libc(mem, profile, invocation);
      } else {
        spn_path_t libs [] = { sdk->msvc.lib.vc, sdk->msvc.lib.ucrt, sdk->msvc.lib.um };
        spn_cc_push_env_paths(mem, invocation, SPN_ENV_LIB, libs, sp_carr_len(libs));
      }
      break;
    }
  }
}

static void add_launcher(sp_mem_t mem, const spn_cc_toolchain_t* toolchain, const spn_profile_info_t* profile, spn_lang_t lang, spn_invocation_t* invocation) {
  spn_toolchain_launcher_t launcher = lang == SPN_LANG_CXX ? toolchain->cxx : toolchain->compiler;
  sp_assert(!spn_arg_empty(launcher.program));
  invocation->program = launcher.program;
  spn_cc_push_strs(mem, invocation, launcher.args);
  invocation->launcher = sp_da_size(invocation->args);
  if (spn_cc_has(toolchain, SPN_CC_CAP_TARGET_TRIPLE)) {
    sp_str_t target = render_target(mem, toolchain, spn_profile_triple(profile));
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
  spn_gnu_render_flags(mem, toolchain, profile, &flags);
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
  add_sdk_compile(mem, toolchain, profile, invocation);
  if (profile->os == SPN_OS_MACOS && is_os_version_present(compile->min_os)) {
    spn_cc_push_fmt(mem, invocation, "-mmacosx-version-min={}.{}", sp_fmt_uint(compile->min_os.major), sp_fmt_uint(compile->min_os.minor));
  }
  if (codeview(toolchain, profile)) {
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
  if (codeview(toolchain, profile)) {
    spn_cc_push_c(mem, invocation, "-Xclang");
    spn_cc_push_fmt(mem, invocation, "-object-file-name={}", sp_fmt_str(sp_fs_get_name(files->output.sub)));
  }
  spn_cc_push_c(mem, invocation, "-o");
  spn_cc_push_path(mem, invocation, files->output);
}

spn_err_t spn_gnu_parse_depfile(sp_mem_t mem, sp_str_t content, sp_da(sp_str_t)* prereqs) {
  occ_parser_t parser = sp_zero;
  if (occ_init(&parser, content)) {
    return SPN_ERROR;
  }
  sp_str_t prereq = sp_zero;
  while (occ_next(&parser, &prereq)) {
    sp_da_push(*prereqs, sp_str_copy(mem, prereq));
  }
  return parser.err ? SPN_ERROR : SPN_OK;
}

static void add_def(sp_mem_t mem, spn_ld_dialect_t dialect, spn_path_t def, spn_invocation_t* invocation) {
  switch (dialect) {
    case SPN_LD_DIALECT_GNU: {
      spn_cc_push_path(mem, invocation, def);
      break;
    }
    case SPN_LD_DIALECT_LINK: {
      spn_cc_push_glued(mem, invocation, "-Wl,/DEF:", def);
      break;
    }
    case SPN_LD_DIALECT_DARWIN:
    case SPN_LD_DIALECT_WASM:
    case SPN_LD_DIALECT_COUNT: {
      sp_unreachable_case();
    }
  }
}

static void add_exports(sp_mem_t mem, spn_format_t format, spn_ld_dialect_t dialect, spn_path_t exports, spn_invocation_t* invocation) {
  switch (format) {
    case SPN_FORMAT_ELF: {
      spn_cc_push_glued(mem, invocation, "-Wl,--version-script,", exports);
      break;
    }
    case SPN_FORMAT_COFF: {
      add_def(mem, dialect, exports, invocation);
      break;
    }
    case SPN_FORMAT_MACHO: {
      spn_cc_push_glued(mem, invocation, "-Wl,-exported_symbols_list,", exports);
      break;
    }
    case SPN_FORMAT_WASM:
    case SPN_FORMAT_COUNT: {
      sp_unreachable_case();
    }
  }
}

static void add_whole_archives(sp_mem_t mem, spn_ld_dialect_t dialect, sp_da(spn_path_t) archives, spn_invocation_t* invocation) {
  switch (dialect) {
    case SPN_LD_DIALECT_GNU:
    case SPN_LD_DIALECT_WASM: {
      spn_cc_push_c(mem, invocation, "-Wl,--whole-archive");
      spn_cc_push_paths(mem, invocation, archives);
      spn_cc_push_c(mem, invocation, "-Wl,--no-whole-archive");
      break;
    }
    case SPN_LD_DIALECT_LINK: {
      sp_da_for(archives, it) {
        spn_cc_push_glued(mem, invocation, "-Wl,/WHOLEARCHIVE:", archives[it]);
      }
      break;
    }
    case SPN_LD_DIALECT_DARWIN: {
      sp_da_for(archives, it) {
        spn_cc_push_glued(mem, invocation, "-Wl,-force_load,", archives[it]);
      }
      break;
    }
    case SPN_LD_DIALECT_COUNT: {
      sp_unreachable_case();
    }
  }
}

static void add_subsystem(sp_mem_t mem, spn_ld_dialect_t dialect, spn_invocation_t* invocation) {
  switch (dialect) {
    case SPN_LD_DIALECT_GNU: {
      spn_cc_push_c(mem, invocation, "-Wl,--subsystem,windows");
      break;
    }
    case SPN_LD_DIALECT_LINK: {
      spn_cc_push_c(mem, invocation, "-Wl,/SUBSYSTEM:WINDOWS");
      spn_cc_push_c(mem, invocation, "-Wl,/ENTRY:mainCRTStartup");
      break;
    }
    case SPN_LD_DIALECT_DARWIN:
    case SPN_LD_DIALECT_WASM:
    case SPN_LD_DIALECT_COUNT: {
      sp_unreachable_case();
    }
  }
}

static void add_rpath(sp_mem_t mem, spn_os_t os, spn_invocation_t* invocation) {
  switch (os) {
    case SPN_OS_LINUX: {
      spn_cc_push_c(mem, invocation, "-Wl,-rpath,$ORIGIN");
      break;
    }
    case SPN_OS_MACOS: {
      spn_cc_push_c(mem, invocation, "-Wl,-rpath,@loader_path");
      break;
    }
    case SPN_OS_WINDOWS: {
      break;
    }
    case SPN_OS_WASI:
    case SPN_OS_FREESTANDING:
    case SPN_OS_NONE: {
      sp_unreachable_case();
    }
  }
}

void spn_gnu_render_link(sp_mem_t mem, const spn_cc_toolchain_t* toolchain, const spn_profile_info_t* profile, const spn_cc_link_t* link, const spn_cc_link_files_t* files, spn_invocation_t* invocation) {
  spn_triple_t triple = spn_profile_triple(profile);
  spn_format_t format = spn_os_format(profile->os);
  spn_ld_dialect_t dialect = spn_ld_dialect(triple);

  add_launcher(mem, toolchain, profile, link->lang, invocation);
  spn_cc_push_strs(mem, invocation, toolchain->link_args);
  spn_cc_flags_t flags = sp_zero;
  sp_da_init(mem, flags.compile);
  sp_da_init(mem, flags.link);
  spn_gnu_render_flags(mem, toolchain, profile, &flags);
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
      if (format == SPN_FORMAT_MACHO) {
        spn_cc_push_fmt(mem, invocation, "-Wl,-install_name,@rpath/{}", sp_fmt_str(sp_fs_get_name(files->output.sub)));
      }
      if (!spn_path_empty(files->exports.path)) {
        add_exports(mem, format, dialect, files->exports.path, invocation);
      }
      break;
    }
    case SPN_CC_OUTPUT_EXE: {
      if (profile->linkage == SPN_LIB_KIND_STATIC && spn_ld_static(dialect)) {
        spn_cc_push_c(mem, invocation, "-static");
      }
      if (link->subsystem == SPN_WIN_SUBSYSTEM_WINDOWS && format == SPN_FORMAT_COFF) {
        add_subsystem(mem, dialect, invocation);
      }
      break;
    }
    case SPN_CC_OUTPUT_OBJECT:
    case SPN_CC_OUTPUT_STATIC_LIB: {
      sp_unreachable_case();
    }
  }
  sp_da_for(link->scripts, it) {
    spn_cc_push_glued(mem, invocation, "-Wl,-T,", link->scripts[it]);
  }
  spn_cc_push_strs(mem, invocation, link->args);
  spn_cc_push_paths(mem, invocation, files->objects);
  if (!sp_da_empty(files->whole_archives)) {
    add_whole_archives(mem, dialect, files->whole_archives, invocation);
  }
  sp_da_for(link->lib_dirs, it) {
    spn_cc_push_glued(mem, invocation, "-L", link->lib_dirs[it]);
  }
  sp_da_for(link->private_libs, it) {
    spn_cc_push_fmt(mem, invocation, "-l{}", sp_fmt_str(link->private_libs[it]));
  }
  sp_da_for(link->libs, it) {
    spn_cc_push_fmt(mem, invocation, "-l{}", sp_fmt_str(link->libs[it]));
  }
  sp_da_for(link->system_libs, it) {
    spn_cc_push_fmt(mem, invocation, "-l{}", sp_fmt_str(link->system_libs[it]));
  }
  add_sdk_link(mem, toolchain, profile, invocation);
  if (profile->os == SPN_OS_MACOS) {
    if (is_os_version_present(link->min_os)) {
      spn_cc_push_fmt(mem, invocation, "-mmacosx-version-min={}.{}", sp_fmt_uint(link->min_os.major), sp_fmt_uint(link->min_os.minor));
    }
    sp_da_for(link->frameworks, it) {
      spn_cc_push_c(mem, invocation, "-framework");
      spn_cc_push_str(mem, invocation, link->frameworks[it]);
    }
  }
  if (spn_triple_dynamic(triple)) {
    add_rpath(mem, profile->os, invocation);
  }
  spn_cc_push_c(mem, invocation, "-o");
  spn_cc_push_path(mem, invocation, files->output);
}

void spn_gnu_render_archive(sp_mem_t mem, const spn_cc_toolchain_t* toolchain, const spn_cc_archive_files_t* files, spn_invocation_t* invocation) {
  invocation->program = toolchain->archiver.program;
  spn_cc_push_strs(mem, invocation, toolchain->archiver.args);
  invocation->launcher = sp_da_size(invocation->args);
  spn_cc_push_c(mem, invocation, "rcs");
  spn_cc_push_path(mem, invocation, files->output);
  spn_cc_push_paths(mem, invocation, files->objects);
}
