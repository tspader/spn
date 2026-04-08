#ifndef SPN_EVENT_EVENT_H
#define SPN_EVENT_EVENT_H

#include "event/types.h"
#include "tui/types.h"

void spn_build_event_init(spn_build_event_t* event, spn_build_event_kind_t kind, spn_build_ctx_t* ctx);
spn_event_buffer_t* spn_event_buffer_new();
void spn_event_buffer_push_kind(spn_event_buffer_t* evs, spn_build_ctx_t* ctx, spn_build_event_kind_t k);
void spn_event_buffer_push_ctx(spn_event_buffer_t* evs, spn_build_ctx_t* ctx, spn_build_event_t e);
void spn_event_buffer_push_ex(spn_event_buffer_t* evs, spn_pkg_t* pkg, spn_build_io_t* io, spn_build_event_t e);
void spn_event_buffer_push(spn_event_buffer_t* events, spn_build_event_t event);

spn_build_event_t spn_build_event_make(spn_build_ctx_t* ctx, spn_build_event_kind_t kind);
sp_da(spn_build_event_t) spn_event_buffer_drain(spn_event_buffer_t* events);
sp_str_t spn_build_event_kind_to_str(spn_build_event_kind_t kind);
spn_verbosity_t spn_build_event_get_verbosity(spn_build_event_kind_t kind);

#endif
