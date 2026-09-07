#include "toolchain.h"

typedef struct {
  const c8* name;
  spn_cc_driver_t driver;
  spn_cc_cap_set_t expect;
} caps_t;

static const caps_t caps_tests [] = {
  { "gcc",   SPN_CC_DRIVER_GCC,   SPN_CC_CAP_NOLIBC | SPN_CC_CAP_FUSE_LD },
  { "clang", SPN_CC_DRIVER_CLANG, SPN_CC_CAP_TARGET_TRIPLE | SPN_CC_CAP_LLVM_TRIPLE | SPN_CC_CAP_CLANG_FRONTEND | SPN_CC_CAP_NOLIBC | SPN_CC_CAP_FUSE_LD },
  { "zig",   SPN_CC_DRIVER_ZIG,   SPN_CC_CAP_TARGET_TRIPLE | SPN_CC_CAP_CLANG_FRONTEND },
  { "msvc",  SPN_CC_DRIVER_MSVC,  0 },
};

sp_test_each(driver, caps, caps_t, caps_tests) {
  sp_expect_eq(t, it->expect, spn_toolchain_driver_caps(it->driver));
  return SP_OK;
}

typedef struct {
  const c8* name;
  spn_cc_driver_t driver;
  spn_ld_flavor_t flavor;
  bool expect;
} produces_t;

static const produces_t produces_tests [] = {
  { "gcc_elf",     SPN_CC_DRIVER_GCC,   SPN_LD_FLAVOR_ELF,   true },
  { "gcc_mingw",   SPN_CC_DRIVER_GCC,   SPN_LD_FLAVOR_MINGW, true },
  { "gcc_msvc",    SPN_CC_DRIVER_GCC,   SPN_LD_FLAVOR_MSVC,  false },
  { "gcc_macho",   SPN_CC_DRIVER_GCC,   SPN_LD_FLAVOR_MACHO, true },
  { "gcc_wasm",    SPN_CC_DRIVER_GCC,   SPN_LD_FLAVOR_WASM,  false },
  { "clang_elf",   SPN_CC_DRIVER_CLANG, SPN_LD_FLAVOR_ELF,   true },
  { "clang_mingw", SPN_CC_DRIVER_CLANG, SPN_LD_FLAVOR_MINGW, true },
  { "clang_msvc",  SPN_CC_DRIVER_CLANG, SPN_LD_FLAVOR_MSVC,  true },
  { "clang_macho", SPN_CC_DRIVER_CLANG, SPN_LD_FLAVOR_MACHO, true },
  { "clang_wasm",  SPN_CC_DRIVER_CLANG, SPN_LD_FLAVOR_WASM,  true },
  { "zig_elf",     SPN_CC_DRIVER_ZIG,   SPN_LD_FLAVOR_ELF,   true },
  { "zig_mingw",   SPN_CC_DRIVER_ZIG,   SPN_LD_FLAVOR_MINGW, true },
  { "zig_msvc",    SPN_CC_DRIVER_ZIG,   SPN_LD_FLAVOR_MSVC,  true },
  { "zig_macho",   SPN_CC_DRIVER_ZIG,   SPN_LD_FLAVOR_MACHO, true },
  { "zig_wasm",    SPN_CC_DRIVER_ZIG,   SPN_LD_FLAVOR_WASM,  true },
  { "msvc_elf",    SPN_CC_DRIVER_MSVC,  SPN_LD_FLAVOR_ELF,   false },
  { "msvc_mingw",  SPN_CC_DRIVER_MSVC,  SPN_LD_FLAVOR_MINGW, false },
  { "msvc_msvc",   SPN_CC_DRIVER_MSVC,  SPN_LD_FLAVOR_MSVC,  true },
  { "msvc_macho",  SPN_CC_DRIVER_MSVC,  SPN_LD_FLAVOR_MACHO, false },
  { "msvc_wasm",   SPN_CC_DRIVER_MSVC,  SPN_LD_FLAVOR_WASM,  false },
};

sp_test_each(driver, produces, produces_t, produces_tests) {
  sp_expect_eq(t, it->expect, spn_toolchain_driver_produces(it->driver, it->flavor));
  return SP_OK;
}

sp_test(driver, retargeting_drivers_produce_every_flavor) {
  sp_carr_for(produces_tests, it) {
    if (spn_toolchain_driver_retargets(produces_tests[it].driver)) {
      sp_expect(t, produces_tests[it].expect);
    }
  }
  return SP_OK;
}
