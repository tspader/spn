#ifndef SPN_TOOLCHAIN_SHA256_H
#define SPN_TOOLCHAIN_SHA256_H

#include "sp.h"
#include "spn.h"

typedef struct {
  u32 state [8];
  u64 length;
  u8 block [64];
  u32 fill;
} spn_sha256_ctx_t;

void      spn_sha256(const void* data, u64 len, u8 digest[32]);
sp_str_t  spn_sha256_hex(sp_mem_t mem, const void* data, u64 len);
spn_err_t spn_sha256_file(sp_mem_t mem, sp_str_t path, sp_str_t* hex);
void      spn_sha256_init(spn_sha256_ctx_t* ctx);
void      spn_sha256_update(spn_sha256_ctx_t* ctx, const u8* data, u64 len);
void      spn_sha256_final(spn_sha256_ctx_t* ctx, u8 digest [32]);

#endif
