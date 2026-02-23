#ifndef SPN_LOCK_H
#define SPN_LOCK_H

#include "sp.h"
#include "pkg.h"
#include "semver.h"
#include "spn.h"

typedef enum {
  SPN_DEP_IMPORT_KIND_EXPLICIT,
  SPN_DEP_IMPORT_KIND_TRANSITIVE
} spn_dep_import_kind_t;

typedef struct {
  sp_str_t name;
  spn_semver_t version;
  sp_str_t commit;
  spn_dep_import_kind_t import_kind;
  spn_visibility_t visibility;
  spn_pkg_kind_t kind;
  sp_da(sp_str_t) deps;
  sp_da(sp_str_t) dependents;
} spn_lock_entry_t;

typedef struct {
  spn_semver_t version;
  sp_str_t commit;
  sp_ht(sp_str_t, spn_lock_entry_t) entries;
  sp_ht(sp_str_t, bool) system_deps;
} spn_lock_file_t;

void spn_lock_file_init(spn_lock_file_t* lock);
spn_lock_file_t spn_build_lock_file(void);
spn_lock_file_t spn_lock_file_load(sp_str_t path);

#endif
