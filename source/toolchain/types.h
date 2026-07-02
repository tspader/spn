#ifndef SPN_TOOLCHAIN_TYPES_H
#define SPN_TOOLCHAIN_TYPES_H

#include "sp.h"
#include "spn.h"

typedef enum {
  SPN_CC_DRIVER_NONE,
  SPN_CC_DRIVER_GCC,
  SPN_CC_DRIVER_CLANG,
  SPN_CC_DRIVER_MSVC,
} spn_cc_driver_t;

typedef struct {
  sp_str_t program;
  sp_da(sp_str_t) args;
} spn_toolchain_launcher_t;

typedef struct {
  sp_str_t url;
  sp_str_t sha256;
  sp_str_t mirrors;
} spn_artifact_t;

typedef struct spn_toolchain {
  sp_str_t name;
  sp_str_t version;
  spn_cc_driver_t driver;
  spn_toolchain_launcher_t compiler;
  spn_toolchain_launcher_t linker;
  spn_toolchain_launcher_t archiver;
  sp_da(spn_triple_t) targets;
  sp_opt(spn_artifact_t) artifact;
} spn_toolchain_t;

typedef sp_str_ht(spn_toolchain_t) spn_toolchain_catalog_t;

#endif
