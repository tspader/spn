#ifndef SPN_TOOLCHAIN_LIBC_H
#define SPN_TOOLCHAIN_LIBC_H

#include "sp.h"
#include "paths/types.h"
#include "toolchain/types.h"

spn_err_t spn_sdk_to_libc(sp_mem_t mem, const spn_path_roots_t* roots, spn_sdk_t* sdk);

#endif
