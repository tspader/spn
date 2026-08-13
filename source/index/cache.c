#include "index/cache.h"
#include "index/index.h"
#include "pkg/id.h"

void spn_index_cache_init(spn_index_cache_t* cache, sp_mem_t mem, sp_intern_t* intern, sp_da(spn_index_info_t)* indexes) {
  cache->mem = mem;
  cache->intern = intern;
  cache->indexes = indexes;
  sp_str_om_new(cache->packages);
}

spn_err_t spn_index_cache_get_package(spn_index_cache_t* cache, spn_pkg_name_t id, spn_index_pkg_t** pkg, spn_index_diag_t* diag) {
  *pkg = SP_NULLPTR;

  sp_str_t qualified = spn_pkg_name_to_qualified(id);
  if (sp_str_om_has(cache->packages, qualified)) {
    *pkg = *sp_str_om_get(cache->packages, qualified);
    return SPN_OK;
  }

  spn_index_pkg_t* package = SP_NULLPTR;
  sp_da_for(*cache->indexes, it) {
    spn_index_info_t* index = &(*cache->indexes)[it];
    spn_try(spn_index_get_package(index, cache->mem, cache->intern, id, &package, diag));
    if (package) {
      break;
    }
  }

  sp_str_om_insert(cache->packages, qualified, package);
  *pkg = package;
  return SPN_OK;
}

