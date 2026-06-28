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

  return SPN_ARCH_NONE;
}

sp_str_t spn_arch_to_str(spn_arch_t arch) {
  switch (arch) {
    case SPN_ARCH_X64:   return sp_str_lit("x86_64");
    case SPN_ARCH_ARM64: return sp_str_lit("aarch64");
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
  return SPN_OS_NONE;
}

sp_str_t spn_os_to_str(spn_os_t os) {
  switch (os) {
    case SPN_OS_LINUX:   return sp_str_lit("linux");
    case SPN_OS_WINDOWS: return sp_str_lit("windows");
    case SPN_OS_MACOS:   return sp_str_lit("macos");
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
  if (sp_str_empty(str)) {
    return SPN_ABI_NONE;
  }

  return SPN_ABI_GNU;
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

// @spader @error
spn_build_mode_t spn_build_mode_from_str(sp_str_t str) {
  if (sp_str_equal_cstr(str, "release")) {
    return SPN_BUILD_MODE_RELEASE;
  }
  if (sp_str_equal_cstr(str, "debug")) {
    return SPN_BUILD_MODE_DEBUG;
  }
  if (sp_str_empty(str)) {
    return SPN_BUILD_MODE_NONE;
  }

  SP_FATAL("Unknown mode {.yellow}; options are [release, debug]", SP_FMT_STR(str));
  SP_UNREACHABLE_RETURN(SPN_BUILD_MODE_RELEASE);
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

  SP_UNREACHABLE_RETURN(SPN_LIB_KIND_SHARED);
}

spn_linkage_t spn_linkage_from_str(sp_str_t str) {
  return spn_lib_kind_from_str(str);
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
