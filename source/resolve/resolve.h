#ifndef SPN_RESOLVE_RESOLVE_H
#define SPN_RESOLVE_RESOLVE_H

#include "error/types.h"
#include "lock/types.h"
#include "resolve/types.h"

void spn_resolver_init(spn_resolver_t* r, sp_mem_t mem, sp_intern_t* intern, spn_index_cache_t* index, spn_pkg_registry_t* registry, spn_event_buffer_t* events, spn_linkage_t linkage, sp_da(spn_pkg_config_entry_t) config);
void spn_resolve_query_init(sp_mem_t mem, spn_resolve_query_t* query);
void spn_resolve_query_add(spn_resolve_query_t* query, spn_requested_pkg_t req);
spn_err_t spn_resolve_from_solver(spn_resolver_t* resolver, spn_resolve_query_t* query);
spn_err_t spn_resolve_from_lock_file(spn_resolver_t* resolver, spn_lock_file_t* lock);

#endif
