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
  spn_ld_family_set_t expect;
} families_t;

static const families_t families_tests [] = {
  { "gcc_elf",     SPN_CC_DRIVER_GCC,   SPN_LD_FLAVOR_ELF,   GNU | LLD },
  { "gcc_mingw",   SPN_CC_DRIVER_GCC,   SPN_LD_FLAVOR_MINGW, GNU | LLD },
  { "gcc_msvc",    SPN_CC_DRIVER_GCC,   SPN_LD_FLAVOR_MSVC,  0 },
  { "gcc_macho",   SPN_CC_DRIVER_GCC,   SPN_LD_FLAVOR_MACHO, LD64 },
  { "gcc_wasm",    SPN_CC_DRIVER_GCC,   SPN_LD_FLAVOR_WASM,  0 },
  { "clang_elf",   SPN_CC_DRIVER_CLANG, SPN_LD_FLAVOR_ELF,   GNU | LLD },
  { "clang_mingw", SPN_CC_DRIVER_CLANG, SPN_LD_FLAVOR_MINGW, GNU | LLD },
  { "clang_msvc",  SPN_CC_DRIVER_CLANG, SPN_LD_FLAVOR_MSVC,  MSVC | LLD },
  { "clang_macho", SPN_CC_DRIVER_CLANG, SPN_LD_FLAVOR_MACHO, LD64 | LLD },
  { "clang_wasm",  SPN_CC_DRIVER_CLANG, SPN_LD_FLAVOR_WASM,  LLD },
  { "zig_elf",     SPN_CC_DRIVER_ZIG,   SPN_LD_FLAVOR_ELF,   LLD },
  { "zig_mingw",   SPN_CC_DRIVER_ZIG,   SPN_LD_FLAVOR_MINGW, LLD },
  { "zig_msvc",    SPN_CC_DRIVER_ZIG,   SPN_LD_FLAVOR_MSVC,  0 },
  { "zig_macho",   SPN_CC_DRIVER_ZIG,   SPN_LD_FLAVOR_MACHO, LLD },
  { "zig_wasm",    SPN_CC_DRIVER_ZIG,   SPN_LD_FLAVOR_WASM,  LLD },
  { "msvc_elf",    SPN_CC_DRIVER_MSVC,  SPN_LD_FLAVOR_ELF,   0 },
  { "msvc_mingw",  SPN_CC_DRIVER_MSVC,  SPN_LD_FLAVOR_MINGW, 0 },
  { "msvc_msvc",   SPN_CC_DRIVER_MSVC,  SPN_LD_FLAVOR_MSVC,  MSVC },
  { "msvc_macho",  SPN_CC_DRIVER_MSVC,  SPN_LD_FLAVOR_MACHO, 0 },
  { "msvc_wasm",   SPN_CC_DRIVER_MSVC,  SPN_LD_FLAVOR_WASM,  0 },
};

sp_test_each(linker, families, families_t, families_tests) {
  sp_expect_eq(t, it->expect, spn_ld_families(it->driver, it->flavor));
  return SP_OK;
}

#define X64_LINUX_GNU   { SPN_ARCH_X64, SPN_OS_LINUX, SPN_ABI_GNU }
#define X64_WIN_MSVC    { SPN_ARCH_X64, SPN_OS_WINDOWS, SPN_ABI_MSVC }

typedef struct {
  const c8* name;
  spn_cc_driver_t driver;
  spn_triple_t target;
  spn_ld_family_t family;
  spn_ld_arg_t expect;
} arg_t;

static const arg_t arg_tests [] = {
  { "gcc_gnu",                    SPN_CC_DRIVER_GCC,   X64_LINUX_GNU,   SPN_LD_FAMILY_GNU,  SPN_LD_ARG_NONE },
  { "gcc_lld",                    SPN_CC_DRIVER_GCC,   X64_LINUX_GNU,   SPN_LD_FAMILY_LLD,  SPN_LD_ARG_FUSE_LLD },
  { "gcc_mingw_lld",              SPN_CC_DRIVER_GCC,   TARGET_WIN_GNU,  SPN_LD_FAMILY_LLD,  SPN_LD_ARG_FUSE_LLD },
  { "gcc_ld64",                   SPN_CC_DRIVER_GCC,   HOST_ARM_MACOS,  SPN_LD_FAMILY_LD64, SPN_LD_ARG_NONE },
  { "clang_linux_gnu",            SPN_CC_DRIVER_CLANG, X64_LINUX_GNU,   SPN_LD_FAMILY_GNU,  SPN_LD_ARG_LD_PATH },
  { "clang_linux_lld",            SPN_CC_DRIVER_CLANG, X64_LINUX_GNU,   SPN_LD_FAMILY_LLD,  SPN_LD_ARG_LD_PATH },
  { "clang_arm_bare_lld",         SPN_CC_DRIVER_CLANG, TARGET_ARM_BARE, SPN_LD_FAMILY_LLD,  SPN_LD_ARG_LD_PATH },
  { "clang_x64_bare_links_via_gcc_gnu", SPN_CC_DRIVER_CLANG, TARGET_X64_BARE, SPN_LD_FAMILY_GNU, SPN_LD_ARG_NONE },
  { "clang_x64_bare_links_via_gcc_lld", SPN_CC_DRIVER_CLANG, TARGET_X64_BARE, SPN_LD_FAMILY_LLD, SPN_LD_ARG_FUSE_LLD },
  { "clang_mingw_gnu",            SPN_CC_DRIVER_CLANG, TARGET_WIN_GNU,  SPN_LD_FAMILY_GNU,  SPN_LD_ARG_LD_PATH },
  { "clang_mingw_lld",            SPN_CC_DRIVER_CLANG, TARGET_WIN_GNU,  SPN_LD_FAMILY_LLD,  SPN_LD_ARG_LD_PATH },
  { "clang_ld64",                 SPN_CC_DRIVER_CLANG, HOST_ARM_MACOS,  SPN_LD_FAMILY_LD64, SPN_LD_ARG_LD_PATH },
  { "clang_macho_lld",            SPN_CC_DRIVER_CLANG, HOST_ARM_MACOS,  SPN_LD_FAMILY_LLD,  SPN_LD_ARG_LD_PATH },
  { "clang_msvc_msvc",            SPN_CC_DRIVER_CLANG, X64_WIN_MSVC,    SPN_LD_FAMILY_MSVC, SPN_LD_ARG_NONE },
  { "clang_msvc_lld",             SPN_CC_DRIVER_CLANG, X64_WIN_MSVC,    SPN_LD_FAMILY_LLD,  SPN_LD_ARG_FUSE_LLD },
  { "clang_wasm_lld",             SPN_CC_DRIVER_CLANG, TARGET_WASM,     SPN_LD_FAMILY_LLD,  SPN_LD_ARG_NONE },
  { "zig_lld",                    SPN_CC_DRIVER_ZIG,   X64_LINUX_GNU,   SPN_LD_FAMILY_LLD,  SPN_LD_ARG_NONE },
  { "msvc_msvc",                  SPN_CC_DRIVER_MSVC,  X64_WIN_MSVC,    SPN_LD_FAMILY_MSVC, SPN_LD_ARG_NONE },
};

sp_test_each(linker, arg, arg_t, arg_tests) {
  sp_expect_eq(t, (u32)it->expect, (u32)spn_ld_arg(it->driver, it->target, it->family));
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

typedef struct {
  spn_ld_issue_kind_t kind;
  spn_ld_flavor_t flavor;
  spn_ld_check_t check;
} issue_expect_t;

typedef struct {
  const c8* name;
  spn_cc_driver_t driver;
  fixture_linker_t declared [SPN_LD_FLAVOR_COUNT];
  fixture_linker_t linkers [SPN_LD_FLAVOR_COUNT];
  issue_expect_t issues [SPN_LD_FLAVOR_COUNT];
  u32 num_issues;
} resolve_t;

static const resolve_t resolve_tests [] = {
  {
    .name = "zig_is_lld_everywhere_but_msvc",
    .driver = SPN_CC_DRIVER_ZIG,
    .linkers = {
      [SPN_LD_FLAVOR_ELF] = { SPN_LD_FAMILY_LLD },
      [SPN_LD_FLAVOR_MINGW] = { SPN_LD_FAMILY_LLD },
      [SPN_LD_FLAVOR_MACHO] = { SPN_LD_FAMILY_LLD },
      [SPN_LD_FLAVOR_WASM] = { SPN_LD_FAMILY_LLD },
    },
  },
  {
    .name = "msvc_is_link_on_msvc_only",
    .driver = SPN_CC_DRIVER_MSVC,
    .linkers = { [SPN_LD_FLAVOR_MSVC] = { SPN_LD_FAMILY_MSVC } },
  },
  {
    .name = "fixed_driver_rejects_declaration",
    .driver = SPN_CC_DRIVER_MSVC,
    .declared = { [SPN_LD_FLAVOR_MSVC] = { SPN_LD_FAMILY_MSVC } },
    .linkers = { [SPN_LD_FLAVOR_MSVC] = { SPN_LD_FAMILY_MSVC } },
    .issues = { { SPN_LD_ISSUE_DECLARED } },
    .num_issues = 1,
  },
  {
    .name = "fixed_driver_rejects_program_only_declaration",
    .driver = SPN_CC_DRIVER_ZIG,
    .declared = { [SPN_LD_FLAVOR_MACHO] = { SPN_LD_FAMILY_NONE, "ld" } },
    .linkers = {
      [SPN_LD_FLAVOR_ELF] = { SPN_LD_FAMILY_LLD },
      [SPN_LD_FLAVOR_MINGW] = { SPN_LD_FAMILY_LLD },
      [SPN_LD_FLAVOR_MACHO] = { SPN_LD_FAMILY_LLD },
      [SPN_LD_FLAVOR_WASM] = { SPN_LD_FAMILY_LLD },
    },
    .issues = { { SPN_LD_ISSUE_DECLARED } },
    .num_issues = 1,
  },
  {
    .name = "declaring_driver_requires_a_slot",
    .driver = SPN_CC_DRIVER_GCC,
    .issues = { { SPN_LD_ISSUE_UNDECLARED } },
    .num_issues = 1,
  },
  {
    .name = "clang_requires_a_slot",
    .driver = SPN_CC_DRIVER_CLANG,
    .issues = { { SPN_LD_ISSUE_UNDECLARED } },
    .num_issues = 1,
  },
  {
    .name = "good_slots_are_kept",
    .driver = SPN_CC_DRIVER_CLANG,
    .declared = {
      [SPN_LD_FLAVOR_ELF] = { SPN_LD_FAMILY_LLD, "ld.lld" },
      [SPN_LD_FLAVOR_MSVC] = { SPN_LD_FAMILY_MSVC },
      [SPN_LD_FLAVOR_WASM] = { SPN_LD_FAMILY_LLD },
    },
    .linkers = {
      [SPN_LD_FLAVOR_ELF] = { SPN_LD_FAMILY_LLD, "ld.lld" },
      [SPN_LD_FLAVOR_MSVC] = { SPN_LD_FAMILY_MSVC },
      [SPN_LD_FLAVOR_WASM] = { SPN_LD_FAMILY_LLD },
    },
  },
  {
    .name = "family_is_required",
    .driver = SPN_CC_DRIVER_CLANG,
    .declared = { [SPN_LD_FLAVOR_ELF] = { SPN_LD_FAMILY_NONE, "ld" } },
    .issues = { { SPN_LD_ISSUE_SLOT, SPN_LD_FLAVOR_ELF, SPN_LD_CHECK_FAMILY_MISSING } },
    .num_issues = 1,
  },
  {
    .name = "family_missing_precedes_program_forbidden",
    .driver = SPN_CC_DRIVER_GCC,
    .declared = { [SPN_LD_FLAVOR_ELF] = { SPN_LD_FAMILY_NONE, "ld" } },
    .issues = { { SPN_LD_ISSUE_SLOT, SPN_LD_FLAVOR_ELF, SPN_LD_CHECK_FAMILY_MISSING } },
    .num_issues = 1,
  },
  {
    .name = "family_forbidden_precedes_program_missing",
    .driver = SPN_CC_DRIVER_CLANG,
    .declared = { [SPN_LD_FLAVOR_MACHO] = { SPN_LD_FAMILY_GNU } },
    .issues = { { SPN_LD_ISSUE_SLOT, SPN_LD_FLAVOR_MACHO, SPN_LD_CHECK_FAMILY_FORBIDDEN } },
    .num_issues = 1,
  },
  {
    .name = "clang_needs_a_program_where_it_links_itself",
    .driver = SPN_CC_DRIVER_CLANG,
    .declared = {
      [SPN_LD_FLAVOR_ELF] = { SPN_LD_FAMILY_GNU },
      [SPN_LD_FLAVOR_MINGW] = { SPN_LD_FAMILY_LLD },
      [SPN_LD_FLAVOR_MACHO] = { SPN_LD_FAMILY_LD64 },
    },
    .issues = {
      { SPN_LD_ISSUE_SLOT, SPN_LD_FLAVOR_ELF, SPN_LD_CHECK_PROGRAM_MISSING },
      { SPN_LD_ISSUE_SLOT, SPN_LD_FLAVOR_MINGW, SPN_LD_CHECK_PROGRAM_MISSING },
      { SPN_LD_ISSUE_SLOT, SPN_LD_FLAVOR_MACHO, SPN_LD_CHECK_PROGRAM_MISSING },
    },
    .num_issues = 3,
  },
  {
    .name = "clang_takes_no_program_where_it_cannot_pass_one",
    .driver = SPN_CC_DRIVER_CLANG,
    .declared = {
      [SPN_LD_FLAVOR_MSVC] = { SPN_LD_FAMILY_LLD, "lld-link" },
      [SPN_LD_FLAVOR_WASM] = { SPN_LD_FAMILY_LLD, "wasm-ld" },
    },
    .issues = {
      { SPN_LD_ISSUE_SLOT, SPN_LD_FLAVOR_MSVC, SPN_LD_CHECK_PROGRAM_FORBIDDEN },
      { SPN_LD_ISSUE_SLOT, SPN_LD_FLAVOR_WASM, SPN_LD_CHECK_PROGRAM_FORBIDDEN },
    },
    .num_issues = 2,
  },
  {
    .name = "gcc_takes_no_program",
    .driver = SPN_CC_DRIVER_GCC,
    .declared = { [SPN_LD_FLAVOR_ELF] = { SPN_LD_FAMILY_LLD, "ld.lld" } },
    .issues = { { SPN_LD_ISSUE_SLOT, SPN_LD_FLAVOR_ELF, SPN_LD_CHECK_PROGRAM_FORBIDDEN } },
    .num_issues = 1,
  },
  {
    .name = "bad_slots_are_reported_and_dropped",
    .driver = SPN_CC_DRIVER_GCC,
    .declared = {
      [SPN_LD_FLAVOR_ELF] = { SPN_LD_FAMILY_GNU },
      [SPN_LD_FLAVOR_MSVC] = { SPN_LD_FAMILY_MSVC },
      [SPN_LD_FLAVOR_MACHO] = { SPN_LD_FAMILY_LD64, "ld" },
    },
    .linkers = { [SPN_LD_FLAVOR_ELF] = { SPN_LD_FAMILY_GNU } },
    .issues = {
      { SPN_LD_ISSUE_SLOT, SPN_LD_FLAVOR_MSVC, SPN_LD_CHECK_FAMILY_FORBIDDEN },
      { SPN_LD_ISSUE_SLOT, SPN_LD_FLAVOR_MACHO, SPN_LD_CHECK_PROGRAM_FORBIDDEN },
    },
    .num_issues = 2,
  },
};

static spn_toolchain_linkers_t linkers_from(const fixture_linker_t* fixture) {
  spn_toolchain_linkers_t linkers = sp_zero;
  sp_for(flavor, SPN_LD_FLAVOR_COUNT) {
    linkers.slots[flavor].family = fixture[flavor].family;
    if (fixture[flavor].program) {
      linkers.slots[flavor].program = spn_arg_lit(sp_cstr_as_str(fixture[flavor].program));
    }
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
    sp_expect_eq(t, (u32)it->issues[at].flavor, (u32)issues.items[at].flavor);
    sp_expect_eq(t, (u32)it->issues[at].check, (u32)issues.items[at].check);
  }
  sp_for(flavor, SPN_LD_FLAVOR_COUNT) {
    sp_expect_eq(t, (u32)it->linkers[flavor].family, (u32)linkers.slots[flavor].family);
    sp_expect_str_eq_c(t, linkers.slots[flavor].program.prefix, it->linkers[flavor].program ? it->linkers[flavor].program : "");
  }
  return SP_OK;
}
