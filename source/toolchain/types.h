#ifndef SPN_TOOLCHAIN_TYPES_H
#define SPN_TOOLCHAIN_TYPES_H

#include "sp.h"
#include "spn.h"

#include "forward/types.h"

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
  sp_str_t mirror_list;
} spn_artifact_t;

typedef sp_opt(spn_artifact_t) sp_opt_spn_artifact_t;

typedef struct {
  spn_triple_t triple;
  spn_artifact_t artifact;
} spn_toolchain_host_t;

typedef struct spn_toolchain {
  sp_str_t name;
  sp_str_t version;
  spn_cc_driver_t driver;
  spn_toolchain_launcher_t compiler;
  spn_toolchain_launcher_t linker;
  spn_toolchain_launcher_t archiver;
  sp_da(spn_triple_t) targets;
  sp_opt_spn_artifact_t artifact;
} spn_toolchain_t;

struct spn_toolchain_catalog_t {
  sp_str_ht(spn_toolchain_t*) entries;
  sp_mem_t mem;
};

#endif
