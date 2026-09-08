#ifndef SPN_TOOLCHAIN_LIBC_H
#define SPN_TOOLCHAIN_LIBC_H

#include "sp.h"
#include "paths/types.h"
#include "toolchain/types.h"

void       spn_sdk_render_libc(sp_io_writer_t* io, const spn_path_roots_t* roots, const spn_sdk_t* sdk);
spn_path_t spn_sdk_libc_path(sp_mem_t mem, const spn_path_roots_t* roots, const spn_sdk_t* sdk);
spn_err_t  spn_sdk_libc_write(sp_mem_t mem, const spn_path_roots_t* roots, const spn_sdk_t* sdk);

#endif
