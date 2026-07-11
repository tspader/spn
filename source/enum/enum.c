#include "enum/enum.h"

#include "sp/macro.h"
#include "spn.h"

spn_arch_t spn_arch_from_str(sp_str_t str) {
  if (sp_str_equal_cstr(str, "x86_64")) {
    return SPN_ARCH_X64;
  }
  else if (sp_str_equal_cstr(str, "aarch64")) {
    return SPN_ARCH_ARM64;
  }
  else if (sp_str_equal_cstr(str, "wasm32")) {
    return SPN_ARCH_WASM32;
  }

  return SPN_ARCH_NONE;
}

sp_str_t spn_arch_to_str(spn_arch_t arch) {
  switch (arch) {
    case SPN_ARCH_X64:   return sp_str_lit("x86_64");
    case SPN_ARCH_ARM64: return sp_str_lit("aarch64");
    case SPN_ARCH_WASM32: return sp_str_lit("wasm32");
    case SPN_ARCH_NONE:  return sp_str_lit("");
  }
  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

spn_os_t spn_os_from_sp_os(sp_os_kind_t os) {
  switch (os) {
    case SP_OS_LINUX: return SPN_OS_LINUX;
    case SP_OS_WIN32: return SPN_OS_WINDOWS;
    case SP_OS_MACOS: return SPN_OS_MACOS;
  }

  sp_unreachable_return(SPN_OS_LINUX);
}

spn_os_t spn_os_from_str(sp_str_t str) {
  if (sp_str_equal_cstr(str, "windows")) {
    return SPN_OS_WINDOWS;
  }
  else if (sp_str_equal_cstr(str, "linux")) {
    return SPN_OS_LINUX;
  }
  else if (sp_str_equal_cstr(str, "macos")) {
    return SPN_OS_MACOS;
  }
  else if (sp_str_equal_cstr(str, "wasi")) {
    return SPN_OS_WASI;
  }
  return SPN_OS_NONE;
}

sp_str_t spn_os_to_str(spn_os_t os) {
  switch (os) {
    case SPN_OS_LINUX:   return sp_str_lit("linux");
    case SPN_OS_WINDOWS: return sp_str_lit("windows");
    case SPN_OS_MACOS:   return sp_str_lit("macos");
    case SPN_OS_WASI:   return sp_str_lit("wasi");
    case SPN_OS_NONE:    return sp_str_lit("");
  }
  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

spn_cc_driver_t spn_cc_driver_from_str(sp_str_t str) {
  if (sp_str_equal_cstr(str, "gcc")) {
    return SPN_CC_DRIVER_GCC;
  }
  if (sp_str_equal_cstr(str, "clang")) {
    return SPN_CC_DRIVER_CLANG;
  }
  if (sp_str_equal_cstr(str, "msvc")) {
    return SPN_CC_DRIVER_MSVC;
  }

  return SPN_CC_DRIVER_NONE;
}

sp_str_t spn_cc_driver_to_str(spn_cc_driver_t driver) {
  switch (driver) {
    case SPN_CC_DRIVER_NONE: {
      return sp_str_lit("");
    }
    case SPN_CC_DRIVER_GCC: {
      return sp_str_lit("gcc");
    }
    case SPN_CC_DRIVER_CLANG: {
      return sp_str_lit("clang");
    }
    case SPN_CC_DRIVER_MSVC: {
      return sp_str_lit("msvc");
    }
  }

  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

spn_abi_t spn_abi_from_str(sp_str_t str) {
  if (sp_str_equal_cstr(str, "gnu")) {
    return SPN_ABI_GNU;
  }
  if (sp_str_equal_cstr(str, "musl")) {
    return SPN_ABI_MUSL;
  }
  if (sp_str_equal_cstr(str, "msvc")) {
    return SPN_ABI_MSVC;
  }
  if (sp_str_equal_cstr(str, "mingw")) {
    return SPN_ABI_MINGW;
  }

  return SPN_ABI_NONE;
}

sp_str_t spn_abi_to_str(spn_abi_t abi) {
  switch (abi) {
    case SPN_ABI_GNU: {
      return sp_str_lit("gnu");
    }
    case SPN_ABI_MUSL: {
      return sp_str_lit("musl");
    }
    case SPN_ABI_MSVC: {
      return sp_str_lit("msvc");
    }
    case SPN_ABI_MINGW: {
      return sp_str_lit("mingw");
    }
    case SPN_ABI_NONE: {
      return sp_str_lit("");
    }
  }

  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

spn_libc_kind_t spn_libc_kind_from_str(sp_str_t str) {
  if (sp_str_equal_cstr(str, "gnu")) {
    return SPN_LIBC_GNU;
  }
  if (sp_str_equal_cstr(str, "musl")) {
    return SPN_LIBC_MUSL;
  }
  if (sp_str_equal_cstr(str, "cosmopolitan")) {
    return SPN_LIBC_COSMOPOLITAN;
  }
  if (sp_str_equal_cstr(str, "custom")) {
    return SPN_LIBC_CUSTOM;
  }

  SP_UNREACHABLE_RETURN(SPN_LIBC_GNU);
}

sp_str_t spn_libc_kind_to_str(spn_libc_kind_t libc) {
  switch (libc) {
    case SPN_LIBC_GNU: {
      return SP_LIT("gnu");
    }
    case SPN_LIBC_MUSL: {
      return SP_LIT("musl");
    }
    case SPN_LIBC_COSMOPOLITAN: {
      return SP_LIT("cosmopolitan");
    }
    case SPN_LIBC_CUSTOM: {
      return SP_LIT("custom");
    }
  }

  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

spn_build_mode_t spn_build_mode_from_str(sp_str_t str) {
  if (sp_str_equal_cstr(str, "release")) {
    return SPN_BUILD_MODE_RELEASE;
  }
  if (sp_str_equal_cstr(str, "debug")) {
    return SPN_BUILD_MODE_DEBUG;
  }

  return SPN_BUILD_MODE_NONE;
}

sp_str_t spn_build_mode_to_str(spn_build_mode_t mode) {
  switch (mode) {
    case SPN_BUILD_MODE_RELEASE: {
      return sp_str_lit("release");
    }
    case SPN_BUILD_MODE_DEBUG: {
      return sp_str_lit("debug");
    }
    case SPN_BUILD_MODE_NONE: {
      return sp_str_lit("");
    }
  }

  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

spn_opt_level_t spn_opt_level_from_str(sp_str_t str) {
  if (sp_str_equal_cstr(str, "0")) {
    return SPN_OPT_LEVEL_0;
  }
  if (sp_str_equal_cstr(str, "1")) {
    return SPN_OPT_LEVEL_1;
  }
  if (sp_str_equal_cstr(str, "2")) {
    return SPN_OPT_LEVEL_2;
  }
  if (sp_str_equal_cstr(str, "3")) {
    return SPN_OPT_LEVEL_3;
  }
  if (sp_str_equal_cstr(str, "s")) {
    return SPN_OPT_LEVEL_S;
  }
  if (sp_str_equal_cstr(str, "z")) {
    return SPN_OPT_LEVEL_Z;
  }

  return SPN_OPT_LEVEL_NONE;
}

sp_str_t spn_opt_level_to_str(spn_opt_level_t level) {
  switch (level) {
    case SPN_OPT_LEVEL_0: {
      return sp_str_lit("0");
    }
    case SPN_OPT_LEVEL_1: {
      return sp_str_lit("1");
    }
    case SPN_OPT_LEVEL_2: {
      return sp_str_lit("2");
    }
    case SPN_OPT_LEVEL_3: {
      return sp_str_lit("3");
    }
    case SPN_OPT_LEVEL_S: {
      return sp_str_lit("s");
    }
    case SPN_OPT_LEVEL_Z: {
      return sp_str_lit("z");
    }
    case SPN_OPT_LEVEL_NONE: {
      return sp_str_lit("");
    }
  }

  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

spn_sanitizer_t spn_sanitizer_from_str(sp_str_t str) {
  if (sp_str_equal_cstr(str, "address")) {
    return SPN_SANITIZER_ADDRESS;
  }
  if (sp_str_equal_cstr(str, "thread")) {
    return SPN_SANITIZER_THREAD;
  }
  if (sp_str_equal_cstr(str, "undefined")) {
    return SPN_SANITIZER_UNDEFINED;
  }
  if (sp_str_equal_cstr(str, "memory")) {
    return SPN_SANITIZER_MEMORY;
  }
  if (sp_str_equal_cstr(str, "leak")) {
    return SPN_SANITIZER_LEAK;
  }

  return SPN_SANITIZER_NONE;
}

sp_str_t spn_sanitizer_to_str(spn_sanitizer_t sanitizer) {
  switch (sanitizer) {
    case SPN_SANITIZER_ADDRESS: {
      return sp_str_lit("address");
    }
    case SPN_SANITIZER_THREAD: {
      return sp_str_lit("thread");
    }
    case SPN_SANITIZER_UNDEFINED: {
      return sp_str_lit("undefined");
    }
    case SPN_SANITIZER_MEMORY: {
      return sp_str_lit("memory");
    }
    case SPN_SANITIZER_LEAK: {
      return sp_str_lit("leak");
    }
    case SPN_SANITIZER_NONE: {
      return sp_str_lit("");
    }
  }

  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

sp_str_t spn_sanitizer_set_to_str(sp_mem_t mem, spn_sanitizer_set_t set) {
  sp_io_dyn_mem_writer_t out;
  sp_io_dyn_mem_writer_init(mem, &out);
  bool first = true;
  sp_for(it, 5) {
    spn_sanitizer_set_t bit = (spn_sanitizer_set_t)1 << it;
    if (!(set & bit)) {
      continue;
    }
    sp_fmt_io(&out.base, first ? "{}" : ",{}", sp_fmt_str(spn_sanitizer_to_str((spn_sanitizer_t)bit)));
    first = false;
  }
  return sp_io_dyn_mem_writer_as_str(&out);
}

bool spn_sanitizer_set_conflicting(spn_sanitizer_set_t set) {
  if ((set & SPN_SANITIZER_THREAD) && (set & (SPN_SANITIZER_ADDRESS | SPN_SANITIZER_MEMORY | SPN_SANITIZER_LEAK))) {
    return true;
  }
  if ((set & SPN_SANITIZER_MEMORY) && (set & (SPN_SANITIZER_ADDRESS | SPN_SANITIZER_LEAK))) {
    return true;
  }
  return false;
}

spn_linkage_t spn_lib_kind_from_str(sp_str_t str) {
  if (sp_str_equal_cstr(str, "shared")) {
    return SPN_LIB_KIND_SHARED;
  }
  if (sp_str_equal_cstr(str, "static")) {
    return SPN_LIB_KIND_STATIC;
  }
  if (sp_str_equal_cstr(str, "source")) {
    return SPN_LIB_KIND_SOURCE;
  }
  if (sp_str_equal_cstr(str, "object")) {
    return SPN_LIB_KIND_OBJECT;
  }

  return SPN_LIB_KIND_NONE;
}

spn_linkage_t spn_linkage_from_str(sp_str_t str) {
  return spn_lib_kind_from_str(str);
}

spn_option_type_t spn_option_type_from_str(sp_str_t str) {
  if (sp_str_equal_cstr(str, "bool")) {
    return SPN_OPTION_TYPE_BOOL;
  }
  if (sp_str_equal_cstr(str, "enum")) {
    return SPN_OPTION_TYPE_ENUM;
  }

  return SPN_OPTION_TYPE_NONE;
}

sp_str_t spn_option_type_to_str(spn_option_type_t type) {
  switch (type) {
    case SPN_OPTION_TYPE_BOOL: {
      return sp_str_lit("bool");
    }
    case SPN_OPTION_TYPE_ENUM: {
      return sp_str_lit("enum");
    }
    case SPN_OPTION_TYPE_NONE: {
      return sp_str_lit("");
    }
  }

  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

sp_str_t spn_option_setter_to_str(spn_option_setter_t setter) {
  switch (setter.kind) {
    case SPN_OPTION_SETTER_NONE: {
      return sp_str_lit("");
    }
    case SPN_OPTION_SETTER_DEFAULT: {
      return sp_str_lit("the default");
    }
    case SPN_OPTION_SETTER_PROFILE: {
      return sp_str_lit("the profile");
    }
    case SPN_OPTION_SETTER_ROOT_MANIFEST: {
      return sp_str_lit("the root manifest");
    }
    case SPN_OPTION_SETTER_UNION: {
      return sp_str_lit("the union of requests");
    }
    case SPN_OPTION_SETTER_CONSUMER: {
      return setter.name;
    }
  }
  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

sp_str_t spn_linkage_to_str(spn_linkage_t kind) {
  switch (kind) {
    case SPN_LIB_KIND_SHARED: {
      return sp_str_lit("shared");
    }
    case SPN_LIB_KIND_STATIC: {
      return sp_str_lit("static");
    }
    case SPN_LIB_KIND_SOURCE: {
      return sp_str_lit("source");
    }
    case SPN_LIB_KIND_OBJECT: {
      return sp_str_lit("object");
    }
    case SPN_LIB_KIND_NONE: sp_unreachable_case();
  }

  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

spn_dir_t spn_cache_dir_kind_from_str(sp_str_t str) {
  if (sp_str_equal_cstr(str, "")) {
    return SPN_DIR_STORE;
  }
  if (sp_str_equal_cstr(str, "cache")) {
    return SPN_DIR_CACHE;
  }
  if (sp_str_equal_cstr(str, "store")) {
    return SPN_DIR_STORE;
  }
  if (sp_str_equal_cstr(str, "include")) {
    return SPN_DIR_INCLUDE;
  }
  if (sp_str_equal_cstr(str, "vendor")) {
    return SPN_DIR_VENDOR;
  }
  if (sp_str_equal_cstr(str, "lib")) {
    return SPN_DIR_LIB;
  }
  if (sp_str_equal_cstr(str, "source")) {
    return SPN_DIR_SOURCE;
  }
  if (sp_str_equal_cstr(str, "work")) {
    return SPN_DIR_WORK;
  }
  if (sp_str_equal_cstr(str, "project")) {
    return SPN_DIR_PROJECT;
  }

  SP_UNREACHABLE_RETURN(SPN_DIR_CACHE);
}

spn_cc_kind_t spn_cc_kind_from_str(sp_str_t str) {
  if (sp_str_equal_cstr(str, "")) {
    return SPN_CC_NONE;
  }
  if (sp_str_equal_cstr(str, "tcc")) {
    return SPN_CC_TCC;
  }
  if (sp_str_equal_cstr(str, "gcc")) {
    return SPN_CC_GCC;
  }
  if (sp_str_equal_cstr(str, "clang")) {
    return SPN_CC_CLANG;
  }
  if (sp_str_equal_cstr(str, "musl-gcc")) {
    return SPN_CC_MUSL_GCC;
  }
  if (sp_str_equal_cstr(str, "zcc")) {
    return SPN_CC_ZIG;
  }
  if (sp_str_equal_cstr(str, "zig cc")) {
    return SPN_CC_ZIG;
  }
  if (sp_str_equal_cstr(str, "cosmocc")) {
    return SPN_CC_COSMOCC;
  }

  return SPN_CC_CUSTOM;
}

sp_str_t spn_c_standard_to_str(spn_c_standard_t standard) {
  switch (standard) {
    case SPN_C11: {
      return sp_str_lit("c11");
    }
    case SPN_C99: {
      return sp_str_lit("c99");
    }
    case SPN_C89: {
      return sp_str_lit("c89");
    }
    case SPN_C_STANDARD_NONE: {
      return sp_str_lit("");
    }
  }

  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

sp_str_t spn_cxx_standard_to_str(spn_cxx_standard_t standard) {
  switch (standard) {
    case SPN_CXX11: {
      return sp_str_lit("c++11");
    }
    case SPN_CXX14: {
      return sp_str_lit("c++14");
    }
    case SPN_CXX17: {
      return sp_str_lit("c++17");
    }
    case SPN_CXX20: {
      return sp_str_lit("c++20");
    }
    case SPN_CXX23: {
      return sp_str_lit("c++23");
    }
    case SPN_CXX_STANDARD_NONE: {
      return sp_str_lit("");
    }
  }

  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

spn_cxx_standard_t spn_cxx_standard_from_str(sp_str_t str) {
  if (sp_str_equal_cstr(str, "c++11")) {
    return SPN_CXX11;
  }
  if (sp_str_equal_cstr(str, "c++14")) {
    return SPN_CXX14;
  }
  if (sp_str_equal_cstr(str, "c++17")) {
    return SPN_CXX17;
  }
  if (sp_str_equal_cstr(str, "c++20")) {
    return SPN_CXX20;
  }
  if (sp_str_equal_cstr(str, "c++23")) {
    return SPN_CXX23;
  }

  return SPN_CXX_STANDARD_NONE;
}

spn_lang_t spn_lang_from_path(sp_str_t path) {
  sp_str_t ext = sp_fs_get_ext(path);
  const c8* cxx [] = { "cpp", "cc", "cxx", "c++", "C" };
  sp_carr_for(cxx, it) {
    if (sp_str_equal_cstr(ext, cxx[it])) {
      return SPN_LANG_CXX;
    }
  }
  return SPN_LANG_C;
}

spn_c_standard_t spn_c_standard_from_str(sp_str_t str) {
  if (sp_str_equal_cstr(str, "c89")) {
    return SPN_C89;
  }
  if (sp_str_equal_cstr(str, "c99")) {
    return SPN_C99;
  }
  if (sp_str_equal_cstr(str, "c11")) {
    return SPN_C11;
  }
  if (sp_str_empty(str)) {
    return SPN_C_STANDARD_NONE;
  }

  SP_UNREACHABLE_RETURN(SPN_C99);
}

sp_str_t spn_pkg_source_to_str(spn_pkg_source_t kind) {
  switch (kind) {
    case SPN_PKG_SOURCE_ROOT: {
      return sp_str_lit("root");
    }
    case SPN_PKG_SOURCE_FILE: {
      return sp_str_lit("file");
    }
    case SPN_PKG_SOURCE_INDEX: {
      return sp_str_lit("index");
    }
  }

  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

spn_pkg_source_t spn_pkg_source_from_str(sp_str_t str) {
  if (sp_str_equal_cstr(str, "root")) {
    return SPN_PKG_SOURCE_ROOT;
  }
  if (sp_str_equal_cstr(str, "file")) {
    return SPN_PKG_SOURCE_FILE;
  }
  if (sp_str_equal_cstr(str, "index")) {
    return SPN_PKG_SOURCE_INDEX;
  }

  SP_UNREACHABLE_RETURN(SPN_PKG_SOURCE_INDEX);
}

sp_str_t spn_index_kind_to_str(spn_index_kind_t kind) {
  switch (kind) {
    case SPN_INDEX_WORKSPACE: {
      return sp_str_lit("workspace");
    }
    case SPN_INDEX_BUILTIN: {
      return sp_str_lit("builtin");
    }
    case SPN_INDEX_USER: {
      return sp_str_lit("user");
    }
  }

  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

spn_index_kind_t spn_index_kind_from_str(sp_str_t str) {
  if (sp_str_equal_cstr(str, "workspace")) {
    return SPN_INDEX_WORKSPACE;
  }
  if (sp_str_equal_cstr(str, "builtin")) {
    return SPN_INDEX_BUILTIN;
  }
  if (sp_str_equal_cstr(str, "user")) {
    return SPN_INDEX_USER;
  }

  SP_UNREACHABLE_RETURN(SPN_INDEX_WORKSPACE);
}

sp_str_t spn_index_dep_kind_to_str(spn_index_dep_kind_t kind) {
  switch (kind) {
    case SPN_INDEX_DEP_NORMAL: {
      return sp_str_lit("normal");
    }
    case SPN_INDEX_DEP_BUILD: {
      return sp_str_lit("build");
    }
    case SPN_INDEX_DEP_TEST: {
      return sp_str_lit("test");
    }
  }

  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

spn_index_dep_kind_t spn_index_dep_kind_from_str(sp_str_t str) {
  if (sp_str_equal_cstr(str, "normal")) {
    return SPN_INDEX_DEP_NORMAL;
  }
  if (sp_str_equal_cstr(str, "build")) {
    return SPN_INDEX_DEP_BUILD;
  }
  if (sp_str_equal_cstr(str, "test")) {
    return SPN_INDEX_DEP_TEST;
  }

  return SPN_INDEX_DEP_NORMAL;
}

sp_str_t spn_index_protocol_to_str(spn_index_protocol_t protocol) {
  switch (protocol) {
    case SPN_INDEX_PROTOCOL_GIT: {
      return sp_str_lit("git");
    }
    case SPN_INDEX_PROTOCOL_HTTP: {
      return sp_str_lit("http");
    }
    case SPN_INDEX_PROTOCOL_FILESYSTEM: {
      return sp_str_lit("filesystem");
    }
  }

  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

spn_index_protocol_t spn_index_protocol_from_str(sp_str_t str) {
  if (sp_str_equal_cstr(str, "git")) {
    return SPN_INDEX_PROTOCOL_GIT;
  }
  if (sp_str_equal_cstr(str, "http")) {
    return SPN_INDEX_PROTOCOL_HTTP;
  }
  if (sp_str_equal_cstr(str, "filesystem")) {
    return SPN_INDEX_PROTOCOL_FILESYSTEM;
  }

  SP_UNREACHABLE_RETURN(SPN_INDEX_PROTOCOL_GIT);
}

sp_os_lib_kind_t spn_lib_kind_to_sp_os_lib_kind(spn_linkage_t kind) {
  switch (kind) {
    case SPN_LIB_KIND_SHARED: {
      return SP_OS_LIB_SHARED;
    }
    case SPN_LIB_KIND_STATIC: {
      return SP_OS_LIB_STATIC;
    }
    case SPN_LIB_KIND_NONE:
    case SPN_LIB_KIND_SOURCE:
    case SPN_LIB_KIND_OBJECT: {
      return 0;
    }
  }

  SP_UNREACHABLE_RETURN(0);
}
