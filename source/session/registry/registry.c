#include "session/registry/registry.h"

#include "ctx/types.h"

#include "pkg/load.h"

spn_loaded_pkg_t* spn_registry_load_file_pkg(spn_pkg_registry_t* registry, sp_str_t qualified, sp_str_t path) {
  spn_loaded_pkg_t* existing = sp_str_ht_get(*registry, qualified);
  if (existing) return existing;

  sp_str_t manifest = path;
  if (sp_str_starts_with(manifest, SP_LIT("file://"))) {
    manifest = sp_str_strip_left(manifest, SP_LIT("file://"));
  }
  if (!sp_str_starts_with(manifest, SP_LIT("/"))) {
    manifest = sp_fs_join_path(spn.paths.project, manifest);
  }
  manifest = sp_fs_normalize_path(manifest);

  if (!sp_fs_exists(manifest)) return SP_NULLPTR;

  spn_pkg_info_t* info = sp_alloc_type(spn_pkg_info_t);
  spn_err_union_t result = spn_pkg_load(info, manifest);
  if (result.kind) return SP_NULLPTR;

  sp_str_ht_insert(*registry, qualified, sp_zero_struct(spn_loaded_pkg_t));
  spn_loaded_pkg_t* loaded = sp_str_ht_get(*registry, qualified);
  loaded->source = SPN_PKG_SOURCE_FILE;
  loaded->info = info;
  loaded->paths.manifest = manifest;
  loaded->paths.source = sp_fs_parent_path(manifest);
  loaded->paths.script = sp_fs_join_path(loaded->paths.source, sp_str_lit("spn.c"));

  return loaded;
}
