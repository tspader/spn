#ifndef SPN_CACHE_H
#define SPN_CACHE_H

#include "sp.h"
#include "spn.h"
#include "pkg/types.h"

spn_pkg_t* spn_pkg_cache_find(spn_pkg_cache_t* cache, sp_str_t name);
spn_pkg_t* spn_pkg_cache_find_from_request(spn_pkg_cache_t* cache, spn_pkg_req_t request);
spn_pkg_t* spn_pkg_cache_ensure(spn_pkg_cache_t* cache, spn_pkg_req_t request);

#endif
