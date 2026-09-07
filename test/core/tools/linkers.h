#ifndef SPN_TEST_LINKERS_H
#define SPN_TEST_LINKERS_H

#include "toolchain/types.h"

#define FAMILIES_NATIVE { \
  [SPN_LD_FLAVOR_ELF] = SPN_LD_FAMILY_GNU, \
  [SPN_LD_FLAVOR_MINGW] = SPN_LD_FAMILY_GNU, \
  [SPN_LD_FLAVOR_MSVC] = SPN_LD_FAMILY_MSVC, \
  [SPN_LD_FLAVOR_MACHO] = SPN_LD_FAMILY_LD64, \
  [SPN_LD_FLAVOR_WASM] = SPN_LD_FAMILY_LLD, \
}

#define FAMILIES_LLD { \
  [SPN_LD_FLAVOR_ELF] = SPN_LD_FAMILY_LLD, \
  [SPN_LD_FLAVOR_MINGW] = SPN_LD_FAMILY_LLD, \
  [SPN_LD_FLAVOR_MSVC] = SPN_LD_FAMILY_LLD, \
  [SPN_LD_FLAVOR_MACHO] = SPN_LD_FAMILY_LLD, \
  [SPN_LD_FLAVOR_WASM] = SPN_LD_FAMILY_LLD, \
}

static bool test_families_expected(const spn_ld_family_t* families) {
  sp_for(flavor, SPN_LD_FLAVOR_COUNT) {
    if (families[flavor]) {
      return true;
    }
  }
  return false;
}

#endif
