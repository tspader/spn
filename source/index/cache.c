#include "index/cache.h"
#include "index/index.h"
#include "pkg/id.h"
#include "semver/compare.h"

void spn_index_cache_init(spn_index_cache_t* cache, sp_mem_t mem, sp_intern_t* intern, spn_index_arr_t* indexes) {
  cache->mem = mem;
  cache->intern = intern;
  cache->indexes = indexes;
  sp_str_om_new(cache->packages);
}

spn_index_pkg_t* spn_index_cache_get_package(spn_index_cache_t* cache, spn_pkg_name_t id) {
  sp_assert(cache);

  sp_str_t qualified = spn_pkg_name_to_qualified(id);

  if (sp_str_om_has(cache->packages, qualified)) {
    return sp_str_om_get(cache->packages, qualified);
  }

  sp_da_for(*cache->indexes, it) {
    spn_index_info_t* index = &(*cache->indexes)[it];
    spn_index_pkg_t* package = spn_index_get_package(index, id);
    if (!package) {
      continue;
    }

    sp_str_om_insert(cache->packages, qualified, *package);
    return sp_str_om_get(cache->packages, qualified);
  }

  return SP_NULLPTR;
}

spn_index_release_t* spn_index_cache_get_release(spn_index_cache_t* cache, spn_pkg_name_t id, spn_semver_t version) {
  spn_index_pkg_t* pkg = spn_index_cache_get_package(cache, id);
  if (!pkg) return SP_NULLPTR;

  sp_da_for(pkg->releases, it) {
    if (spn_semver_eq(pkg->releases[it].version, version)) {
      return &pkg->releases[it];
    }
  }

  return SP_NULLPTR;
}
