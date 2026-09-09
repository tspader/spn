#ifndef SMOKE_VARIANT_H
#define SMOKE_VARIANT_H

#include "sp.h"
#include "lanes.h"

typedef enum {
  DISTRO_DEBIAN,
  DISTRO_UBUNTU20,
  DISTRO_UBUNTU19,
  DISTRO_CENTOS7,
  DISTRO_ALPINE,
} distro_t;

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
  LANE_NONE,
  LANE_GCC,
  LANE_CLANG,
  LANE_LLVM,
  LANE_ZIG,
  LANE_GCC_LLD,
  LANE_AARCH64_GNU,
  LANE_MINGW_GNU,
  LANE_CLANG_MINGW,
  LANE_CLANG_MINGW_LLD,
  LANE_CLANG_BARE,
  LANE_MUSL_GCC,
  LANE_CLANG_MUSL,
  LANE_CLANG_WASI,
  LANE_WASI_SDK,
  LANE_WASI_SDK_LOCAL,
  LANE_CLANG_SYSROOT,
  LANE_CLANG_CROSS,
  LANE_CLANG_MSVC,
  LANE_CLANG_XWIN,
  LANE_ZIG_XWIN,
  LANE_ZIG_LOCAL,
  // LANE_WASI_SDK_ABS,
  LANE_ARM_GNU_LINUX,
  LANE_ARM_GNU_ELF,
  // LANE_ARM_GNU_SYSROOT,
  LANE_W64DEVKIT,
  LANE_CLANG64,
  LANE_OSXCROSS,
  LANE_COUNT,
} lane_t;

typedef enum {
  SYSROOT_NONE,
  SYSROOT_MUSL,
  SYSROOT_WASI,
  SYSROOT_ARM64,
  SYSROOT_WASI_SDK,
  SYSROOT_XWIN,
  SYSROOT_ZIG,
} sysroot_kind_t;

typedef enum {
  INSTALL_PACKAGES,
  INSTALL_LINKS,
  INSTALL_DEBS,
  INSTALL_ARTIFACT,
  INSTALL_XWIN,
} install_kind_t;

#define SMOKE_MAX_LANES 5
#define SMOKE_MAX_SYSROOTS 2
#define SMOKE_MAX_LINKS 2
#define SMOKE_MAX_DEBS 8
#define SMOKE_MAX_PACKAGES 4

typedef struct {
  const c8* dir;
  const c8* from;
} sysroot_link_t;

typedef struct {
  const c8* arch;
  const c8* names [SMOKE_MAX_DEBS];
} sysroot_debs_t;

typedef struct {
  const c8* arch;
  const c8* variant;
} sysroot_xwin_t;

typedef struct {
  const c8* name;
  const c8* path;
  const c8* packages [SMOKE_MAX_PACKAGES];
  install_kind_t kind;
  union {
    sysroot_link_t links [SMOKE_MAX_LINKS];
    sysroot_debs_t debs;
    lane_t artifact;
    sysroot_xwin_t xwin;
  };
} sysroot_t;

typedef struct {
  const c8* name;
  distro_t distro;
  u32 compilers;
  lane_t check;
  spn_kind_t spn;
  const c8* extra [SMOKE_MAX_PACKAGES];
  sysroot_kind_t sysroots [SMOKE_MAX_SYSROOTS];
  lane_t lanes [SMOKE_MAX_LANES];
} variant_t;

typedef enum {
  VERIFY_OK,
  VERIFY_LANE_UNDECLARED,
  VERIFY_LANE_UNLISTED,
  VERIFY_SYSROOT_UNPROVIDED,
} verify_kind_t;

typedef struct {
  verify_kind_t kind;
  const variant_t* variant;
  lane_t lane;
  sp_str_t name;
  sp_str_t sysroot;
} verify_t;

extern const variant_t variants [];
extern const u32 num_variants;
extern const sysroot_t sysroots [];

const c8*                 lane_name(lane_t lane);
lane_t                    lane_find(sp_str_t name);
const spn_cg_toolchain_t* lane_decl(lane_t lane, const spn_cg_toolchains_t* builtin, const spn_cg_toolchains_t* lanes);
verify_t                  lanes_verify(const spn_cg_toolchains_t* builtin, const spn_cg_toolchains_t* lanes);

sp_str_t                  sysroot_links_setup(sp_mem_t mem, const sysroot_t* sysroot);
sp_str_t                  sysroot_debs_setup(sp_mem_t mem, const sysroot_t* sysroot);

const variant_t*          variant_find(const c8* name);
const variant_t*          variant_hosting(lane_t lane);
bool                      variant_hosts(const variant_t* variant, lane_t lane);
lane_t                    variant_seed(const variant_t* variant);
sp_str_t                  variant_template(const variant_t* variant);
sp_str_t                  variant_packages(sp_mem_t mem, const variant_t* variant);
sp_str_t                  variant_summary(sp_mem_t mem, const variant_t* variant);
verify_t                  variant_verify(const variant_t* variant, const spn_cg_toolchains_t* builtin, const spn_cg_toolchains_t* lanes);

#endif
