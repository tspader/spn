#ifndef SMOKE_VARIANT_H
#define SMOKE_VARIANT_H

#include "sp.h"

typedef enum {
  DISTRO_DEBIAN,
  DISTRO_UBUNTU20,
  DISTRO_UBUNTU19,
  DISTRO_CENTOS7,
  DISTRO_ALPINE,
} distro_kind_t;

typedef enum {
  SPN_MUSL,
  SPN_GNU,
} spn_kind_t;

typedef enum {
  COMPILER_GCC = 1 << 0,
  COMPILER_GXX = 1 << 1,
  COMPILER_CLANG = 1 << 2,
} compiler_t;

typedef enum {
  TOOLCHAIN_NONE,
  TOOLCHAIN_GCC,
  TOOLCHAIN_CLANG,
  TOOLCHAIN_ZIG,
} toolchain_t;

#define SMOKE_MAX_LANES 4

typedef struct {
  const c8* name;
  distro_kind_t distro;
  u32 installed;
  toolchain_t toolchain;
  spn_kind_t spn;
  const c8* extra;
  const c8* lanes [SMOKE_MAX_LANES];
} variant_t;

extern const variant_t variants [];
extern const u32 num_variants;

const variant_t* variant_find(const c8* name);
const variant_t* variant_hosting(const c8* lane);
bool             variant_hosts(const variant_t* variant, const c8* lane);
sp_str_t get_template_name(const variant_t* variant);
const c8* toolchain_name(toolchain_t toolchain);
sp_str_t get_variant_packages(sp_mem_t mem, const variant_t* variant);
sp_str_t variant_summary(sp_mem_t mem, const variant_t* variant);

#endif
