#ifndef SPN_EVENT_EVENT_H
#define SPN_EVENT_EVENT_H

#include "event/types.h"
#include "tui/types.h"

sp_str_t                 spn_build_event_kind_to_str(spn_build_event_kind_t kind);
spn_verbosity_t          spn_build_event_get_verbosity(spn_build_event_kind_t kind);
spn_event_buffer_t*      spn_event_buffer_new(sp_mem_t mem);
sp_da(spn_build_event_t) spn_event_buffer_drain(sp_mem_t mem, spn_event_buffer_t* events);
void                     spn_event_buffer_push(spn_event_buffer_t* events, spn_build_event_t event);
void                     spn_event_buffer_push_ex(spn_event_buffer_t* evs, spn_pkg_info_t* pkg, spn_build_io_t* io, spn_build_event_t e);

#endif
