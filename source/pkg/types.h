#ifndef SPN_PKG_TYPES_H
#define SPN_PKG_TYPES_H

#include "sp.h"

#include "forward/types.h"
#include "ordered_map.h"
#include "option/types.h"
#include "semver/types.h"
#include "toolchain/types.h"

#define SPN_PACKAGE_KIND(X) \
  X(SPN_PACKAGE_KIND_ROOT, "root") \
  X(SPN_PACKAGE_KIND_FILE, "file") \
  X(SPN_PACKAGE_KIND_INDEX, "index")

typedef enum {
  SPN_PACKAGE_KIND(SP_X_NAMED_ENUM_DEFINE)
} spn_pkg_kind_t;


typedef struct {
  sp_str_t namespace;
  sp_str_t name;
} spn_pkg_id_t;

typedef struct {
  spn_semver_t version;
  sp_str_t commit;
} spn_pkg_metadata_t;

typedef struct spn_pkg_req {
  spn_pkg_id_t id;
  spn_pkg_kind_t kind;
  u8 test;
  u8 build;
  u8 package;
  spn_visibility_t visibility;
  union {
    spn_semver_range_t range;
    sp_str_t file;
  };
} spn_requested_pkg_t;

struct spn_pkg_info {
  sp_str_t namespace;
  sp_str_t name;
  sp_str_t qualified;
  sp_str_t repo;
  sp_str_t url;
  sp_str_t author;
  sp_str_t maintainer;
  spn_semver_t version;
  sp_om(spn_target_info_t) libs;
  sp_om(spn_target_info_t) exes;
  sp_om(spn_target_info_t) scripts;
  sp_om(spn_target_info_t) tests;
  sp_om(spn_profile_info_t) profiles;
  sp_om(spn_index_info_t) indexes;
  sp_ht(sp_str_t, spn_requested_pkg_t) deps;
  sp_ht(sp_str_t, spn_dep_option_t) options;
  sp_ht(sp_str_t, spn_dep_options_t) config;
  sp_ht(spn_semver_t, spn_pkg_metadata_t) metadata;
  sp_da(spn_semver_t) versions;
  sp_da(sp_str_t) include;
  sp_da(sp_str_t) define;
  sp_da(sp_str_t) system_deps;
  sp_om(spn_toolchain_entry_t) toolchains;

  sp_mem_arena_t* arena;
};

typedef sp_str_ht(sp_str_t) spn_pkg_registry_t;

#endif
