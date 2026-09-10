#ifndef WINVM_VARIANT_H
#define WINVM_VARIANT_H

#include "sp.h"

#define WINVM_MAX_STEPS 6

typedef struct {
  const c8* recipe;
  const c8* arg;
} winvm_step_t;

typedef struct {
  const c8* name;
  const c8* summary;
  u32 memory_mb;
  u32 vcpus;
  u8 octet;
  winvm_step_t steps[WINVM_MAX_STEPS];
} winvm_variant_t;

extern const winvm_variant_t winvm_variants[];
extern const u32 winvm_num_variants;

const winvm_variant_t* winvm_variant_find(const c8* name);
sp_str_t winvm_variant_summary(sp_mem_t mem, const winvm_variant_t* variant);

#endif
