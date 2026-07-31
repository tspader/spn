#include "sp.h"
#include "session/registry/registry.h"

#include "pkg/load.h"
#include "pkg/id.h"

spn_err_union_t spn_registry_load_file_pkg(spn_pkg_registry_t* registry, sp_mem_t mem, sp_intern_t* intern, sp_str_t qualified, sp_str_t manifest, spn_registry_pkg_t** pkg) {
  spn_pkg_id_t id = spn_pkg_id(intern, qualified);
  spn_registry_pkg_t* existing = sp_ht_getp(*registry, id);
  if (existing) {
    *pkg = existing;
    return spn_result(SPN_OK);
  }

  *pkg = SP_NULLPTR;

  spn_pkg_info_t* info = sp_alloc_type(mem, spn_pkg_info_t);
  try_union(spn_pkg_load(mem, intern, manifest, SPN_MANIFEST_DEP, qualified, info));

  // Option requests, config keys, and edge lookups all route by the name the
  // edge requested; a manifest declaring some other name would strand them
  if (!sp_str_equal(info->qualified, qualified)) {
    return (spn_err_union_t) {
      .kind = SPN_ERR_PKG_MISMATCH,
      .mismatch = { .path = manifest, .declared = info->qualified, .requested = qualified },
    };
  }

  sp_ht_insert(*registry, id, ((spn_registry_pkg_t) {
    .source = SPN_PKG_SOURCE_FILE,
    .info = info,
    .manifest = manifest,
  }));

  *pkg = sp_ht_getp(*registry, id);
  return spn_result(SPN_OK);
}
