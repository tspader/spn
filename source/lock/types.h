#ifndef SPN_LOCK_TYPES_H
#define SPN_LOCK_TYPES_H

#include "sp.h"
#include "spn.h"

#include "pkg/types.h"
#include "semver/types.h"

typedef enum {
  SPN_DEP_IMPORT_KIND_EXPLICIT,
  SPN_DEP_IMPORT_KIND_TRANSITIVE
} spn_dep_import_kind_t;

typedef struct {
  sp_str_t name;
  spn_semver_t version;
  sp_str_t commit;
  spn_dep_import_kind_t import_kind;
  spn_pkg_source_t kind;
  struct {
    sp_str_t url;
    sp_str_t rev;
    sp_str_t dir;
  } source;
  struct {
    sp_str_t url;
    sp_str_t rev;
    sp_str_t dir;
  } manifest;
  struct {
    sp_str_t manifest;
    sp_str_t script;
  } paths;
  sp_da(sp_str_t) deps;
  sp_da(sp_str_t) dependents;
} spn_lock_entry_t;

typedef struct {
  spn_semver_t version;
  sp_str_t commit;
  sp_ht(sp_str_t, spn_lock_entry_t) entries;
  sp_ht(sp_str_t, bool) system_deps;
} spn_lock_file_t;

#endif
