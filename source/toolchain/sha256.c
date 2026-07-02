#include "toolchain/sha256.h"

void spn_sha256(const void* data, u64 len, u8 digest[32]) {
  sp_mem_zero(digest, 32);
}

sp_str_t spn_sha256_hex(sp_mem_t mem, const void* data, u64 len) {
  return sp_str_lit("");
}

spn_err_t spn_sha256_file(sp_mem_t mem, sp_str_t path, sp_str_t* hex) {
  *hex = sp_str_lit("");
  return SPN_ERROR;
}
