#ifndef SPN_SHA256_TYPES_H
#define SPN_SHA256_TYPES_H

#include "sp.h"

typedef struct {
  u32 state [8];
  u64 length;
  u8 block [64];
  u32 fill;
} spn_sha256_ctx_t;

#endif
