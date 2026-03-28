#ifndef SPN_TOOLCHAIN_TYPES_H
#define SPN_TOOLCHAIN_TYPES_H

#include "sp.h"
#include "semver/types.h"

typedef enum {
  SPN_CC_DRIVER_NONE,
  SPN_CC_DRIVER_GCC,
  SPN_CC_DRIVER_MSVC,
} spn_cc_driver_t;

typedef enum {
  SPN_ABI_NONE,
  SPN_ABI_GNU,
  SPN_ABI_MUSL,
  SPN_ABI_MSVC
} spn_abi_t;

typedef struct spn_toolchain_info {
  sp_str_t name;
  sp_str_t url;
  sp_str_t compiler;
  sp_str_t linker;
  sp_str_t archiver;
  sp_str_t sysroot;
  spn_cc_driver_t driver;
  spn_abi_t abi;
  bool export;
  sp_str_t package;
  spn_semver_range_t version;
} spn_toolchain_info_t;

typedef struct spn_toolchain {
  spn_toolchain_info_t* info;
  sp_str_t root;
  sp_str_t compiler;
  sp_str_t linker;
  sp_str_t archiver;
} spn_toolchain_t;

#endif
