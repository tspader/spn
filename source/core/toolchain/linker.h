#ifndef SPN_TOOLCHAIN_LINKER_H
#define SPN_TOOLCHAIN_LINKER_H

#include "toolchain/types.h"

spn_ld_dialect_t spn_ld_dialect(spn_triple_t target);
bool             spn_ld_static(spn_ld_dialect_t dialect);
bool             spn_ld_scripts(spn_ld_family_t family, spn_format_t format);
bool             spn_ld_accepts(spn_cc_driver_t driver, spn_ld_family_t declared);
spn_ld_family_t  spn_ld_family(spn_cc_driver_t driver, spn_ld_family_t declared, spn_triple_t target);

#endif
