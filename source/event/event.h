#ifndef SPN_EVENT_EVENT_H
#define SPN_EVENT_EVENT_H

#include "event/types.h"

extern const spn_event_info_t spn_event_info[SPN_EVENT_COUNT];

// The union arm carrying an event's payload, per the SPN_EVENT_LIST row;
// null for events without one
void* spn_event_payload(spn_build_event_t* event);

spn_event_buffer_t*      spn_event_buffer_new(sp_mem_t mem);
sp_da(spn_build_event_t) spn_event_buffer_drain(sp_mem_t mem, spn_event_buffer_t* events);
void                     spn_event_buffer_push(spn_event_buffer_t* events, spn_build_event_t event);
void                     spn_event_buffer_push_ex(spn_event_buffer_t* evs, spn_pkg_info_t* pkg, spn_build_io_t* io, spn_build_event_t e);

#endif
