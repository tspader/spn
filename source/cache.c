#include "cache.h"
#include "pkg/load.h"
#include "pkg/mutate.h"
#include "pkg/pkg.h"

#include "intern.h"

spn_pkg_t* spn_pkg_cache_find(spn_pkg_cache_t* cache, sp_str_t name) {
  return sp_om_get(*cache, name);
}

spn_pkg_t* spn_pkg_cache_find_from_request(spn_pkg_cache_t* cache, spn_pkg_req_t request) {
  spn_pkg_t* package = spn_pkg_cache_find(cache, request.id.name);
  if (!package) {
    return SP_NULLPTR;
  }

  if (package->kind != request.kind) {
    return SP_NULLPTR;
  }

  return package;
}

spn_pkg_t* spn_pkg_cache_ensure(spn_pkg_cache_t* cache, spn_pkg_req_t request) {
  sp_str_t name = spn_intern(request.id.name);

  if (!sp_om_has(*cache, name)) {
    sp_om_insert(*cache, name, SP_ZERO_STRUCT(spn_pkg_t));
    spn_pkg_t* pkg = sp_om_get(*cache, name);
    spn_pkg_init(pkg, name);

    switch (request.kind) {
      case SPN_PACKAGE_KIND_FILE: {
        sp_str_t prefix = sp_str_lit("file://");
        sp_str_t manifest = {
          .data = request.file.data + prefix.len,
          .len = request.file.len - prefix.len
        };
        sp_try_as_null(spn_pkg_from_manifest(pkg, manifest));

        break;
      }
      case SPN_PACKAGE_KIND_INDEX: {
        SP_FATAL("Legacy index package loading was removed. New index-backed resolution is not implemented yet.");

        break;
      }
      case SPN_PACKAGE_KIND_ROOT: {
        SP_FATAL("unimplemented find_package");
        break;
      }
    }
  }

  return spn_pkg_cache_find_from_request(cache, request);
}
