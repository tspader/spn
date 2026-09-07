#include "toolchain.h"

#define GNU  spn_ld_family_bit(SPN_LD_FAMILY_GNU)
#define LLD  spn_ld_family_bit(SPN_LD_FAMILY_LLD)
#define LD64 spn_ld_family_bit(SPN_LD_FAMILY_LD64)
#define MSVC spn_ld_family_bit(SPN_LD_FAMILY_MSVC)

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
  { "windows_msvc", HOST_X64_WIN_MSVC,   SPN_LD_FLAVOR_MSVC },
  { "macos",        HOST_ARM_MACOS,      SPN_LD_FLAVOR_MACHO },
  { "wasi",         TARGET_WASM,         SPN_LD_FLAVOR_WASM },
};

sp_test_each(linker, flavor, flavor_t, flavor_tests) {
  sp_expect_eq(t, (u32)it->expect, (u32)spn_ld_flavor(it->target));
  return SP_OK;
}

typedef struct {
  const c8* name;
  spn_cc_driver_t driver;
  spn_ld_flavor_t flavor;
  spn_ld_family_set_t families;
  spn_ld_family_t fallback;
} driver_t;

static const driver_t driver_tests [] = {
  { "gcc_elf",     SPN_CC_DRIVER_GCC,   SPN_LD_FLAVOR_ELF,   GNU | LLD,   SPN_LD_FAMILY_GNU },
  { "gcc_mingw",   SPN_CC_DRIVER_GCC,   SPN_LD_FLAVOR_MINGW, GNU | LLD,   SPN_LD_FAMILY_GNU },
  { "gcc_msvc",    SPN_CC_DRIVER_GCC,   SPN_LD_FLAVOR_MSVC,  0,           SPN_LD_FAMILY_NONE },
  { "gcc_macho",   SPN_CC_DRIVER_GCC,   SPN_LD_FLAVOR_MACHO, LD64,        SPN_LD_FAMILY_LD64 },
  { "gcc_wasm",    SPN_CC_DRIVER_GCC,   SPN_LD_FLAVOR_WASM,  0,           SPN_LD_FAMILY_NONE },
  { "clang_elf",   SPN_CC_DRIVER_CLANG, SPN_LD_FLAVOR_ELF,   GNU | LLD,   SPN_LD_FAMILY_GNU },
  { "clang_mingw", SPN_CC_DRIVER_CLANG, SPN_LD_FLAVOR_MINGW, GNU | LLD,   SPN_LD_FAMILY_GNU },
  { "clang_msvc",  SPN_CC_DRIVER_CLANG, SPN_LD_FLAVOR_MSVC,  MSVC | LLD,  SPN_LD_FAMILY_MSVC },
  { "clang_macho", SPN_CC_DRIVER_CLANG, SPN_LD_FLAVOR_MACHO, LD64 | LLD,  SPN_LD_FAMILY_LD64 },
  { "clang_wasm",  SPN_CC_DRIVER_CLANG, SPN_LD_FLAVOR_WASM,  LLD,         SPN_LD_FAMILY_LLD },
  { "zig_elf",     SPN_CC_DRIVER_ZIG,   SPN_LD_FLAVOR_ELF,   LLD,         SPN_LD_FAMILY_LLD },
  { "zig_mingw",   SPN_CC_DRIVER_ZIG,   SPN_LD_FLAVOR_MINGW, LLD,         SPN_LD_FAMILY_LLD },
  { "zig_msvc",    SPN_CC_DRIVER_ZIG,   SPN_LD_FLAVOR_MSVC,  0,           SPN_LD_FAMILY_NONE },
  { "zig_macho",   SPN_CC_DRIVER_ZIG,   SPN_LD_FLAVOR_MACHO, LLD,         SPN_LD_FAMILY_LLD },
  { "zig_wasm",    SPN_CC_DRIVER_ZIG,   SPN_LD_FLAVOR_WASM,  LLD,         SPN_LD_FAMILY_LLD },
  { "msvc_elf",    SPN_CC_DRIVER_MSVC,  SPN_LD_FLAVOR_ELF,   0,           SPN_LD_FAMILY_NONE },
  { "msvc_mingw",  SPN_CC_DRIVER_MSVC,  SPN_LD_FLAVOR_MINGW, 0,           SPN_LD_FAMILY_NONE },
  { "msvc_msvc",   SPN_CC_DRIVER_MSVC,  SPN_LD_FLAVOR_MSVC,  MSVC,        SPN_LD_FAMILY_MSVC },
  { "msvc_macho",  SPN_CC_DRIVER_MSVC,  SPN_LD_FLAVOR_MACHO, 0,           SPN_LD_FAMILY_NONE },
  { "msvc_wasm",   SPN_CC_DRIVER_MSVC,  SPN_LD_FLAVOR_WASM,  0,           SPN_LD_FAMILY_NONE },
};

sp_test_each(linker, families, driver_t, driver_tests) {
  sp_expect_eq(t, it->families, spn_ld_families(it->driver, it->flavor));
  return SP_OK;
}

sp_test_each(linker, family_default, driver_t, driver_tests) {
  spn_ld_family_t fallback = spn_ld_family_default(it->driver, it->flavor);
  sp_expect_eq(t, (u32)it->fallback, (u32)fallback);
  sp_expect_eq(t, fallback != SPN_LD_FAMILY_NONE, spn_ld_families(it->driver, it->flavor) != 0);
  if (fallback) {
    sp_expect(t, spn_ld_families(it->driver, it->flavor) & spn_ld_family_bit(fallback));
  }
  return SP_OK;
}

typedef struct {
  const c8* name;
  spn_ld_family_t families [SPN_LD_FLAVOR_COUNT];
  spn_triple_t target;
  bool expect;
} links_t;

static const links_t links_tests [] = {
  { "elf_family_links_linux",        { [SPN_LD_FLAVOR_ELF] = SPN_LD_FAMILY_GNU },    HOST_X64_LINUX,    true },
  { "elf_family_links_freestanding", { [SPN_LD_FLAVOR_ELF] = SPN_LD_FAMILY_LLD },    TARGET_ARM_BARE,   true },
  { "other_flavor_does_not_link",    { [SPN_LD_FLAVOR_ELF] = SPN_LD_FAMILY_GNU },    HOST_ARM_MACOS,    false },
  { "windows_abi_picks_the_flavor",  { [SPN_LD_FLAVOR_MINGW] = SPN_LD_FAMILY_GNU },  HOST_X64_WIN_MSVC, false },
  { "nothing_links_nothing",         { 0 },                                          HOST_X64_LINUX,    false },
};

sp_test_each(linker, links, links_t, links_tests) {
  spn_toolchain_linkers_t linkers = sp_zero;
  sp_for(flavor, SPN_LD_FLAVOR_COUNT) {
    linkers.families[flavor] = it->families[flavor];
  }
  sp_expect_eq(t, it->expect, spn_ld_links(&linkers, it->target));
  return SP_OK;
}

typedef struct {
  const c8* name;
  spn_ld_family_t family;
  spn_ld_flavor_t flavor;
  spn_ld_cap_set_t expect;
} caps_t;

static const caps_t caps_tests [] = {
  { "gnu_elf",    SPN_LD_FAMILY_GNU,  SPN_LD_FLAVOR_ELF,   SPN_LD_CAP_SCRIPT },
  { "gnu_mingw",  SPN_LD_FAMILY_GNU,  SPN_LD_FLAVOR_MINGW, SPN_LD_CAP_SCRIPT | SPN_LD_CAP_EXCLUDE_LIBS },
  { "gnu_msvc",   SPN_LD_FAMILY_GNU,  SPN_LD_FLAVOR_MSVC,  0 },
  { "gnu_macho",  SPN_LD_FAMILY_GNU,  SPN_LD_FLAVOR_MACHO, 0 },
  { "gnu_wasm",   SPN_LD_FAMILY_GNU,  SPN_LD_FLAVOR_WASM,  0 },
  { "lld_elf",    SPN_LD_FAMILY_LLD,  SPN_LD_FLAVOR_ELF,   SPN_LD_CAP_SCRIPT },
  { "lld_mingw",  SPN_LD_FAMILY_LLD,  SPN_LD_FLAVOR_MINGW, 0 },
  { "lld_msvc",   SPN_LD_FAMILY_LLD,  SPN_LD_FLAVOR_MSVC,  0 },
  { "lld_macho",  SPN_LD_FAMILY_LLD,  SPN_LD_FLAVOR_MACHO, 0 },
  { "lld_wasm",   SPN_LD_FAMILY_LLD,  SPN_LD_FLAVOR_WASM,  0 },
  { "ld64_macho", SPN_LD_FAMILY_LD64, SPN_LD_FLAVOR_MACHO, 0 },
  { "msvc_msvc",  SPN_LD_FAMILY_MSVC, SPN_LD_FLAVOR_MSVC,  0 },
  { "none",       SPN_LD_FAMILY_NONE, SPN_LD_FLAVOR_ELF,   0 },
};

sp_test_each(linker, caps, caps_t, caps_tests) {
  sp_expect_eq(t, it->expect, spn_ld_caps(it->family, it->flavor));
  return SP_OK;
}

#define GCC_DEFAULTS { \
  [SPN_LD_FLAVOR_ELF] = SPN_LD_FAMILY_GNU, \
  [SPN_LD_FLAVOR_MINGW] = SPN_LD_FAMILY_GNU, \
  [SPN_LD_FLAVOR_MACHO] = SPN_LD_FAMILY_LD64, \
}

#define CLANG_DEFAULTS { \
  [SPN_LD_FLAVOR_ELF] = SPN_LD_FAMILY_GNU, \
  [SPN_LD_FLAVOR_MINGW] = SPN_LD_FAMILY_GNU, \
  [SPN_LD_FLAVOR_MSVC] = SPN_LD_FAMILY_MSVC, \
  [SPN_LD_FLAVOR_MACHO] = SPN_LD_FAMILY_LD64, \
  [SPN_LD_FLAVOR_WASM] = SPN_LD_FAMILY_LLD, \
}

#define ZIG_FIXED { \
  [SPN_LD_FLAVOR_ELF] = SPN_LD_FAMILY_LLD, \
  [SPN_LD_FLAVOR_MINGW] = SPN_LD_FAMILY_LLD, \
  [SPN_LD_FLAVOR_MACHO] = SPN_LD_FAMILY_LLD, \
  [SPN_LD_FLAVOR_WASM] = SPN_LD_FAMILY_LLD, \
}

#define MSVC_FIXED { [SPN_LD_FLAVOR_MSVC] = SPN_LD_FAMILY_MSVC }

typedef struct {
  const c8* name;
  spn_cc_driver_t driver;
  spn_ld_family_t declared [SPN_LD_FLAVOR_COUNT];
  spn_ld_family_t linkers [SPN_LD_FLAVOR_COUNT];
  spn_ld_issue_t issues [SPN_LD_FLAVOR_COUNT];
  u32 num_issues;
} resolve_t;

static const resolve_t resolve_tests [] = {
  {
    .name = "zig_is_lld_everywhere_but_msvc",
    .driver = SPN_CC_DRIVER_ZIG,
    .linkers = ZIG_FIXED,
  },
  {
    .name = "msvc_is_link_on_msvc_only",
    .driver = SPN_CC_DRIVER_MSVC,
    .linkers = MSVC_FIXED,
  },
  {
    .name = "msvc_rejects_declaration",
    .driver = SPN_CC_DRIVER_MSVC,
    .declared = { [SPN_LD_FLAVOR_MSVC] = SPN_LD_FAMILY_MSVC },
    .linkers = MSVC_FIXED,
    .issues = { { SPN_LD_ISSUE_DECLARED } },
    .num_issues = 1,
  },
  {
    .name = "zig_rejects_declaration",
    .driver = SPN_CC_DRIVER_ZIG,
    .declared = { [SPN_LD_FLAVOR_ELF] = SPN_LD_FAMILY_LLD },
    .linkers = ZIG_FIXED,
    .issues = { { SPN_LD_ISSUE_DECLARED } },
    .num_issues = 1,
  },
  {
    .name = "gcc_defaults_when_undeclared",
    .driver = SPN_CC_DRIVER_GCC,
    .linkers = GCC_DEFAULTS,
  },
  {
    .name = "clang_defaults_when_undeclared",
    .driver = SPN_CC_DRIVER_CLANG,
    .linkers = CLANG_DEFAULTS,
  },
  {
    .name = "declared_slot_overrides_its_default_only",
    .driver = SPN_CC_DRIVER_GCC,
    .declared = { [SPN_LD_FLAVOR_ELF] = SPN_LD_FAMILY_LLD },
    .linkers = {
      [SPN_LD_FLAVOR_ELF] = SPN_LD_FAMILY_LLD,
      [SPN_LD_FLAVOR_MINGW] = SPN_LD_FAMILY_GNU,
      [SPN_LD_FLAVOR_MACHO] = SPN_LD_FAMILY_LD64,
    },
  },
  {
    .name = "forbidden_family_is_reported_and_defaulted",
    .driver = SPN_CC_DRIVER_GCC,
    .declared = {
      [SPN_LD_FLAVOR_MSVC] = SPN_LD_FAMILY_MSVC,
      [SPN_LD_FLAVOR_MACHO] = SPN_LD_FAMILY_GNU,
    },
    .linkers = GCC_DEFAULTS,
    .issues = {
      { SPN_LD_ISSUE_FORBIDDEN, SPN_LD_FLAVOR_MSVC },
      { SPN_LD_ISSUE_FORBIDDEN, SPN_LD_FLAVOR_MACHO },
    },
    .num_issues = 2,
  },
  {
    .name = "clang_swaps_every_slot_to_lld",
    .driver = SPN_CC_DRIVER_CLANG,
    .declared = {
      [SPN_LD_FLAVOR_ELF] = SPN_LD_FAMILY_LLD,
      [SPN_LD_FLAVOR_MINGW] = SPN_LD_FAMILY_LLD,
      [SPN_LD_FLAVOR_MSVC] = SPN_LD_FAMILY_LLD,
      [SPN_LD_FLAVOR_MACHO] = SPN_LD_FAMILY_LLD,
    },
    .linkers = {
      [SPN_LD_FLAVOR_ELF] = SPN_LD_FAMILY_LLD,
      [SPN_LD_FLAVOR_MINGW] = SPN_LD_FAMILY_LLD,
      [SPN_LD_FLAVOR_MSVC] = SPN_LD_FAMILY_LLD,
      [SPN_LD_FLAVOR_MACHO] = SPN_LD_FAMILY_LLD,
      [SPN_LD_FLAVOR_WASM] = SPN_LD_FAMILY_LLD,
    },
  },
};

static spn_toolchain_linkers_t linkers_from(const spn_ld_family_t* families) {
  spn_toolchain_linkers_t linkers = sp_zero;
  sp_for(flavor, SPN_LD_FLAVOR_COUNT) {
    linkers.families[flavor] = families[flavor];
  }
  return linkers;
}

sp_test_each(linker, resolve, resolve_t, resolve_tests) {
  spn_toolchain_linkers_t declared = linkers_from(it->declared);
  spn_toolchain_linkers_t linkers = sp_zero;
  spn_ld_issues_t issues = spn_ld_resolve(it->driver, &declared, &linkers);

  sp_must_eq(t, it->num_issues, issues.count);
  sp_for(at, issues.count) {
    sp_expect_eq(t, (u32)it->issues[at].kind, (u32)issues.items[at].kind);
    if (issues.items[at].kind == SPN_LD_ISSUE_FORBIDDEN) {
      sp_expect_eq(t, (u32)it->issues[at].flavor, (u32)issues.items[at].flavor);
    }
  }
  sp_for(flavor, SPN_LD_FLAVOR_COUNT) {
    sp_test_kv(t, "flavor", spn_ld_flavor_to_str((spn_ld_flavor_t)flavor));
    sp_expect_eq(t, (u32)it->linkers[flavor], (u32)linkers.families[flavor]);
  }
  sp_test_kv_clear(t, SP_NULLPTR);
  return SP_OK;
}
