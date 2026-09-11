#include "toolchain.h"

typedef struct {
  const c8* name;
  spn_triple_t target;
  spn_ld_dialect_t expect;
} dialect_t;

static const dialect_t dialect_tests [] = {
  { "linux_gnu",    HOST_X64_LINUX,      SPN_LD_DIALECT_GNU },
  { "linux_musl",   HOST_X64_LINUX_MUSL, SPN_LD_DIALECT_GNU },
  { "freestanding", TARGET_X64_BARE,     SPN_LD_DIALECT_GNU },
  { "windows_gnu",  TARGET_WIN_GNU,      SPN_LD_DIALECT_GNU },
  { "windows_msvc", TARGET_WIN_MSVC,     SPN_LD_DIALECT_LINK },
  { "macos",        HOST_ARM_MACOS,      SPN_LD_DIALECT_DARWIN },
  { "wasi",         TARGET_WASM,         SPN_LD_DIALECT_WASM },
};

sp_test_each(linker, dialect, dialect_t, dialect_tests) {
  sp_expect_eq(t, (u32)it->expect, (u32)spn_ld_dialect(it->target));
  return SP_OK;
}

typedef struct {
  const c8* name;
  spn_ld_dialect_t dialect;
  bool expect;
} static_t;

static const static_t static_tests [] = {
  { "gnu",    SPN_LD_DIALECT_GNU,    true },
  { "link",   SPN_LD_DIALECT_LINK,   false },
  { "darwin", SPN_LD_DIALECT_DARWIN, false },
  { "wasm",   SPN_LD_DIALECT_WASM,   false },
};

sp_test_each(linker, static, static_t, static_tests) {
  sp_expect_eq(t, it->expect, spn_ld_static(it->dialect));
  return SP_OK;
}

typedef struct {
  const c8* name;
  spn_ld_family_t family;
  spn_format_t format;
  bool expect;
} scripts_t;

static const scripts_t scripts_tests [] = {
  { "gnu_elf",    SPN_LD_FAMILY_GNU,  SPN_FORMAT_ELF,   true },
  { "gnu_coff",   SPN_LD_FAMILY_GNU,  SPN_FORMAT_COFF,  true },
  { "lld_elf",    SPN_LD_FAMILY_LLD,  SPN_FORMAT_ELF,   true },
  { "lld_coff",   SPN_LD_FAMILY_LLD,  SPN_FORMAT_COFF,  false },
  { "lld_macho",  SPN_LD_FAMILY_LLD,  SPN_FORMAT_MACHO, false },
  { "lld_wasm",   SPN_LD_FAMILY_LLD,  SPN_FORMAT_WASM,  false },
  { "ld64_macho", SPN_LD_FAMILY_LD64, SPN_FORMAT_MACHO, false },
  { "msvc_coff",  SPN_LD_FAMILY_MSVC, SPN_FORMAT_COFF,  false },
};

sp_test_each(linker, scripts, scripts_t, scripts_tests) {
  sp_expect_eq(t, it->expect, spn_ld_scripts(it->family, it->format));
  return SP_OK;
}

typedef struct {
  const c8* name;
  spn_cc_driver_t driver;
  spn_ld_family_t declared;
  bool expect;
} accepts_t;

static const accepts_t accepts_tests [] = {
  { "nothing_declared",       SPN_CC_DRIVER_GCC,   SPN_LD_FAMILY_NONE, true },
  { "gcc_declares_lld",       SPN_CC_DRIVER_GCC,   SPN_LD_FAMILY_LLD,  true },
  { "clang_declares_lld",     SPN_CC_DRIVER_CLANG, SPN_LD_FAMILY_LLD,  true },
  { "zig_rejects_lld",        SPN_CC_DRIVER_ZIG,   SPN_LD_FAMILY_LLD,  false },
  { "msvc_rejects_lld",       SPN_CC_DRIVER_MSVC,  SPN_LD_FAMILY_LLD,  false },
  { "native_is_not_declared", SPN_CC_DRIVER_GCC,   SPN_LD_FAMILY_GNU,  false },
  { "foreign_is_rejected",    SPN_CC_DRIVER_CLANG, SPN_LD_FAMILY_LD64, false },
};

sp_test_each(linker, accepts, accepts_t, accepts_tests) {
  sp_expect_eq(t, it->expect, spn_ld_accepts(it->driver, it->declared));
  return SP_OK;
}

typedef struct {
  const c8* name;
  spn_cc_driver_t driver;
  spn_triple_t target;
  spn_ld_family_t expect;
} native_t;

static const native_t native_tests [] = {
  { "gcc_linux",   SPN_CC_DRIVER_GCC,   HOST_X64_LINUX,  SPN_LD_FAMILY_GNU },
  { "gcc_mingw",   SPN_CC_DRIVER_GCC,   TARGET_WIN_GNU,  SPN_LD_FAMILY_GNU },
  { "gcc_macos",   SPN_CC_DRIVER_GCC,   HOST_ARM_MACOS,  SPN_LD_FAMILY_LD64 },
  { "clang_msvc",  SPN_CC_DRIVER_CLANG, TARGET_WIN_MSVC, SPN_LD_FAMILY_MSVC },
  { "clang_wasi",  SPN_CC_DRIVER_CLANG, TARGET_WASM,     SPN_LD_FAMILY_LLD },
  { "msvc_msvc",   SPN_CC_DRIVER_MSVC,  TARGET_WIN_MSVC, SPN_LD_FAMILY_MSVC },
  { "zig_linux",   SPN_CC_DRIVER_ZIG,   HOST_X64_LINUX,  SPN_LD_FAMILY_LLD },
  { "zig_macos",   SPN_CC_DRIVER_ZIG,   HOST_ARM_MACOS,  SPN_LD_FAMILY_LLD },
};

sp_test_each(linker, native, native_t, native_tests) {
  sp_expect_eq(t, (u32)it->expect, (u32)spn_ld_native(it->driver, it->target));
  return SP_OK;
}
