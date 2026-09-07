#include "toolchain.h"

typedef struct {
  const c8* name;
  spn_triple_t target;
  spn_ld_flavor_t expect;
} flavor_t;

static const flavor_t flavor_tests [] = {
  { "linux_gnu",    HOST_X64_LINUX,      SPN_LD_FLAVOR_ELF },
  { "linux_musl",   HOST_X64_LINUX_MUSL, SPN_LD_FLAVOR_ELF },
  { "freestanding", TARGET_X64_BARE,     SPN_LD_FLAVOR_ELF },
  { "windows_gnu",  TARGET_WIN_GNU,      SPN_LD_FLAVOR_MINGW },
  { "windows_msvc", TARGET_WIN_MSVC,   SPN_LD_FLAVOR_MSVC },
  { "macos",        HOST_ARM_MACOS,      SPN_LD_FLAVOR_MACHO },
  { "wasi",         TARGET_WASM,         SPN_LD_FLAVOR_WASM },
};

sp_test_each(linker, flavor, flavor_t, flavor_tests) {
  sp_expect_eq(t, (u32)it->expect, (u32)spn_ld_flavor(it->target));
  return SP_OK;
}

typedef struct {
  const c8* name;
  spn_ld_flavor_t flavor;
  bool expect;
} static_t;

static const static_t static_tests [] = {
  { "elf",   SPN_LD_FLAVOR_ELF,   true },
  { "mingw", SPN_LD_FLAVOR_MINGW, true },
  { "msvc",  SPN_LD_FLAVOR_MSVC,  false },
  { "macho", SPN_LD_FLAVOR_MACHO, false },
  { "wasm",  SPN_LD_FLAVOR_WASM,  false },
};

sp_test_each(linker, static, static_t, static_tests) {
  sp_expect_eq(t, it->expect, spn_ld_static(it->flavor));
  return SP_OK;
}

typedef struct {
  const c8* name;
  spn_ld_family_t family;
  spn_ld_flavor_t flavor;
  bool expect;
} scripts_t;

static const scripts_t scripts_tests [] = {
  { "gnu_elf",    SPN_LD_FAMILY_GNU,  SPN_LD_FLAVOR_ELF,   true },
  { "gnu_mingw",  SPN_LD_FAMILY_GNU,  SPN_LD_FLAVOR_MINGW, true },
  { "lld_elf",    SPN_LD_FAMILY_LLD,  SPN_LD_FLAVOR_ELF,   true },
  { "lld_mingw",  SPN_LD_FAMILY_LLD,  SPN_LD_FLAVOR_MINGW, false },
  { "lld_msvc",   SPN_LD_FAMILY_LLD,  SPN_LD_FLAVOR_MSVC,  false },
  { "lld_macho",  SPN_LD_FAMILY_LLD,  SPN_LD_FLAVOR_MACHO, false },
  { "lld_wasm",   SPN_LD_FAMILY_LLD,  SPN_LD_FLAVOR_WASM,  false },
  { "ld64_macho", SPN_LD_FAMILY_LD64, SPN_LD_FLAVOR_MACHO, false },
  { "msvc_msvc",  SPN_LD_FAMILY_MSVC, SPN_LD_FLAVOR_MSVC,  false },
};

sp_test_each(linker, scripts, scripts_t, scripts_tests) {
  sp_expect_eq(t, it->expect, spn_ld_scripts(it->family, it->flavor));
  return SP_OK;
}

typedef struct {
  spn_ld_family_t linkers [SPN_LD_FLAVOR_COUNT];
  spn_ld_flavor_t rejected [SPN_LD_FLAVOR_COUNT];
  u32 num_rejected;
} resolve_expect_t;

typedef struct {
  const c8* name;
  spn_cc_driver_t driver;
  spn_cg_linkers_t declared;
  resolve_expect_t expect;
} resolve_t;

static const resolve_t resolve_tests [] = {
  {
    .name = "zig_is_lld_everywhere",
    .driver = SPN_CC_DRIVER_ZIG,
    .expect = { .linkers = FAMILIES_LLD },
  },
  {
    .name = "msvc_is_native",
    .driver = SPN_CC_DRIVER_MSVC,
    .expect = { .linkers = FAMILIES_NATIVE },
  },
  {
    .name = "gcc_defaults_to_native",
    .driver = SPN_CC_DRIVER_GCC,
    .expect = { .linkers = FAMILIES_NATIVE },
  },
  {
    .name = "clang_defaults_to_native",
    .driver = SPN_CC_DRIVER_CLANG,
    .expect = { .linkers = FAMILIES_NATIVE },
  },
  {
    .name = "owning_driver_rejects_every_declared_slot",
    .driver = SPN_CC_DRIVER_ZIG,
    .declared = { .elf = sp_opt_some(SPN_LD_FAMILY_LLD), .macho = sp_opt_some(SPN_LD_FAMILY_LLD) },
    .expect = {
      .linkers = FAMILIES_LLD,
      .rejected = { SPN_LD_FLAVOR_ELF, SPN_LD_FLAVOR_MACHO },
      .num_rejected = 2,
    },
  },
  {
    .name = "declared_native_family_is_accepted",
    .driver = SPN_CC_DRIVER_GCC,
    .declared = { .elf = sp_opt_some(SPN_LD_FAMILY_GNU) },
    .expect = { .linkers = FAMILIES_NATIVE },
  },
  {
    .name = "declared_slot_overrides_its_default_only",
    .driver = SPN_CC_DRIVER_GCC,
    .declared = { .elf = sp_opt_some(SPN_LD_FAMILY_LLD) },
    .expect = {
      .linkers = {
        [SPN_LD_FLAVOR_ELF] = SPN_LD_FAMILY_LLD,
        [SPN_LD_FLAVOR_MINGW] = SPN_LD_FAMILY_GNU,
        [SPN_LD_FLAVOR_MSVC] = SPN_LD_FAMILY_MSVC,
        [SPN_LD_FLAVOR_MACHO] = SPN_LD_FAMILY_LD64,
        [SPN_LD_FLAVOR_WASM] = SPN_LD_FAMILY_LLD,
      },
    },
  },
  {
    .name = "foreign_dialect_is_rejected",
    .driver = SPN_CC_DRIVER_GCC,
    .declared = { .macho = sp_opt_some(SPN_LD_FAMILY_GNU) },
    .expect = {
      .linkers = FAMILIES_NATIVE,
      .rejected = { SPN_LD_FLAVOR_MACHO },
      .num_rejected = 1,
    },
  },
  {
    .name = "unproducible_flavor_is_rejected",
    .driver = SPN_CC_DRIVER_GCC,
    .declared = { .msvc = sp_opt_some(SPN_LD_FAMILY_LLD) },
    .expect = {
      .linkers = FAMILIES_NATIVE,
      .rejected = { SPN_LD_FLAVOR_MSVC },
      .num_rejected = 1,
    },
  },
  {
    .name = "clang_swaps_every_slot_to_lld",
    .driver = SPN_CC_DRIVER_CLANG,
    .declared = {
      .elf = sp_opt_some(SPN_LD_FAMILY_LLD),
      .mingw = sp_opt_some(SPN_LD_FAMILY_LLD),
      .msvc = sp_opt_some(SPN_LD_FAMILY_LLD),
      .macho = sp_opt_some(SPN_LD_FAMILY_LLD),
      .wasm = sp_opt_some(SPN_LD_FAMILY_LLD),
    },
    .expect = { .linkers = FAMILIES_LLD },
  },
};

sp_test_each(linker, resolve, resolve_t, resolve_tests) {
  spn_toolchain_linkers_t linkers = sp_zero;
  spn_ld_issues_t issues = spn_ld_resolve(it->driver, &it->declared, &linkers);

  sp_must_eq(t, it->expect.num_rejected, issues.count);
  sp_for(at, issues.count) {
    sp_expect_eq(t, (u32)it->expect.rejected[at], (u32)issues.items[at]);
  }
  sp_for(flavor, SPN_LD_FLAVOR_COUNT) {
    sp_test_kv(t, "flavor", spn_ld_flavor_to_str((spn_ld_flavor_t)flavor));
    sp_expect_eq(t, (u32)it->expect.linkers[flavor], (u32)linkers.families[flavor]);
  }
  sp_test_kv_clear(t, SP_NULLPTR);
  return SP_OK;
}
