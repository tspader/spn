#ifndef SPN_INDEX_TYPES_H
#define SPN_INDEX_TYPES_H

#include "sp.h"
#include "spn/session.h"

#include "sp/sp_om.h"
#include "pkg/types.h"
#include "semver/types.h"

typedef enum {
  SPN_INDEX_DEP_NORMAL,
  SPN_INDEX_DEP_BUILD,
  SPN_INDEX_DEP_TEST,
} spn_index_dep_kind_t;

typedef struct {
  spn_index_dep_kind_t kind;
  bool private;
  spn_pkg_name_t id;
  spn_semver_range_t range;
  spn_when_t when;
  spn_when_t options;
} spn_index_dep_t;

typedef struct {
  sp_str_t manifest;
  sp_str_t script;
} spn_index_rel_paths_t;

typedef struct {
  sp_str_t name;
  sp_da(spn_linkage_t) linkages;
} spn_index_target_t;

typedef struct {
  spn_pkg_name_t id;
  spn_semver_t version;

  bool yanked;

  spn_pkg_tree_t source;
  spn_pkg_tree_t manifest;
  spn_index_rel_paths_t paths;

  sp_da(spn_index_dep_t) deps;
  sp_da(spn_index_target_t) targets;
  spn_option_map_t options;
} spn_index_release_t;

typedef struct {
  spn_pkg_name_t id;
  sp_da(spn_index_release_t) releases;
} spn_index_pkg_t;

struct spn_index_info {
  sp_str_t name;
  spn_index_kind_t kind;
  spn_index_protocol_t protocol;
  union {
    struct {
      sp_str_t url;
      sp_str_t rev;
      sp_str_t publish_url;
    } git;
    struct {
      sp_str_t url;
    } http;
    struct {
      sp_str_t path;
    } dir;
  };
  sp_str_t location;
  u32 refresh;
};

typedef sp_da(spn_index_info_t) spn_index_arr_t;

typedef struct {
  sp_mem_t mem;
  sp_intern_t* intern;
  spn_index_arr_t* indexes;
  sp_str_om(spn_index_pkg_t*) packages;
} spn_index_cache_t;

#endif
