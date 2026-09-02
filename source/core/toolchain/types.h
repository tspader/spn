#ifndef SPN_TOOLCHAIN_TYPES_H
#define SPN_TOOLCHAIN_TYPES_H

#include "sp.h"
#include "spn/core.h"

#include "core/types.h"
#include "paths/types.h"
#include "sp_om/sp_om.h"

typedef enum {
  SPN_CC_DRIVER_NONE,
  SPN_CC_DRIVER_GCC,
  SPN_CC_DRIVER_CLANG,
  SPN_CC_DRIVER_MSVC,
  SPN_CC_DRIVER_ZIG,
} spn_cc_driver_t;

typedef enum {
  SPN_CC_CAP_TARGET_TRIPLE  = 1 << 0,
  SPN_CC_CAP_CLANG_FRONTEND = 1 << 1,
  SPN_CC_CAP_EXCLUDE_LIBS   = 1 << 2,
  SPN_CC_CAP_NOLIBC         = 1 << 3,
  SPN_CC_CAP_FREESTANDING   = 1 << 4,
  SPN_CC_CAP_LLVM_TRIPLE    = 1 << 5,
} spn_cc_cap_t;

typedef u32 spn_cc_cap_set_t;

typedef enum {
  SPN_TOOLCHAIN_ROLE_BUILD,
  SPN_TOOLCHAIN_ROLE_SCRIPT,
} spn_toolchain_role_t;

typedef struct {
  spn_arg_t program;
  sp_da(sp_str_t) args;
} spn_toolchain_launcher_t;

typedef struct {
  sp_str_t url;
  sp_str_t sha256;
  sp_str_t mirror_list;
} spn_artifact_t;

typedef struct {
  spn_triple_t triple;
  spn_artifact_t artifact;
} spn_toolchain_host_t;

typedef enum {
  SPN_TOOLCHAIN_SUPPORT_NONE,
  SPN_TOOLCHAIN_SUPPORT_LOCAL,
  SPN_TOOLCHAIN_SUPPORT_ARTIFACT,
} spn_toolchain_support_kind_t;

typedef struct {
  spn_toolchain_support_kind_t kind;
  spn_artifact_t artifact;
} spn_toolchain_support_t;

typedef struct {
  sp_str_t name;
  sp_str_t version;
  spn_cc_driver_t driver;
  spn_toolchain_launcher_t compiler;
  spn_toolchain_launcher_t cxx;
  spn_toolchain_launcher_t linker;
  spn_toolchain_launcher_t archiver;
  sp_da(spn_toolchain_host_t) hosts;
  sp_da(spn_triple_t) targets;
  spn_toolchain_support_t support;
} spn_toolchain_info_t;

// Entries preserve declaration order; auto-selection takes the first match.
struct spn_toolchain_catalog_t {
  sp_mem_t mem;
  spn_triple_t host;
  sp_str_om(spn_toolchain_info_t) entries;
};

typedef spn_err_t (*spn_fetch_fn)(sp_str_t url, sp_str_t dest, void* user_data);

typedef struct {
  sp_str_t path;
  u64 size;
  sp_tm_epoch_t mtime;
  sp_hash_t hash;
} spn_probe_entry_t;

typedef struct {
  sp_mem_t mem;
  sp_str_t file;
  sp_str_om(spn_probe_entry_t) entries;
} spn_probe_cache_t;

typedef struct {
  sp_mem_t mem;
  sp_str_t dir;
  sp_str_t mirror;
  spn_fetch_fn fetch;
  void* fetch_user_data;
  spn_probe_cache_t probes;
} spn_toolchain_store_t;

#endif
