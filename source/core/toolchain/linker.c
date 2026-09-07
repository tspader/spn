#include "toolchain/linker.h"

#include "macro/macro.h"
#include "toolchain/toolchain.h"

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

bool spn_ld_static(spn_ld_flavor_t flavor) {
  switch (flavor) {
    case SPN_LD_FLAVOR_ELF:
    case SPN_LD_FLAVOR_MINGW: return true;
    case SPN_LD_FLAVOR_MSVC:
    case SPN_LD_FLAVOR_MACHO:
    case SPN_LD_FLAVOR_WASM: return false;
    case SPN_LD_FLAVOR_COUNT: sp_unreachable_case();
  }
  SP_UNREACHABLE_RETURN(false);
}

bool spn_ld_scripts(spn_ld_family_t family, spn_ld_flavor_t flavor) {
  switch (family) {
    case SPN_LD_FAMILY_GNU: return flavor == SPN_LD_FLAVOR_ELF || flavor == SPN_LD_FLAVOR_MINGW;
    case SPN_LD_FAMILY_LLD: return flavor == SPN_LD_FLAVOR_ELF;
    case SPN_LD_FAMILY_LD64:
    case SPN_LD_FAMILY_MSVC: return false;
    case SPN_LD_FAMILY_NONE: sp_unreachable_case();
  }
  SP_UNREACHABLE_RETURN(false);
}

static spn_ld_family_t native(spn_ld_flavor_t flavor) {
  switch (flavor) {
    case SPN_LD_FLAVOR_ELF:
    case SPN_LD_FLAVOR_MINGW: return SPN_LD_FAMILY_GNU;
    case SPN_LD_FLAVOR_MSVC: return SPN_LD_FAMILY_MSVC;
    case SPN_LD_FLAVOR_MACHO: return SPN_LD_FAMILY_LD64;
    case SPN_LD_FLAVOR_WASM: return SPN_LD_FAMILY_LLD;
    case SPN_LD_FLAVOR_COUNT: sp_unreachable_case();
  }
  SP_UNREACHABLE_RETURN(SPN_LD_FAMILY_NONE);
}

static spn_ld_family_t fallback(spn_cc_driver_t driver, spn_ld_flavor_t flavor) {
  switch (driver) {
    case SPN_CC_DRIVER_GCC:
    case SPN_CC_DRIVER_CLANG:
    case SPN_CC_DRIVER_MSVC: return native(flavor);
    case SPN_CC_DRIVER_ZIG: return SPN_LD_FAMILY_LLD;
    case SPN_CC_DRIVER_NONE: sp_unreachable_case();
  }
  SP_UNREACHABLE_RETURN(SPN_LD_FAMILY_NONE);
}

static bool accepts(spn_cc_driver_t driver, spn_ld_flavor_t flavor, spn_ld_family_t family) {
  if (!(spn_toolchain_driver_caps(driver) & SPN_CC_CAP_FUSE_LD)) {
    return false;
  }
  if (!spn_toolchain_driver_produces(driver, flavor)) {
    return false;
  }
  return family == native(flavor) || family == SPN_LD_FAMILY_LLD;
}

static spn_ld_family_t declared_family(const spn_cg_linkers_t* declared, spn_ld_flavor_t flavor) {
  switch (flavor) {
    case SPN_LD_FLAVOR_ELF:   return sp_opt_is_null(declared->elf)   ? SPN_LD_FAMILY_NONE : sp_opt_get(declared->elf);
    case SPN_LD_FLAVOR_MINGW: return sp_opt_is_null(declared->mingw) ? SPN_LD_FAMILY_NONE : sp_opt_get(declared->mingw);
    case SPN_LD_FLAVOR_MSVC:  return sp_opt_is_null(declared->msvc)  ? SPN_LD_FAMILY_NONE : sp_opt_get(declared->msvc);
    case SPN_LD_FLAVOR_MACHO: return sp_opt_is_null(declared->macho) ? SPN_LD_FAMILY_NONE : sp_opt_get(declared->macho);
    case SPN_LD_FLAVOR_WASM:  return sp_opt_is_null(declared->wasm)  ? SPN_LD_FAMILY_NONE : sp_opt_get(declared->wasm);
    case SPN_LD_FLAVOR_COUNT: sp_unreachable_case();
  }
  SP_UNREACHABLE_RETURN(SPN_LD_FAMILY_NONE);
}

spn_ld_family_t spn_ld_family(const spn_toolchain_linkers_t* linkers, spn_triple_t target) {
  return linkers->families[spn_ld_flavor(target)];
}

spn_ld_issues_t spn_ld_resolve(spn_cc_driver_t driver, const spn_cg_linkers_t* declared, spn_toolchain_linkers_t* linkers) {
  spn_ld_issues_t issues = sp_zero;
  sp_for(flavor, SPN_LD_FLAVOR_COUNT) {
    linkers->families[flavor] = fallback(driver, (spn_ld_flavor_t)flavor);
  }
  sp_for(flavor, SPN_LD_FLAVOR_COUNT) {
    spn_ld_family_t family = declared_family(declared, (spn_ld_flavor_t)flavor);
    if (family == SPN_LD_FAMILY_NONE) {
      continue;
    }
    if (!accepts(driver, (spn_ld_flavor_t)flavor, family)) {
      issues.items[issues.count++] = (spn_ld_flavor_t)flavor;
      continue;
    }
    linkers->families[flavor] = family;
  }
  return issues;
}
