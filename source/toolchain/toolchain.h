#ifndef SPN_TOOLCHAIN_H
#define SPN_TOOLCHAIN_H

#include "toolchain/types.h"
#include "toolchain/catalog.h"
#include "toolchain/select.h"
#include "toolchain/provision.h"

spn_toolchain_launcher_t spn_toolchain_launcher_with_root(sp_mem_t mem, spn_toolchain_launcher_t launcher, sp_str_t root);
sp_str_t                 spn_toolchain_launcher_to_str(sp_mem_t mem, spn_toolchain_launcher_t launcher);
bool                     spn_toolchain_has_cxx(spn_toolchain_t* toolchain);

#endif
