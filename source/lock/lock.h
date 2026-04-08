#ifndef SPN_LOCK_LOCK_H
#define SPN_LOCK_LOCK_H

#include "event/types.h"
#include "lock/types.h"
#include "resolve/types.h"

void spn_lock_file_init(spn_lock_file_t* lock);
spn_lock_file_t spn_build_lock_file(spn_resolver_t* resolver, spn_pkg_info_t* root);
spn_lock_file_t spn_lock_file_parse(sp_str_t toml, spn_event_buffer_t* events);
spn_lock_file_t spn_lock_file_load(sp_str_t path, spn_event_buffer_t* events);
sp_str_t spn_lock_file_to_str(spn_lock_file_t* lock);

#endif
