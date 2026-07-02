#ifndef SPN_TOOLCHAIN_SHA256_H
#define SPN_TOOLCHAIN_SHA256_H

#include "sp.h"
#include "spn.h"

void      spn_sha256(const void* data, u64 len, u8 digest[32]);
sp_str_t  spn_sha256_hex(sp_mem_t mem, const void* data, u64 len);
spn_err_t spn_sha256_file(sp_mem_t mem, sp_str_t path, sp_str_t* hex);

#endif
