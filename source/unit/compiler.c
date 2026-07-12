#include "unit/compiler.h"

spn_cc_toolchain_t spn_toolchain_unit_compiler(const spn_toolchain_unit_t* unit) {
  return (spn_cc_toolchain_t) {
    .name = unit->info->name,
    .driver = unit->info->driver,
    .compiler = unit->compiler,
    .cxx = unit->cxx,
    .linker = unit->linker,
    .archiver = unit->archiver,
    .archiver_driver = unit->info->driver == SPN_CC_DRIVER_MSVC ? SPN_AR_DRIVER_MSVC : SPN_AR_DRIVER_GNU,
  };
}
