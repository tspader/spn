#include "index/dir.h"

#include "index/release.h"
#include "pkg/id.h"
#include "pkg/load.h"

spn_err_union_t spn_index_dir_get_package(spn_index_info_t* index, sp_mem_t mem, sp_intern_t* intern, spn_pkg_name_t id, spn_index_pkg_t** pkg) {
  *pkg = SP_NULLPTR;

  sp_str_t dir = sp_fs_join_path(mem, index->location, id.name);
  sp_str_t manifest = sp_fs_join_path(mem, dir, sp_str_lit("spn.toml"));

  spn_pkg_info_t* info = sp_alloc_type(mem, spn_pkg_info_t);
  spn_err_union_t loaded = spn_pkg_load(mem, intern, manifest, SPN_MANIFEST_DEP, spn_pkg_name_to_qualified(id), info);
  if (loaded.kind == SPN_ERR_NO_MANIFEST) {
    return spn_result(SPN_OK);
  }
  if (loaded.kind) {
    return loaded;
  }

  if (!sp_str_equal(info->name, id.name)) {
    return (spn_err_union_t) {
      .kind = SPN_ERR_INDEX_CORRUPT,
      .index_corrupt = { .name = spn_pkg_name_to_qualified(id), .path = manifest },
    };
  }

  if (!sp_str_equal(info->qualified, spn_pkg_name_to_qualified(id))) {
    return spn_result(SPN_OK);
  }

  spn_index_release_t release = sp_zero;
  spn_try_union(spn_index_release_from_pkg(mem, info, &release));

  release.manifest = (spn_pkg_root_t) { .kind = SPN_PKG_ROOT_LOCAL, .local = dir };

  spn_index_pkg_t* package = sp_alloc_type(mem, spn_index_pkg_t);
  *package = (spn_index_pkg_t) { .id = id };
  sp_da_init(mem, package->releases);
  sp_da_push(package->releases, release);
  *pkg = package;
  return spn_result(SPN_OK);
}
