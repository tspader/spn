#ifndef SPN_TOOLCHAIN_TYPES_H
#define SPN_TOOLCHAIN_TYPES_H

#include "sp.h"
#include "spn/core.h"

#include "core/types.h"
#include "paths/types.h"
#include "sp_om/sp_om.h"

typedef enum {
  SPN_CC_CAP_TARGET_TRIPLE  = 1 << 0,
  SPN_CC_CAP_CLANG_FRONTEND = 1 << 1,
  SPN_CC_CAP_NOLIBC         = 1 << 2,
  SPN_CC_CAP_LLVM_TRIPLE    = 1 << 3,
} spn_cc_cap_t;

typedef u32 spn_cc_cap_set_t;

typedef enum {
  SPN_LD_FLAVOR_ELF,
  SPN_LD_FLAVOR_MINGW,
  SPN_LD_FLAVOR_MSVC,
  SPN_LD_FLAVOR_MACHO,
  SPN_LD_FLAVOR_WASM,
  SPN_LD_FLAVOR_COUNT,
} spn_ld_flavor_t;

#define spn_ld_flavor_bit(flavor) (1u << (flavor))

typedef u32 spn_ld_flavor_set_t;

typedef enum {
  SPN_LD_FAMILY_NONE,
  SPN_LD_FAMILY_GNU,
  SPN_LD_FAMILY_LLD,
  SPN_LD_FAMILY_LD64,
  SPN_LD_FAMILY_MSVC,
} spn_ld_family_t;

#define spn_ld_family_bit(family) (1u << (family))

typedef u32 spn_ld_family_set_t;

typedef enum {
  SPN_LD_CAP_SCRIPT       = 1 << 0,
  SPN_LD_CAP_EXCLUDE_LIBS = 1 << 1,
} spn_ld_cap_t;

typedef u32 spn_ld_cap_set_t;

typedef enum {
  SPN_LD_ARG_NONE,
  SPN_LD_ARG_LD_PATH,
  SPN_LD_ARG_FUSE_LLD,
} spn_ld_arg_t;

typedef enum {
  SPN_LD_CHECK_OK,
  SPN_LD_CHECK_FAMILY_MISSING,
  SPN_LD_CHECK_FAMILY_FORBIDDEN,
  SPN_LD_CHECK_PROGRAM_MISSING,
  SPN_LD_CHECK_PROGRAM_FORBIDDEN,
} spn_ld_check_t;

typedef struct {
  spn_ld_family_t family;
  spn_arg_t program;
} spn_toolchain_linker_t;

typedef struct {
  spn_toolchain_linker_t slots [SPN_LD_FLAVOR_COUNT];
} spn_toolchain_linkers_t;

typedef enum {
  SPN_LD_ISSUE_DECLARED,
  SPN_LD_ISSUE_UNDECLARED,
  SPN_LD_ISSUE_SLOT,
} spn_ld_issue_kind_t;

typedef struct {
  spn_ld_issue_kind_t kind;
  spn_ld_flavor_t flavor;
  spn_ld_check_t check;
} spn_ld_issue_t;

typedef struct {
  spn_ld_issue_t items [SPN_LD_FLAVOR_COUNT];
  u32 count;
} spn_ld_issues_t;

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
  SPN_TOOLCHAIN_SOURCE_LOCAL,
  SPN_TOOLCHAIN_SOURCE_DISTRIBUTION,
  SPN_TOOLCHAIN_SOURCE_MIXED,
} spn_toolchain_source_t;

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
  spn_toolchain_launcher_t archiver;
  spn_toolchain_linkers_t linkers;
  spn_toolchain_source_t source;
  sp_da(spn_toolchain_host_t) hosts;
  sp_da(spn_triple_t) targets;
} spn_toolchain_decl_t;

typedef struct {
  sp_str_t name;
  sp_str_t version;
  spn_cc_driver_t driver;
  spn_toolchain_launcher_t compiler;
  spn_toolchain_launcher_t cxx;
  spn_toolchain_launcher_t archiver;
  spn_toolchain_linkers_t linkers;
  sp_da(spn_triple_t) targets;
  spn_toolchain_support_t support;
} spn_toolchain_info_t;

// Entries preserve declaration order; auto-selection takes the first match.
struct spn_toolchain_catalog_t {
  sp_mem_t mem;
  spn_triple_t host;
  sp_str_om(spn_toolchain_info_t) entries;
};

typedef struct {
  spn_abi_t items [SPN_ABI_COUNT];
  u32 count;
} spn_abi_list_t;

typedef enum {
  SPN_TOOLCHAIN_REF_NONE,
  SPN_TOOLCHAIN_REF_AUTO,
  SPN_TOOLCHAIN_REF_NAMED,
} spn_toolchain_ref_kind_t;

typedef struct {
  spn_toolchain_ref_kind_t kind;
  sp_str_t name;
} spn_toolchain_ref_t;

typedef struct {
  spn_toolchain_ref_t toolchain;
  spn_triple_t target;
  spn_abi_list_t abis;
} spn_toolchain_query_t;

typedef struct {
  spn_toolchain_info_t* toolchain;
  spn_triple_t triple;
} spn_toolchain_selection_t;

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
