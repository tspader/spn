#include "toolchain.h"

typedef struct {
  const c8* name;
  spn_cc_driver_t driver;
  spn_cc_cap_set_t expect;
} caps_t;

static const caps_t caps_tests [] = {
  { "gcc",   SPN_CC_DRIVER_GCC,   SPN_CC_CAP_NOLIBC | SPN_CC_CAP_FUSE_LD },
  { "clang", SPN_CC_DRIVER_CLANG, SPN_CC_CAP_TARGET_TRIPLE | SPN_CC_CAP_LLVM_TRIPLE | SPN_CC_CAP_CLANG_FRONTEND | SPN_CC_CAP_NOLIBC | SPN_CC_CAP_FUSE_LD },
  { "zig",   SPN_CC_DRIVER_ZIG,   SPN_CC_CAP_TARGET_TRIPLE | SPN_CC_CAP_CLANG_FRONTEND | SPN_CC_CAP_CODEVIEW | SPN_CC_CAP_LIBC_FILE },
  { "msvc",  SPN_CC_DRIVER_MSVC,  0 },
};

sp_test_each(driver, caps, caps_t, caps_tests) {
  sp_expect_eq(t, it->expect, spn_toolchain_driver_caps(it->driver));
  return SP_OK;
}

typedef struct {
  const c8* name;
  spn_cc_driver_t driver;
  spn_ld_dialect_t dialect;
  bool expect;
} composes_t;

static const composes_t composes_tests [] = {
  { "gcc_gnu",      SPN_CC_DRIVER_GCC,   SPN_LD_DIALECT_GNU,    true },
  { "gcc_link",     SPN_CC_DRIVER_GCC,   SPN_LD_DIALECT_LINK,   false },
  { "gcc_darwin",   SPN_CC_DRIVER_GCC,   SPN_LD_DIALECT_DARWIN, true },
  { "gcc_wasm",     SPN_CC_DRIVER_GCC,   SPN_LD_DIALECT_WASM,   false },
  { "clang_gnu",    SPN_CC_DRIVER_CLANG, SPN_LD_DIALECT_GNU,    true },
  { "clang_link",   SPN_CC_DRIVER_CLANG, SPN_LD_DIALECT_LINK,   true },
  { "clang_darwin", SPN_CC_DRIVER_CLANG, SPN_LD_DIALECT_DARWIN, true },
  { "clang_wasm",   SPN_CC_DRIVER_CLANG, SPN_LD_DIALECT_WASM,   true },
  { "zig_gnu",      SPN_CC_DRIVER_ZIG,   SPN_LD_DIALECT_GNU,    true },
  { "zig_link",     SPN_CC_DRIVER_ZIG,   SPN_LD_DIALECT_LINK,   true },
  { "zig_darwin",   SPN_CC_DRIVER_ZIG,   SPN_LD_DIALECT_DARWIN, true },
  { "zig_wasm",     SPN_CC_DRIVER_ZIG,   SPN_LD_DIALECT_WASM,   true },
  { "msvc_gnu",     SPN_CC_DRIVER_MSVC,  SPN_LD_DIALECT_GNU,    false },
  { "msvc_link",    SPN_CC_DRIVER_MSVC,  SPN_LD_DIALECT_LINK,   true },
  { "msvc_darwin",  SPN_CC_DRIVER_MSVC,  SPN_LD_DIALECT_DARWIN, false },
  { "msvc_wasm",    SPN_CC_DRIVER_MSVC,  SPN_LD_DIALECT_WASM,   false },
};

sp_test_each(driver, composes, composes_t, composes_tests) {
  sp_expect_eq(t, it->expect, spn_toolchain_driver_composes(it->driver, it->dialect));
  return SP_OK;
}

typedef struct {
  const c8* name;
  spn_cc_driver_t driver;
  spn_os_t os;
  spn_abi_t expect;
} default_abi_t;

static const default_abi_t default_abi_tests [] = {
  { "gcc_windows",   SPN_CC_DRIVER_GCC,   SPN_OS_WINDOWS,      SPN_ABI_GNU },
  { "clang_windows", SPN_CC_DRIVER_CLANG, SPN_OS_WINDOWS,      SPN_ABI_MSVC },
  { "zig_windows",   SPN_CC_DRIVER_ZIG,   SPN_OS_WINDOWS,      SPN_ABI_GNU },
  { "msvc_windows",  SPN_CC_DRIVER_MSVC,  SPN_OS_WINDOWS,      SPN_ABI_MSVC },
  { "gcc_linux",     SPN_CC_DRIVER_GCC,   SPN_OS_LINUX },
  { "clang_linux",   SPN_CC_DRIVER_CLANG, SPN_OS_LINUX },
  { "zig_linux",     SPN_CC_DRIVER_ZIG,   SPN_OS_LINUX },
  { "msvc_linux",    SPN_CC_DRIVER_MSVC,  SPN_OS_LINUX },
  { "gcc_macos",     SPN_CC_DRIVER_GCC,   SPN_OS_MACOS,        SPN_ABI_APPLE },
  { "clang_macos",   SPN_CC_DRIVER_CLANG, SPN_OS_MACOS,        SPN_ABI_APPLE },
  { "zig_macos",     SPN_CC_DRIVER_ZIG,   SPN_OS_MACOS,        SPN_ABI_APPLE },
  { "msvc_macos",    SPN_CC_DRIVER_MSVC,  SPN_OS_MACOS,        SPN_ABI_APPLE },
  { "gcc_wasi",      SPN_CC_DRIVER_GCC,   SPN_OS_WASI,         SPN_ABI_MUSL },
  { "clang_wasi",    SPN_CC_DRIVER_CLANG, SPN_OS_WASI,         SPN_ABI_MUSL },
  { "zig_wasi",      SPN_CC_DRIVER_ZIG,   SPN_OS_WASI,         SPN_ABI_MUSL },
  { "msvc_wasi",     SPN_CC_DRIVER_MSVC,  SPN_OS_WASI,         SPN_ABI_MUSL },
  { "gcc_bare",      SPN_CC_DRIVER_GCC,   SPN_OS_FREESTANDING, SPN_ABI_BARE },
  { "clang_bare",    SPN_CC_DRIVER_CLANG, SPN_OS_FREESTANDING, SPN_ABI_BARE },
  { "zig_bare",      SPN_CC_DRIVER_ZIG,   SPN_OS_FREESTANDING, SPN_ABI_BARE },
  { "msvc_bare",     SPN_CC_DRIVER_MSVC,  SPN_OS_FREESTANDING, SPN_ABI_BARE },
};

sp_test_each(driver, default_abi, default_abi_t, default_abi_tests) {
  sp_expect_eq(t, (u32)it->expect, (u32)spn_default_abi(it->driver, it->os));
  return SP_OK;
}

sp_test(driver, retargeting_drivers_compose_every_dialect) {
  sp_carr_for(composes_tests, it) {
    if (spn_toolchain_driver_retargets(composes_tests[it].driver)) {
      sp_expect(t, composes_tests[it].expect);
    }
  }
  return SP_OK;
}
