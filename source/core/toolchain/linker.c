#include "toolchain/linker.h"

#include "macro/macro.h"
#include "paths/paths.h"

spn_ld_flavor_t spn_ld_flavor(spn_triple_t target) {
  switch (target.os) {
    case SPN_OS_LINUX:
    case SPN_OS_FREESTANDING: return SPN_LD_FLAVOR_ELF;
    case SPN_OS_WINDOWS: return target.abi == SPN_ABI_MSVC ? SPN_LD_FLAVOR_MSVC : SPN_LD_FLAVOR_MINGW;
    case SPN_OS_MACOS: return SPN_LD_FLAVOR_MACHO;
    case SPN_OS_WASI: return SPN_LD_FLAVOR_WASM;
    case SPN_OS_NONE: sp_unreachable_case();
  }
  SP_UNREACHABLE_RETURN(SPN_LD_FLAVOR_ELF);
}

static spn_ld_family_set_t gcc_families(spn_ld_flavor_t flavor) {
  switch (flavor) {
    case SPN_LD_FLAVOR_ELF:
    case SPN_LD_FLAVOR_MINGW: return spn_ld_family_bit(SPN_LD_FAMILY_GNU) | spn_ld_family_bit(SPN_LD_FAMILY_LLD);
    case SPN_LD_FLAVOR_MACHO: return spn_ld_family_bit(SPN_LD_FAMILY_LD64);
    case SPN_LD_FLAVOR_MSVC:
    case SPN_LD_FLAVOR_WASM: return 0;
    case SPN_LD_FLAVOR_COUNT: sp_unreachable_case();
  }
  SP_UNREACHABLE_RETURN(0);
}

static spn_ld_family_set_t clang_families(spn_ld_flavor_t flavor) {
  switch (flavor) {
    case SPN_LD_FLAVOR_ELF:
    case SPN_LD_FLAVOR_MINGW: return spn_ld_family_bit(SPN_LD_FAMILY_GNU) | spn_ld_family_bit(SPN_LD_FAMILY_LLD);
    case SPN_LD_FLAVOR_MSVC: return spn_ld_family_bit(SPN_LD_FAMILY_MSVC) | spn_ld_family_bit(SPN_LD_FAMILY_LLD);
    case SPN_LD_FLAVOR_MACHO: return spn_ld_family_bit(SPN_LD_FAMILY_LD64) | spn_ld_family_bit(SPN_LD_FAMILY_LLD);
    case SPN_LD_FLAVOR_WASM: return spn_ld_family_bit(SPN_LD_FAMILY_LLD);
    case SPN_LD_FLAVOR_COUNT: sp_unreachable_case();
  }
  SP_UNREACHABLE_RETURN(0);
}

static spn_ld_family_t zig_family(spn_ld_flavor_t flavor) {
  switch (flavor) {
    case SPN_LD_FLAVOR_ELF:
    case SPN_LD_FLAVOR_MINGW:
    case SPN_LD_FLAVOR_MACHO:
    case SPN_LD_FLAVOR_WASM: return SPN_LD_FAMILY_LLD;
    case SPN_LD_FLAVOR_MSVC: return SPN_LD_FAMILY_NONE;
    case SPN_LD_FLAVOR_COUNT: sp_unreachable_case();
  }
  SP_UNREACHABLE_RETURN(SPN_LD_FAMILY_NONE);
}

static spn_ld_family_t msvc_family(spn_ld_flavor_t flavor) {
  return flavor == SPN_LD_FLAVOR_MSVC ? SPN_LD_FAMILY_MSVC : SPN_LD_FAMILY_NONE;
}

static spn_ld_family_set_t fixed_families(spn_ld_family_t family) {
  return family == SPN_LD_FAMILY_NONE ? 0 : spn_ld_family_bit(family);
}

spn_ld_family_set_t spn_ld_families(spn_cc_driver_t driver, spn_ld_flavor_t flavor) {
  switch (driver) {
    case SPN_CC_DRIVER_GCC: return gcc_families(flavor);
    case SPN_CC_DRIVER_CLANG: return clang_families(flavor);
    case SPN_CC_DRIVER_ZIG: return fixed_families(zig_family(flavor));
    case SPN_CC_DRIVER_MSVC: return fixed_families(msvc_family(flavor));
    case SPN_CC_DRIVER_NONE: sp_unreachable_case();
  }
  SP_UNREACHABLE_RETURN(0);
}

static spn_ld_arg_t gcc_arg(spn_ld_family_t family) {
  return family == SPN_LD_FAMILY_LLD ? SPN_LD_ARG_FUSE_LLD : SPN_LD_ARG_NONE;
}

static bool clang_links_through_gcc(spn_triple_t target) {
  return target.os == SPN_OS_FREESTANDING && target.arch == SPN_ARCH_X64;
}

static spn_ld_arg_t clang_arg(spn_triple_t target, spn_ld_family_t family) {
  if (clang_links_through_gcc(target)) {
    return gcc_arg(family);
  }
  switch (spn_ld_flavor(target)) {
    case SPN_LD_FLAVOR_ELF:
    case SPN_LD_FLAVOR_MINGW:
    case SPN_LD_FLAVOR_MACHO: return SPN_LD_ARG_LD_PATH;
    case SPN_LD_FLAVOR_MSVC: return gcc_arg(family);
    case SPN_LD_FLAVOR_WASM: return SPN_LD_ARG_NONE;
    case SPN_LD_FLAVOR_COUNT: sp_unreachable_case();
  }
  SP_UNREACHABLE_RETURN(SPN_LD_ARG_NONE);
}

spn_ld_arg_t spn_ld_arg(spn_cc_driver_t driver, spn_triple_t target, spn_ld_family_t family) {
  switch (driver) {
    case SPN_CC_DRIVER_GCC: return gcc_arg(family);
    case SPN_CC_DRIVER_CLANG: return clang_arg(target, family);
    case SPN_CC_DRIVER_ZIG:
    case SPN_CC_DRIVER_MSVC: return SPN_LD_ARG_NONE;
    case SPN_CC_DRIVER_NONE: sp_unreachable_case();
  }
  SP_UNREACHABLE_RETURN(SPN_LD_ARG_NONE);
}

static bool program_required(spn_cc_driver_t driver, spn_ld_flavor_t flavor) {
  if (driver != SPN_CC_DRIVER_CLANG) {
    return false;
  }
  switch (flavor) {
    case SPN_LD_FLAVOR_ELF:
    case SPN_LD_FLAVOR_MINGW:
    case SPN_LD_FLAVOR_MACHO: return true;
    case SPN_LD_FLAVOR_MSVC:
    case SPN_LD_FLAVOR_WASM: return false;
    case SPN_LD_FLAVOR_COUNT: sp_unreachable_case();
  }
  SP_UNREACHABLE_RETURN(false);
}

static spn_ld_cap_set_t gnu_caps(spn_ld_flavor_t flavor) {
  switch (flavor) {
    case SPN_LD_FLAVOR_ELF: return SPN_LD_CAP_SCRIPT;
    case SPN_LD_FLAVOR_MINGW: return SPN_LD_CAP_SCRIPT | SPN_LD_CAP_EXCLUDE_LIBS;
    case SPN_LD_FLAVOR_MSVC:
    case SPN_LD_FLAVOR_MACHO:
    case SPN_LD_FLAVOR_WASM: return 0;
    case SPN_LD_FLAVOR_COUNT: sp_unreachable_case();
  }
  SP_UNREACHABLE_RETURN(0);
}

spn_ld_cap_set_t spn_ld_caps(spn_ld_family_t family, spn_ld_flavor_t flavor) {
  switch (family) {
    case SPN_LD_FAMILY_GNU: return gnu_caps(flavor);
    case SPN_LD_FAMILY_LLD: return flavor == SPN_LD_FLAVOR_ELF ? SPN_LD_CAP_SCRIPT : 0;
    case SPN_LD_FAMILY_LD64:
    case SPN_LD_FAMILY_MSVC:
    case SPN_LD_FAMILY_NONE: return 0;
  }
  SP_UNREACHABLE_RETURN(0);
}

static bool fixed(spn_cc_driver_t driver) {
  switch (driver) {
    case SPN_CC_DRIVER_ZIG:
    case SPN_CC_DRIVER_MSVC: return true;
    case SPN_CC_DRIVER_GCC:
    case SPN_CC_DRIVER_CLANG: return false;
    case SPN_CC_DRIVER_NONE: sp_unreachable_case();
  }
  SP_UNREACHABLE_RETURN(false);
}

static spn_toolchain_linker_t fixed_linker(spn_cc_driver_t driver, spn_ld_flavor_t flavor) {
  switch (driver) {
    case SPN_CC_DRIVER_ZIG: return (spn_toolchain_linker_t) { .family = zig_family(flavor) };
    case SPN_CC_DRIVER_MSVC: return (spn_toolchain_linker_t) { .family = msvc_family(flavor) };
    case SPN_CC_DRIVER_GCC:
    case SPN_CC_DRIVER_CLANG:
    case SPN_CC_DRIVER_NONE: sp_unreachable_case();
  }
  SP_UNREACHABLE_RETURN(sp_zero_struct(spn_toolchain_linker_t));
}

static bool present(spn_toolchain_linker_t linker) {
  return linker.family != SPN_LD_FAMILY_NONE || !spn_arg_empty(linker.program);
}

bool spn_ld_links(const spn_toolchain_linkers_t* linkers, spn_triple_t target) {
  return present(linkers->slots[spn_ld_flavor(target)]);
}

static bool any_declared(const spn_toolchain_linkers_t* linkers) {
  sp_for(flavor, SPN_LD_FLAVOR_COUNT) {
    if (present(linkers->slots[flavor])) {
      return true;
    }
  }
  return false;
}

static void push_issue(spn_ld_issues_t* issues, spn_ld_issue_t issue) {
  issues->items[issues->count++] = issue;
}

static spn_ld_check_t check_slot(spn_cc_driver_t driver, spn_ld_flavor_t flavor, spn_toolchain_linker_t linker) {
  if (linker.family == SPN_LD_FAMILY_NONE) {
    return SPN_LD_CHECK_FAMILY_MISSING;
  }
  if (!(spn_ld_families(driver, flavor) & spn_ld_family_bit(linker.family))) {
    return SPN_LD_CHECK_FAMILY_FORBIDDEN;
  }
  bool needs_program = program_required(driver, flavor);
  bool has_program = !spn_arg_empty(linker.program);
  if (needs_program && !has_program) {
    return SPN_LD_CHECK_PROGRAM_MISSING;
  }
  if (!needs_program && has_program) {
    return SPN_LD_CHECK_PROGRAM_FORBIDDEN;
  }
  return SPN_LD_CHECK_OK;
}

spn_ld_issues_t spn_ld_resolve(spn_cc_driver_t driver, const spn_toolchain_linkers_t* declared, spn_toolchain_linkers_t* linkers) {
  spn_ld_issues_t issues = sp_zero;
  *linkers = sp_zero_s(spn_toolchain_linkers_t);

  if (fixed(driver)) {
    if (any_declared(declared)) {
      push_issue(&issues, (spn_ld_issue_t) { .kind = SPN_LD_ISSUE_DECLARED });
    }
    sp_for(flavor, SPN_LD_FLAVOR_COUNT) {
      linkers->slots[flavor] = fixed_linker(driver, (spn_ld_flavor_t)flavor);
    }
    return issues;
  }

  if (!any_declared(declared)) {
    push_issue(&issues, (spn_ld_issue_t) { .kind = SPN_LD_ISSUE_UNDECLARED });
    return issues;
  }
  sp_for(flavor, SPN_LD_FLAVOR_COUNT) {
    spn_toolchain_linker_t linker = declared->slots[flavor];
    if (!present(linker)) {
      continue;
    }
    spn_ld_check_t check = check_slot(driver, (spn_ld_flavor_t)flavor, linker);
    if (check == SPN_LD_CHECK_OK) {
      linkers->slots[flavor] = linker;
      continue;
    }
    push_issue(&issues, (spn_ld_issue_t) { .kind = SPN_LD_ISSUE_SLOT, .flavor = (spn_ld_flavor_t)flavor, .check = check });
  }
  return issues;
}
