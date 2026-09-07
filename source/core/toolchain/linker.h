#ifndef SPN_TOOLCHAIN_LINKER_H
#define SPN_TOOLCHAIN_LINKER_H

#include "common.gen.h"
#include "toolchain/types.h"

spn_ld_flavor_t spn_ld_flavor(spn_triple_t target);
bool            spn_ld_static(spn_ld_flavor_t flavor);
bool            spn_ld_scripts(spn_ld_family_t family, spn_ld_flavor_t flavor);
spn_ld_family_t spn_ld_family(const spn_toolchain_linkers_t* linkers, spn_triple_t target);
spn_ld_issues_t spn_ld_resolve(spn_cc_driver_t driver, const spn_cg_linkers_t* declared, spn_toolchain_linkers_t* linkers);

#endif
