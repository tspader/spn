#include "toolchain/linker.h"

#include "macro/macro.h"
#include "toolchain/toolchain.h"

spn_ld_dialect_t spn_ld_dialect(spn_triple_t target) {
  switch (target.os) {
    case SPN_OS_LINUX:
    case SPN_OS_FREESTANDING: return SPN_LD_DIALECT_GNU;
    case SPN_OS_WINDOWS: return target.abi == SPN_ABI_MSVC ? SPN_LD_DIALECT_LINK : SPN_LD_DIALECT_GNU;
    case SPN_OS_MACOS: return SPN_LD_DIALECT_DARWIN;
    case SPN_OS_WASI: return SPN_LD_DIALECT_WASM;
    case SPN_OS_NONE: sp_unreachable_case();
  }
  SP_UNREACHABLE_RETURN(SPN_LD_DIALECT_GNU);
}

bool spn_ld_static(spn_ld_dialect_t dialect) {
  switch (dialect) {
    case SPN_LD_DIALECT_GNU: return true;
    case SPN_LD_DIALECT_LINK:
    case SPN_LD_DIALECT_DARWIN:
    case SPN_LD_DIALECT_WASM: return false;
    case SPN_LD_DIALECT_COUNT: sp_unreachable_case();
  }
  SP_UNREACHABLE_RETURN(false);
}

bool spn_ld_scripts(spn_ld_family_t family, spn_format_t format) {
  switch (family) {
    case SPN_LD_FAMILY_GNU: return format == SPN_FORMAT_ELF || format == SPN_FORMAT_COFF;
    case SPN_LD_FAMILY_LLD: return format == SPN_FORMAT_ELF;
    case SPN_LD_FAMILY_LD64:
    case SPN_LD_FAMILY_MSVC: return false;
    case SPN_LD_FAMILY_NONE: sp_unreachable_case();
  }
  SP_UNREACHABLE_RETURN(false);
}

static spn_ld_family_t native(spn_ld_dialect_t dialect) {
  switch (dialect) {
    case SPN_LD_DIALECT_GNU: return SPN_LD_FAMILY_GNU;
    case SPN_LD_DIALECT_LINK: return SPN_LD_FAMILY_MSVC;
    case SPN_LD_DIALECT_DARWIN: return SPN_LD_FAMILY_LD64;
    case SPN_LD_DIALECT_WASM: return SPN_LD_FAMILY_LLD;
    case SPN_LD_DIALECT_COUNT: sp_unreachable_case();
  }
  SP_UNREACHABLE_RETURN(SPN_LD_FAMILY_NONE);
}

static spn_ld_family_t fallback(spn_cc_driver_t driver, spn_ld_dialect_t dialect) {
  switch (driver) {
    case SPN_CC_DRIVER_GCC:
    case SPN_CC_DRIVER_CLANG:
    case SPN_CC_DRIVER_MSVC: return native(dialect);
    case SPN_CC_DRIVER_ZIG: return SPN_LD_FAMILY_LLD;
    case SPN_CC_DRIVER_NONE: sp_unreachable_case();
  }
  SP_UNREACHABLE_RETURN(SPN_LD_FAMILY_NONE);
}

bool spn_ld_accepts(spn_cc_driver_t driver, spn_ld_family_t declared) {
  if (declared == SPN_LD_FAMILY_NONE) {
    return true;
  }
  return (spn_toolchain_driver_caps(driver) & SPN_CC_CAP_FUSE_LD) && declared == SPN_LD_FAMILY_LLD;
}

spn_ld_family_t spn_ld_family(spn_cc_driver_t driver, spn_ld_family_t linker, spn_triple_t target) {
  if (linker) {
    return linker;
  }
  return fallback(driver, spn_ld_dialect(target));
}
