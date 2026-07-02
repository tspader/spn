#include "toolchain/sha256.h"

typedef struct {
  u32 state [8];
  u64 length;
  u8 block [64];
  u32 fill;
} spn_sha256_ctx_t;

static const u32 spn_sha256_k [64] = {
  0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
  0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
  0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
  0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
  0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
  0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
  0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
  0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

#define SPN_SHA256_ROR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

static void spn_sha256_init(spn_sha256_ctx_t* ctx) {
  ctx->state[0] = 0x6a09e667;
  ctx->state[1] = 0xbb67ae85;
  ctx->state[2] = 0x3c6ef372;
  ctx->state[3] = 0xa54ff53a;
  ctx->state[4] = 0x510e527f;
  ctx->state[5] = 0x9b05688c;
  ctx->state[6] = 0x1f83d9ab;
  ctx->state[7] = 0x5be0cd19;
  ctx->length = 0;
  ctx->fill = 0;
}

static void spn_sha256_compress(spn_sha256_ctx_t* ctx, const u8* block) {
  u32 w [64];

  for (u32 i = 0; i < 16; i++) {
    w[i] = ((u32)block[i * 4] << 24) | ((u32)block[i * 4 + 1] << 16) | ((u32)block[i * 4 + 2] << 8) | (u32)block[i * 4 + 3];
  }
  for (u32 i = 16; i < 64; i++) {
    u32 s0 = SPN_SHA256_ROR(w[i - 15], 7) ^ SPN_SHA256_ROR(w[i - 15], 18) ^ (w[i - 15] >> 3);
    u32 s1 = SPN_SHA256_ROR(w[i - 2], 17) ^ SPN_SHA256_ROR(w[i - 2], 19) ^ (w[i - 2] >> 10);
    w[i] = w[i - 16] + s0 + w[i - 7] + s1;
  }

  u32 a = ctx->state[0];
  u32 b = ctx->state[1];
  u32 c = ctx->state[2];
  u32 d = ctx->state[3];
  u32 e = ctx->state[4];
  u32 f = ctx->state[5];
  u32 g = ctx->state[6];
  u32 h = ctx->state[7];

  for (u32 i = 0; i < 64; i++) {
    u32 s1 = SPN_SHA256_ROR(e, 6) ^ SPN_SHA256_ROR(e, 11) ^ SPN_SHA256_ROR(e, 25);
    u32 ch = (e & f) ^ (~e & g);
    u32 t1 = h + s1 + ch + spn_sha256_k[i] + w[i];
    u32 s0 = SPN_SHA256_ROR(a, 2) ^ SPN_SHA256_ROR(a, 13) ^ SPN_SHA256_ROR(a, 22);
    u32 maj = (a & b) ^ (a & c) ^ (b & c);
    u32 t2 = s0 + maj;

    h = g;
    g = f;
    f = e;
    e = d + t1;
    d = c;
    c = b;
    b = a;
    a = t1 + t2;
  }

  ctx->state[0] += a;
  ctx->state[1] += b;
  ctx->state[2] += c;
  ctx->state[3] += d;
  ctx->state[4] += e;
  ctx->state[5] += f;
  ctx->state[6] += g;
  ctx->state[7] += h;
}

static void spn_sha256_update(spn_sha256_ctx_t* ctx, const u8* data, u64 len) {
  ctx->length += len;

  while (len) {
    u32 take = (u32)sp_min(len, (u64)(64 - ctx->fill));
    sp_mem_copy(ctx->block + ctx->fill, data, take);
    ctx->fill += take;
    data += take;
    len -= take;

    if (ctx->fill == 64) {
      spn_sha256_compress(ctx, ctx->block);
      ctx->fill = 0;
    }
  }
}

static void spn_sha256_final(spn_sha256_ctx_t* ctx, u8 digest [32]) {
  u64 bits = ctx->length * 8;

  u8 pad = 0x80;
  spn_sha256_update(ctx, &pad, 1);
  ctx->length -= 1;

  u8 zero = 0;
  while (ctx->fill != 56) {
    spn_sha256_update(ctx, &zero, 1);
    ctx->length -= 1;
  }

  u8 tail [8];
  for (u32 i = 0; i < 8; i++) {
    tail[i] = (u8)(bits >> (56 - i * 8));
  }
  spn_sha256_update(ctx, tail, 8);

  for (u32 i = 0; i < 8; i++) {
    digest[i * 4] = (u8)(ctx->state[i] >> 24);
    digest[i * 4 + 1] = (u8)(ctx->state[i] >> 16);
    digest[i * 4 + 2] = (u8)(ctx->state[i] >> 8);
    digest[i * 4 + 3] = (u8)(ctx->state[i]);
  }
}

static sp_str_t spn_sha256_digest_to_hex(sp_mem_t mem, const u8 digest [32]) {
  static const c8 hex [] = "0123456789abcdef";
  c8* out = (c8*)sp_alloc(mem, 64);
  for (u32 i = 0; i < 32; i++) {
    out[i * 2] = hex[digest[i] >> 4];
    out[i * 2 + 1] = hex[digest[i] & 0xf];
  }
  return sp_str(out, 64);
}

void spn_sha256(const void* data, u64 len, u8 digest [32]) {
  spn_sha256_ctx_t ctx;
  spn_sha256_init(&ctx);
  spn_sha256_update(&ctx, (const u8*)data, len);
  spn_sha256_final(&ctx, digest);
}

sp_str_t spn_sha256_hex(sp_mem_t mem, const void* data, u64 len) {
  u8 digest [32];
  spn_sha256(data, len, digest);
  return spn_sha256_digest_to_hex(mem, digest);
}

spn_err_t spn_sha256_file(sp_mem_t mem, sp_str_t path, sp_str_t* hex) {
  sp_io_file_reader_t reader;
  if (sp_io_file_reader_from_path(&reader, path)) return SPN_ERROR;

  spn_sha256_ctx_t ctx;
  spn_sha256_init(&ctx);

  u8 chunk [65536];
  while (true) {
    u64 bytes_read = 0;
    sp_err_t err = sp_io_read(&reader.base, chunk, sizeof(chunk), &bytes_read);
    if (bytes_read) spn_sha256_update(&ctx, chunk, bytes_read);
    if (err == SP_ERR_IO_EOF) break;
    if (err) {
      sp_io_file_reader_close(&reader);
      return SPN_ERROR;
    }
    if (!bytes_read) break;
  }

  sp_io_file_reader_close(&reader);

  u8 digest [32];
  spn_sha256_final(&ctx, digest);
  *hex = spn_sha256_digest_to_hex(mem, digest);
  return SPN_OK;
}
