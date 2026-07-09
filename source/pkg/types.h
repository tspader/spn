#ifndef SPN_PKG_TYPES_H
#define SPN_PKG_TYPES_H

#include "sp.h"

#include "forward/types.h"
#include "git/types.h"
#include "intern/types.h"
#include "sp/sp_om.h"
#include "semver/types.h"
#include "target/types.h"
#include "toolchain/types.h"
#include "when/types.h"

typedef enum {
  SPN_PKG_SOURCE_ROOT,
  SPN_PKG_SOURCE_FILE,
  SPN_PKG_SOURCE_INDEX,
} spn_pkg_source_t;

// A tree spn can place on disk: a path that already exists, or a git repo to
// check out. Manifests and source code are each one of these, materialized
// independently, so "local recipe + remote source" is just LOCAL + GIT.
typedef enum {
  SPN_PKG_TREE_NONE,
  SPN_PKG_TREE_LOCAL,
  SPN_PKG_TREE_GIT,
} spn_pkg_tree_kind_t;

typedef struct {
  spn_pkg_tree_kind_t kind;
  union {
    sp_str_t local;
    spn_git_checkout_id_t git;
  };
} spn_pkg_tree_t;


typedef struct {
  sp_str_t namespace;
  sp_str_t name;
} spn_pkg_name_t;

typedef struct SP_ALIGNED {
  sp_intern_id_t qualified;
  sp_hash_t hash;
  spn_semver_t version;
} spn_pkg_id_t;

typedef enum {
  SPN_DEP_KIND_PACKAGE,
  SPN_DEP_KIND_BUILD,
  SPN_DEP_KIND_TEST,
} spn_dep_kind_t;

typedef struct spn_pkg_req {
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
} spn_requested_pkg_t;


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

typedef sp_str_om(spn_target_info_t)     spn_target_info_om_t;
typedef sp_str_om(spn_profile_info_t)    spn_profile_info_om_t;
typedef sp_str_om(spn_index_info_t)      spn_index_info_om_t;
typedef sp_str_om(spn_toolchain_t) spn_toolchain_om_t;
typedef sp_str_om(spn_option_info_t)     spn_option_info_om_t;

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
  spn_target_info_om_t libs;
  spn_target_info_om_t exes;
  spn_target_info_om_t scripts;
  spn_target_info_om_t tests;
  spn_profile_info_om_t profiles;
  spn_index_info_om_t indexes;
  sp_da(spn_requested_pkg_t) deps;
  sp_da(spn_pkg_config_entry_t) config;
  spn_option_info_om_t options;
  sp_da(sp_str_t) include;
  sp_da(sp_str_t) define;
  sp_da(sp_str_t) public_define;
  sp_da(sp_str_t) system_deps;
  struct {
    spn_gated_list_t system_deps;
  } gated;
  spn_toolchain_om_t toolchains;
  spn_target_info_t build;
  spn_target_info_t configure;
  struct {
    sp_da(spn_publish_copy_t) copy;
  } publish;

  bool applied;
  sp_mem_arena_t* arena;
};

#endif
