#ifndef SPN_RESOLVE_RESOLVE_H
#define SPN_RESOLVE_RESOLVE_H

#include "error/types.h"
#include "lock/types.h"
#include "resolve/types.h"

void spn_resolver_init(spn_resolver_t* r, spn_index_cache_t* index, spn_pkg_registry_t* registry, spn_event_buffer_t* events);
void spn_resolve_query_add(spn_resolve_query_t* query, spn_requested_pkg_t req);
spn_err_t spn_resolve_from_solver(spn_resolver_t* resolver, spn_resolve_query_t* query);
spn_err_t spn_resolve_from_lock_file(spn_resolver_t* resolver, spn_lock_file_t* lock);

spn_requested_pkg_t spn_pkg_req_from_str(sp_str_t str);

#endif
