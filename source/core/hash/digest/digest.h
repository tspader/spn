#ifndef SPN_HASH_DIGEST_DIGEST_H
#define SPN_HASH_DIGEST_DIGEST_H

#include "sp.h"
#include "spn/core.h"
#include "blake3.h"
#include "hash/sha256/sha256.h"

typedef enum {
  SPN_DIGEST_BLAKE3,
  SPN_DIGEST_SHA256,
} spn_digest_kind_t;

typedef struct {
  spn_digest_kind_t kind;
  union {
    blake3_hasher blake3;
    spn_sha256_ctx_t sha256;
  };
} spn_digest_ctx_t;

void      spn_digest_init_blake3(spn_digest_ctx_t* ctx);
void      spn_digest_init_sha256(spn_digest_ctx_t* ctx);
void      spn_digest_update(spn_digest_ctx_t* ctx, const void* data, u64 len);
void      spn_digest_final(spn_digest_ctx_t* ctx, u8 digest [32]);
void      spn_digest(spn_digest_kind_t kind, const void* data, u64 len, u8 digest [32]);
sp_str_t  spn_digest_hex(sp_mem_t mem, const u8 digest [32]);
spn_err_t spn_digest_file(spn_digest_kind_t kind, sp_str_t path, u8 digest [32], u64* size);
spn_err_t spn_digest_file_hex(spn_digest_kind_t kind, sp_mem_t mem, sp_str_t path, sp_str_t* hex);
sp_hash_t spn_digest_hash(const void* data, u64 len);
sp_hash_t spn_digest_hash_str(sp_str_t str);
sp_hash_t spn_digest_hash_combine(const sp_hash_t* parts, u64 count);

#endif
