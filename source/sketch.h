#include "sp.h"
#include "forward/types.h"
#include "index/types.h"
#include "pkg/types.h"
#include "resolve/types.h"
#include "semver/types.h"
#include "unit/types.h"

typedef enum {
  SPN_PKG_SOURCE_ROOT,
  SPN_PKG_SOURCE_FILE,
  SPN_PKG_SOURCE_INDEX,
} pkg_source_t;

typedef struct {
  sp_str_t qualified;
  spn_semver_t version;
} pkg_id_t;

typedef struct {
  spn_pkg_id_t id;
  spn_pkg_dep_kind_t kind;
  spn_pkg_source_t source;
  spn_semver_range_t version;
} requested_dep_t;

typedef struct {
  pkg_id_t id;
  spn_pkg_dep_kind_t kind;
  spn_pkg_source_t source;
} resolved_dep_t;

// We want an array of all packages in the build (both index packages and local
// packages). This is a lightweight struct to represent that; there might also
// be a union if we need the specific data per-type
//
// Also, this is a filtered list of dependencies
typedef struct {
  pkg_id_t id;
  sp_da(resolved_dep_t) deps;
} resolved_pkg_t;

// A single JSONL entry from the index
typedef spn_index_rel_t index_release_t;

typedef struct {
  sp_om(pkg_id_t, resolved_pkg_t) packages;
} resolve_t;

#define str(_str) sp_str_lit(_str)
void sketch() {
  resolved_dep_t deps [] = {
    {
      .id = { str("core"), str("sqlite") },
      .version = spn_semver_lit(3, 51, 1),
      .kind = SPN_DEP_KIND_PACKAGE,
      .source = SPN_PKG_SOURCE_INDEX
    },
    {
      .id = { str("core"), str("sqlite") },
      .version = spn_semver_lit(3, 51, 1),
      .kind = SPN_DEP_KIND_BUILD,
      .source = SPN_PKG_SOURCE_INDEX
    },
  };
}
