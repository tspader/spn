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

typedef sp_opt(spn_artifact_t) spn_opt_artifact_t;

typedef struct {
  spn_triple_t triple;
  spn_artifact_t artifact;
} spn_toolchain_host_t;

typedef struct {
  sp_str_t name;
  sp_str_t version;
  spn_cc_driver_t driver;
  spn_toolchain_launcher_t compiler;
  spn_toolchain_launcher_t cxx;
  spn_toolchain_launcher_t linker;
  spn_toolchain_launcher_t archiver;
  sp_da(spn_triple_t) targets;
  spn_opt_artifact_t artifact;
} spn_toolchain_info_t;

struct spn_toolchain_catalog_t {
  sp_str_ht(spn_toolchain_info_t*) entries;
  sp_mem_t mem;
};

typedef spn_err_t (*spn_fetch_fn)(sp_str_t url, sp_str_t dest, void* user_data);

typedef struct {
  sp_mem_t mem;
  sp_str_t dir;
  sp_str_t mirror;
  spn_fetch_fn fetch;
  void* fetch_user_data;
} spn_toolchain_store_t;

#endif
