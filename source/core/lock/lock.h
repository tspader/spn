#ifndef SPN_LOCK_LOCK_H
#define SPN_LOCK_LOCK_H

#include "event/types.h"
#include "lock/types.h"
#include "resolve/types.h"

void spn_lock_file_init(sp_mem_t mem, spn_lock_file_t* lock);
spn_lock_file_t spn_build_lock_file(sp_mem_t mem, sp_intern_t* intern, spn_resolve_t resolve, spn_pkg_info_t* root);
spn_lock_file_t spn_lock_file_parse(sp_mem_t mem, sp_str_t toml, spn_event_buffer_t* events);
spn_lock_file_t spn_lock_file_load(sp_mem_t mem, sp_str_t path, spn_event_buffer_t* events);
sp_str_t spn_lock_file_to_str(sp_mem_t mem, spn_lock_file_t* lock);

#endif
