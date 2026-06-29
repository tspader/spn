#ifndef SPN_TOOLCHAIN_TYPES_H
#define SPN_TOOLCHAIN_TYPES_H

#include "sp.h"
#include "spn.h"
#include "semver/types.h"

typedef enum {
  SPN_CC_DRIVER_NONE,
  SPN_CC_DRIVER_GCC,
  SPN_CC_DRIVER_CLANG,
  SPN_CC_DRIVER_MSVC,
} spn_cc_driver_t;

typedef enum {
  SPN_TOOLCHAIN_INLINE,
  SPN_TOOLCHAIN_INDEX,
  SPN_TOOLCHAIN_BUILTIN,
} spn_toolchain_kind_t;

typedef struct {
  sp_str_t package;
  spn_semver_range_t range;
} spn_toolchain_req_t;

typedef struct {
  sp_str_t program;
  sp_da(sp_str_t) args;
} spn_toolchain_launcher_t;

#define SPN_TOOLCHAIN_MAX_HOSTS 8
#define SPN_TOOLCHAIN_MAX_TARGETS 16

typedef struct spn_toolchain_info {
  sp_str_t name;
  sp_str_t version;
  sp_str_t url;
  sp_str_t sha;
  spn_toolchain_launcher_t compiler;
  spn_toolchain_launcher_t linker;
  spn_toolchain_launcher_t archiver;
  sp_str_t sysroot;
  spn_cc_driver_t driver;
  spn_triple_t hosts [8];
  spn_triple_t targets [16];
  bool export;
} spn_toolchain_info_t;

typedef struct spn_toolchain_entry {
  sp_str_t name;
  spn_toolchain_kind_t kind;
  union {
    spn_toolchain_info_t info;
    spn_toolchain_req_t request;
  };
} spn_toolchain_entry_t;

#endif
