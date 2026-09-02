#ifndef SPN_TOOLCHAIN_H
#define SPN_TOOLCHAIN_H

#include "paths/types.h"
#include "toolchain/types.h"
#include "toolchain/catalog.h"
#include "toolchain/driver.h"
#include "toolchain/select.h"
#include "toolchain/provision.h"

spn_toolchain_launcher_t spn_toolchain_launcher_with_root(sp_mem_t mem, spn_toolchain_launcher_t launcher, spn_path_t root);
sp_str_t                 spn_toolchain_launcher_to_str(const spn_path_roots_t* roots, sp_mem_t mem, spn_toolchain_launcher_t launcher);
bool                     spn_toolchain_has_cxx(spn_toolchain_info_t* toolchain);
sp_da(spn_triple_t)      spn_toolchain_targets(sp_mem_t mem, const spn_toolchain_info_t* toolchain, spn_triple_t host);

#endif
