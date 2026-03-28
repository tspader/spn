#ifndef SPN_TOOLCHAIN_H
#define SPN_TOOLCHAIN_H

#include "toolchain/types.h"

sp_str_t spn_toolchain_resolve_path(spn_toolchain_info_t* toolchain, sp_str_t path);
sp_str_t spn_toolchain_get_linker_driver(spn_toolchain_info_t* toolchain);

#endif
