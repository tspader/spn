#ifndef SPN_PKG_TYPES_H
#define SPN_PKG_TYPES_H

#include "sp.h"

#include "core/types.h"
#include "git/types.h"
#include "intern/types.h"
#include "paths/types.h"
#include "macro/macro.h"
#include "sp_om/sp_om.h"
#include "semver/types.h"
#include "target/types.h"
#include "toolchain/types.h"
#include "when/types.h"

typedef enum {
  SPN_PKG_SOURCE_ROOT,
  SPN_PKG_SOURCE_FILE,
  SPN_PKG_SOURCE_INDEX,
} spn_pkg_source_t;

typedef enum {
  SPN_MANIFEST_ROOT,
  SPN_MANIFEST_DEP,
} spn_manifest_role_t;

typedef enum {
  SPN_PKG_ROOT_NONE,
  SPN_PKG_ROOT_LOCAL,
  SPN_PKG_ROOT_GIT,
} spn_pkg_root_kind_t;

typedef struct {
  spn_pkg_root_kind_t kind;
  union {
    sp_str_t local;
    spn_git_checkout_id_t git;
  };
} spn_pkg_root_t;


typedef struct {
  sp_str_t namespace;
  sp_str_t name;
} spn_pkg_name_t;

SPN_PACK_PUSH
typedef struct {
  sp_intern_id_t qualified;
  sp_hash_t hash;
  spn_semver_t version;
} spn_pkg_id_t;
SPN_PACK_POP

_Static_assert(
  sizeof(spn_pkg_id_t) == sizeof(sp_intern_id_t) + sizeof(sp_hash_t) + sizeof(spn_semver_t),
  "spn_pkg_id_t is byte-hashed as a key; it must have no padding"
);

typedef struct {
  sp_intern_str_t qualified;
  spn_pkg_source_t source;
  spn_dep_kind_t kind;
  bool private;
  spn_when_t when;
  spn_when_t options;
  union {
    struct { spn_semver_range_t range; } index;
    struct { sp_str_t path; } file;
  };
} spn_requested_dep_t;


typedef struct {
  sp_str_t consumer;
  const spn_when_t* options;
} spn_option_request_t;

typedef sp_da(spn_option_request_t) spn_option_requests_t;
typedef sp_ht(sp_intern_id_t, spn_option_requests_t) spn_option_seeds_t;

typedef struct {
  spn_option_setter_kind_t kind;
  sp_str_t name;
} spn_option_setter_t;

typedef struct {
  sp_str_t name;
  spn_option_value_t value;
  spn_option_setter_t setter;
  bool is_default;
} spn_resolved_option_t;

typedef sp_da(spn_resolved_option_t) spn_resolved_options_t;

typedef enum {
  SPN_OPTION_ERR_UNDECLARED,
  SPN_OPTION_ERR_BAD_VALUE,
  SPN_OPTION_ERR_CONFLICT,
  SPN_OPTION_ERR_VETO,
  SPN_OPTION_ERR_NO_VALUE,
  SPN_OPTION_ERR_LATE_GATE,
  SPN_OPTION_ERR_UNKNOWN_PKG,
} spn_option_err_t;

typedef struct {
  spn_option_err_t kind;
  sp_str_t pkg;
  sp_str_t option;
  spn_option_value_t value;
  spn_option_setter_t a;
  spn_option_setter_t b;
} spn_option_violation_t;

typedef sp_da(spn_option_violation_t) spn_option_violations_t;

typedef struct {
  spn_resolved_options_t options;
  spn_option_violations_t violations;
} spn_merged_options_t;


typedef struct {
  sp_opt(spn_linkage_t) kind;
  spn_when_t options;
  bool defaults_declined;
} spn_pkg_config_t;

typedef struct {
  sp_str_t key;
  spn_pkg_config_t value;
} spn_pkg_config_entry_t;

typedef struct {
  sp_str_t from;
  sp_str_t to;
} spn_publish_copy_t;

typedef struct {
  sp_str_t qualified;
  spn_git_patch_set_t set;
} spn_pkg_patch_t;

typedef sp_str_om(spn_target_info_t)     spn_target_map_t;
typedef sp_str_om(spn_profile_info_t)    spn_profile_map_t;
typedef sp_str_om(spn_index_info_t)      spn_index_map_t;
typedef sp_str_om(spn_toolchain_info_t) spn_toolchain_map_t;
typedef sp_str_om(spn_option_info_t)     spn_option_map_t;

struct spn_pkg_info {
  sp_str_t namespace;
  sp_str_t name;
  sp_str_t qualified;
  sp_str_t repo;
  sp_str_t author;
  sp_str_t maintainer;
  spn_semver_t version;
  struct {
    sp_str_t url;
    sp_str_t commit;
  } upstream;
  spn_target_map_t libs;
  spn_target_map_t exes;
  spn_target_map_t scripts;
  spn_target_map_t tests;
  spn_target_map_t examples;
  spn_profile_map_t profiles;
  spn_index_map_t indexes;
  sp_da(spn_requested_dep_t) deps;
  sp_da(spn_pkg_config_entry_t) config;
  sp_da(spn_pkg_patch_t) patches;
  spn_option_map_t options;
  sp_da(spn_path_t) include;
  sp_da(sp_str_t) define;
  sp_da(sp_str_t) public_define;
  sp_da(sp_str_t) system_deps;
  struct {
    sp_da(sp_str_t) frameworks;
    spn_os_version_t min_os;
  } macos;
  struct {
    spn_gated_list_t system_deps;
    spn_gated_path_list_t include;
    spn_gated_list_t define;
    spn_gated_list_t frameworks;
  } gated;
  spn_toolchain_map_t toolchains;
  spn_target_info_t build;
  spn_target_info_t configure;
  struct {
    sp_da(spn_publish_copy_t) copy;
  } publish;

  bool applied;
  sp_mem_arena_t* arena;
};

#endif
