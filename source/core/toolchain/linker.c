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

typedef struct {
  spn_ld_family_t family;
  spn_ld_family_set_t extras;
} slot_t;

static slot_t gcc_slot(spn_ld_flavor_t flavor) {
  switch (flavor) {
    case SPN_LD_FLAVOR_ELF:
    case SPN_LD_FLAVOR_MINGW: return (slot_t) { SPN_LD_FAMILY_GNU, spn_ld_family_bit(SPN_LD_FAMILY_LLD) };
    case SPN_LD_FLAVOR_MACHO: return (slot_t) { SPN_LD_FAMILY_LD64 };
    case SPN_LD_FLAVOR_MSVC:
    case SPN_LD_FLAVOR_WASM: return (slot_t) { SPN_LD_FAMILY_NONE };
    case SPN_LD_FLAVOR_COUNT: sp_unreachable_case();
  }
  SP_UNREACHABLE_RETURN(sp_zero_struct(slot_t));
}

static slot_t clang_slot(spn_ld_flavor_t flavor) {
  switch (flavor) {
    case SPN_LD_FLAVOR_ELF:
    case SPN_LD_FLAVOR_MINGW: return (slot_t) { SPN_LD_FAMILY_GNU, spn_ld_family_bit(SPN_LD_FAMILY_LLD) };
    case SPN_LD_FLAVOR_MSVC: return (slot_t) { SPN_LD_FAMILY_MSVC, spn_ld_family_bit(SPN_LD_FAMILY_LLD) };
    case SPN_LD_FLAVOR_MACHO: return (slot_t) { SPN_LD_FAMILY_LD64, spn_ld_family_bit(SPN_LD_FAMILY_LLD) };
    case SPN_LD_FLAVOR_WASM: return (slot_t) { SPN_LD_FAMILY_LLD };
    case SPN_LD_FLAVOR_COUNT: sp_unreachable_case();
  }
  SP_UNREACHABLE_RETURN(sp_zero_struct(slot_t));
}

static slot_t zig_slot(spn_ld_flavor_t flavor) {
  switch (flavor) {
    case SPN_LD_FLAVOR_ELF:
    case SPN_LD_FLAVOR_MINGW:
    case SPN_LD_FLAVOR_MACHO:
    case SPN_LD_FLAVOR_WASM: return (slot_t) { SPN_LD_FAMILY_LLD };
    case SPN_LD_FLAVOR_MSVC: return (slot_t) { SPN_LD_FAMILY_NONE };
    case SPN_LD_FLAVOR_COUNT: sp_unreachable_case();
  }
  SP_UNREACHABLE_RETURN(sp_zero_struct(slot_t));
}

static slot_t msvc_slot(spn_ld_flavor_t flavor) {
  return (slot_t) { flavor == SPN_LD_FLAVOR_MSVC ? SPN_LD_FAMILY_MSVC : SPN_LD_FAMILY_NONE };
}

static slot_t slot(spn_cc_driver_t driver, spn_ld_flavor_t flavor) {
  switch (driver) {
    case SPN_CC_DRIVER_GCC: return gcc_slot(flavor);
    case SPN_CC_DRIVER_CLANG: return clang_slot(flavor);
    case SPN_CC_DRIVER_ZIG: return zig_slot(flavor);
    case SPN_CC_DRIVER_MSVC: return msvc_slot(flavor);
    case SPN_CC_DRIVER_NONE: sp_unreachable_case();
  }
  SP_UNREACHABLE_RETURN(sp_zero_struct(slot_t));
}

spn_ld_family_t spn_ld_family_default(spn_cc_driver_t driver, spn_ld_flavor_t flavor) {
  return slot(driver, flavor).family;
}

spn_ld_family_set_t spn_ld_families(spn_cc_driver_t driver, spn_ld_flavor_t flavor) {
  slot_t s = slot(driver, flavor);
  if (s.family == SPN_LD_FAMILY_NONE) {
    return 0;
  }
  return spn_ld_family_bit(s.family) | s.extras;
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

static bool any_declared(const spn_toolchain_linkers_t* linkers) {
  sp_for(flavor, SPN_LD_FLAVOR_COUNT) {
    if (linkers->families[flavor] != SPN_LD_FAMILY_NONE) {
      return true;
    }
  }
  return false;
}

static void push_issue(spn_ld_issues_t* issues, spn_ld_issue_t issue) {
  issues->items[issues->count++] = issue;
}

spn_ld_family_t spn_ld_family(const spn_toolchain_linkers_t* linkers, spn_triple_t target) {
  return linkers->families[spn_ld_flavor(target)];
}

bool spn_ld_links(const spn_toolchain_linkers_t* linkers, spn_triple_t target) {
  return spn_ld_family(linkers, target) != SPN_LD_FAMILY_NONE;
}

spn_ld_issues_t spn_ld_resolve(spn_cc_driver_t driver, const spn_toolchain_linkers_t* declared, spn_toolchain_linkers_t* linkers) {
  spn_ld_issues_t issues = sp_zero;
  sp_for(flavor, SPN_LD_FLAVOR_COUNT) {
    linkers->families[flavor] = spn_ld_family_default(driver, (spn_ld_flavor_t)flavor);
  }

  if (fixed(driver)) {
    if (any_declared(declared)) {
      push_issue(&issues, (spn_ld_issue_t) { .kind = SPN_LD_ISSUE_DECLARED });
    }
    return issues;
  }

  sp_for(flavor, SPN_LD_FLAVOR_COUNT) {
    spn_ld_family_t family = declared->families[flavor];
    if (family == SPN_LD_FAMILY_NONE) {
      continue;
    }
    if (!(spn_ld_families(driver, (spn_ld_flavor_t)flavor) & spn_ld_family_bit(family))) {
      push_issue(&issues, (spn_ld_issue_t) { .kind = SPN_LD_ISSUE_FORBIDDEN, .flavor = (spn_ld_flavor_t)flavor });
      continue;
    }
    linkers->families[flavor] = family;
  }
  return issues;
}
