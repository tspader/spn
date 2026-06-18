#ifndef SPN_PKG_TYPES_H
#define SPN_PKG_TYPES_H

#include "sp.h"

#include "forward/types.h"
#include "intern/types.h"
#include "sp/sp_om.h"
#include "option/types.h"
#include "semver/types.h"
#include "toolchain/types.h"

typedef enum {
  SPN_PKG_SOURCE_ROOT,
  SPN_PKG_SOURCE_FILE,
  SPN_PKG_SOURCE_INDEX,
} spn_pkg_source_t;


typedef struct {
  sp_str_t namespace;
  sp_str_t name;
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
  union {
    struct { spn_semver_range_t range; } index;
    struct { sp_str_t path; } file;
  };
} spn_requested_pkg_t;


#define spn_pkg_map(T) sp_om(sp_str_t, T)

#define spn_pkg_map_ensure(om) \
  if (!(om)) { \
    spn_pkg_map_init(om); \
  }

#define spn_pkg_map_init(om) \
  sp_om_new(om); \
  sp_om_set_fns(om, spn_pkg_key_hash, spn_pkg_key_compare)

#define spn_pkg_map_insert(om, key, val)                                             \
  do { \
    sp_om_ensure(om); \
    sp_om_insert(om, (key), (val)); \
  } while (0)

#define spn_pkg_map_new(om)           spn_pkg_map_init(om);
#define spn_pkg_map_free(om)          sp_om_free(om)
#define spn_pkg_map_get(om, key)      sp_om_get(om, (key))
#define spn_pkg_map_getp(om, key)     sp_om_getp(om, (key))
#define spn_pkg_map_has(om, key)      sp_om_has(om, (key))
#define spn_pkg_map_at(om, n)         sp_om_at(om, n)
#define spn_pkg_map_size(om)          sp_om_size(om)
#define spn_pkg_map_empty(om)         sp_om_empty(om)
#define spn_pkg_map_for(om, it)       sp_om_for(om, it)
#define spn_pkg_map_back(om)          sp_om_back(om)


typedef struct {
  spn_semver_t version;
  sp_str_t commit;
} spn_pkg_metadata_t;

struct spn_pkg_info {
  sp_str_t namespace;
  sp_str_t name;
  sp_str_t qualified;
  sp_str_t repo;
  sp_str_t url;
  sp_str_t author;
  sp_str_t maintainer;
  spn_semver_t version;
  sp_str_om(spn_target_info_t) libs;
  sp_str_om(spn_target_info_t) exes;
  sp_str_om(spn_target_info_t) scripts;
  sp_str_om(spn_target_info_t) tests;
  sp_str_om(spn_profile_info_t) profiles;
  sp_str_om(spn_index_info_t) indexes;
  sp_ht(sp_str_t, spn_requested_pkg_t) deps;
  sp_ht(sp_str_t, spn_dep_option_t) options;
  sp_ht(sp_str_t, spn_dep_options_t) config;
  sp_ht(spn_semver_t, spn_pkg_metadata_t) metadata;
  sp_da(spn_semver_t) versions;
  sp_da(sp_str_t) include;
  sp_da(sp_str_t) define;
  sp_da(sp_str_t) system_deps;
  sp_str_om(spn_toolchain_entry_t) toolchains;

  sp_mem_arena_t* arena;
};

#endif
