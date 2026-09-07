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

typedef enum {
  SYSROOT_NONE,
  SYSROOT_MUSL,
  SYSROOT_WASI,
  SYSROOT_ARM64,
} sysroot_kind_t;

#define SMOKE_MAX_LANES 5
#define SMOKE_MAX_SYSROOTS 2
#define SMOKE_MAX_LINKS 2
#define SMOKE_MAX_DEBS 8

typedef struct {
  const c8* dir;
  const c8* from;
} sysroot_link_t;

typedef struct {
  const c8* name;
  const c8* path;
  const c8* packages;
  const c8* arch;
  const c8* debs [SMOKE_MAX_DEBS];
  sysroot_link_t links [SMOKE_MAX_LINKS];
} sysroot_t;

typedef struct {
  const c8* name;
  distro_kind_t distro;
  u32 installed;
  toolchain_t toolchain;
  spn_kind_t spn;
  const c8* extra;
  sysroot_kind_t sysroots [SMOKE_MAX_SYSROOTS];
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
sp_str_t get_variant_setup(sp_mem_t mem, const variant_t* variant);
sp_str_t variant_summary(sp_mem_t mem, const variant_t* variant);

#endif
