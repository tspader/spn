#ifndef SPN_TOOLCHAIN_TYPES_H
#define SPN_TOOLCHAIN_TYPES_H

#include "sp.h"

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

struct spn_toolchain {
  sp_str_t name;
  sp_str_t compiler;
  sp_str_t linker;
  sp_str_t archiver;
  sp_str_t sysroot;
  spn_cc_driver_t driver;
  spn_abi_t abi;
};

#endif
