#include "toolchain/sdk.h"

#include "ctx/types.h"
#include "enum/enum.h"
#include "error/error.h"
#include "hash/digest/digest.h"
#include "paths/paths.h"
#include "triple/triple.h"

spn_sdk_kind_t spn_sdk_kind(spn_triple_t target) {
  switch (target.os) {
    case SPN_OS_MACOS: return SPN_SDK_MACOS;
    case SPN_OS_WINDOWS: return target.abi == SPN_ABI_MSVC ? SPN_SDK_MSVC : SPN_SDK_SYSROOT;
    case SPN_OS_LINUX:
    case SPN_OS_WASI: return SPN_SDK_SYSROOT;
    case SPN_OS_FREESTANDING: return SPN_SDK_NONE;
    case SPN_OS_NONE: sp_unreachable_case();
  }
  SP_UNREACHABLE_RETURN(SPN_SDK_NONE);
}

bool spn_sdk_declarable(spn_sdk_kind_t kind) {
  switch (kind) {
    case SPN_SDK_SYSROOT:
    case SPN_SDK_MACOS:
    case SPN_SDK_MSVC: return true;
    case SPN_SDK_NONE: return false;
  }
  SP_UNREACHABLE_RETURN(false);
}

bool spn_sdk_libc(spn_sdk_kind_t kind) {
  switch (kind) {
    case SPN_SDK_MACOS:
    case SPN_SDK_MSVC: return true;
    case SPN_SDK_NONE:
    case SPN_SDK_SYSROOT: return false;
  }
  SP_UNREACHABLE_RETURN(false);
}

spn_sdk_t spn_sdk_from_root(sp_mem_t mem, spn_sdk_kind_t kind, spn_path_t root, spn_arch_t arch) {
  return sp_zero_struct(spn_sdk_t);
}

spn_sdk_t spn_sdk_from_msvc(sp_mem_t mem, const sp_msvc_sdk_t* kits, const sp_msvc_vs_t* vs, spn_arch_t arch) {
  return sp_zero_struct(spn_sdk_t);
}

bool spn_sdk_serves(const spn_sdk_t* sdk, spn_triple_t target) {
  return false;
}

const spn_sdk_t* spn_sdk_find(sp_da(spn_sdk_t) sdks, spn_triple_t target) {
  return SP_NULLPTR;
}

sp_da(spn_sdk_t) spn_sdk_detect(sp_mem_t mem, sp_env_t* env, spn_triple_t host) {
  return sp_da_new(mem, spn_sdk_t);
}

spn_sdk_t spn_sdk_resolve(sp_mem_t mem, sp_da(spn_sdk_t) sdks, const spn_toolchain_selection_t* selection) {
  return sp_zero_struct(spn_sdk_t);
}

sp_hash_t spn_sdk_hash(const spn_sdk_t* sdk) {
  return 0;
}

void spn_sdk_render_libc(sp_io_writer_t* io, const spn_path_roots_t* roots, const spn_sdk_t* sdk) {
}

spn_path_t spn_sdk_libc_path(sp_mem_t mem, const spn_path_roots_t* roots, const spn_sdk_t* sdk) {
  return sp_zero_struct(spn_path_t);
}

spn_err_t spn_sdk_libc_write(sp_mem_t mem, const spn_path_roots_t* roots, const spn_sdk_t* sdk) {
  return SPN_OK;
}
