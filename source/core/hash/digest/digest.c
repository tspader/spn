#include "hash/digest/digest.h"

#include "hash/sha256/sha256.h"

void spn_digest_init_blake3(spn_digest_ctx_t* ctx) {
  ctx->kind = SPN_DIGEST_BLAKE3;
  blake3_hasher_init(&ctx->blake3);
}

void spn_digest_init_sha256(spn_digest_ctx_t* ctx) {
  ctx->kind = SPN_DIGEST_SHA256;
  spn_sha256_init(&ctx->sha256);
}

void spn_digest_update(spn_digest_ctx_t* ctx, const void* data, u64 len) {
  switch (ctx->kind) {
    case SPN_DIGEST_BLAKE3: {
      blake3_hasher_update(&ctx->blake3, data, len);
      break;
    }
    case SPN_DIGEST_SHA256: {
      spn_sha256_update(&ctx->sha256, (const u8*)data, len);
      break;
    }
  }
}

void spn_digest_final(spn_digest_ctx_t* ctx, u8 digest [32]) {
  switch (ctx->kind) {
    case SPN_DIGEST_BLAKE3: {
      blake3_hasher_finalize(&ctx->blake3, digest, 32);
      break;
    }
    case SPN_DIGEST_SHA256: {
      spn_sha256_final(&ctx->sha256, digest);
      break;
    }
  }
}

static void init_kind(spn_digest_ctx_t* ctx, spn_digest_kind_t kind) {
  switch (kind) {
    case SPN_DIGEST_BLAKE3: {
      spn_digest_init_blake3(ctx);
      break;
    }
    case SPN_DIGEST_SHA256: {
      spn_digest_init_sha256(ctx);
      break;
    }
  }
}

void spn_digest(spn_digest_kind_t kind, const void* data, u64 len, u8 digest [32]) {
  spn_digest_ctx_t ctx = sp_zero;
  init_kind(&ctx, kind);
  spn_digest_update(&ctx, data, len);
  spn_digest_final(&ctx, digest);
}

sp_str_t spn_digest_hex(sp_mem_t mem, const u8 digest [32]) {
  static const c8 hex [] = "0123456789abcdef";
  c8* buffer = (c8*)sp_alloc(mem, 64);
  sp_for(it, 32) {
    buffer[it * 2] = hex[digest[it] >> 4];
    buffer[it * 2 + 1] = hex[digest[it] & 0xf];
  }
  return sp_str(buffer, 64);
}

spn_err_t spn_digest_file(spn_digest_kind_t kind, sp_str_t path, u8 digest [32], u64* size) {
  sp_io_file_reader_t reader = sp_zero;
  if (sp_io_file_reader_from_path(&reader, path)) {
    return SPN_ERROR;
  }

  spn_digest_ctx_t ctx = sp_zero;
  init_kind(&ctx, kind);

  *size = 0;
  u8 chunk [65536];
  while (true) {
    u64 bytes_read = 0;
    sp_err_t err = sp_io_read(&reader.base, chunk, sizeof(chunk), &bytes_read);
    if (bytes_read) {
      spn_digest_update(&ctx, chunk, bytes_read);
      *size += bytes_read;
    }
    if (err == SP_ERR_IO_EOF) {
      break;
    }
    if (err) {
      sp_io_file_reader_close(&reader);
      return SPN_ERROR;
    }
    if (!bytes_read) {
      break;
    }
  }

  sp_io_file_reader_close(&reader);
  spn_digest_final(&ctx, digest);
  return SPN_OK;
}

spn_err_t spn_digest_file_hex(spn_digest_kind_t kind, sp_mem_t mem, sp_str_t path, sp_str_t* hex) {
  u8 digest [32];
  u64 size = 0;
  if (spn_digest_file(kind, path, digest, &size)) {
    return SPN_ERROR;
  }
  *hex = spn_digest_hex(mem, digest);
  return SPN_OK;
}

sp_hash_t spn_digest_hash(const void* data, u64 len) {
  spn_digest_ctx_t ctx = sp_zero;
  spn_digest_init_blake3(&ctx);
  spn_digest_update(&ctx, data, len);
  u8 bytes [sizeof(sp_hash_t)] = sp_zero;
  blake3_hasher_finalize(&ctx.blake3, bytes, sizeof(bytes));
  sp_hash_t hash = sp_zero;
  sp_mem_copy(&hash, bytes, sizeof(bytes));
  return hash;
}

sp_hash_t spn_digest_hash_str(sp_str_t str) {
  return spn_digest_hash(str.data, str.len);
}

sp_hash_t spn_digest_hash_combine(const sp_hash_t* parts, u64 count) {
  return spn_digest_hash(parts, count * sizeof(sp_hash_t));
}
