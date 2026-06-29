#ifndef SPN_TOOLCHAIN_H
#define SPN_TOOLCHAIN_H

#include "toolchain/types.h"

sp_da(spn_toolchain_entry_t) spn_toolchain_load_builtins(spn_triple_t host, sp_mem_t mem);
sp_str_t                 spn_toolchain_resolve_path(spn_toolchain_info_t* toolchain, sp_str_t path);
spn_toolchain_launcher_t spn_toolchain_get_linker_driver(spn_toolchain_info_t* toolchain);
spn_toolchain_launcher_t spn_toolchain_launcher_with_root(spn_toolchain_launcher_t launcher, sp_str_t root);
sp_str_t                 spn_toolchain_launcher_to_str(spn_toolchain_launcher_t launcher);

#endif
