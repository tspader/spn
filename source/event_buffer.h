#ifndef SPN_EVENT_BUFFER_H
#define SPN_EVENT_BUFFER_H

#include "sp.h"
#include "tui.h"

typedef struct spn_build_ctx spn_build_ctx_t;

typedef struct {
  sp_rb(spn_build_event_t) buffer;
  sp_mutex_t mutex;
  sp_cv_t condition;
} spn_event_buffer_t;

spn_event_buffer_t*      spn_event_buffer_new(void);
void                     spn_event_buffer_push(spn_event_buffer_t* evs, spn_build_ctx_t* ctx, spn_build_event_kind_t k);
void                     spn_event_buffer_push_ctx(spn_event_buffer_t* evs, spn_build_ctx_t* ctx, spn_build_event_t e);
void                     spn_event_buffer_push_ex(spn_event_buffer_t* evs, spn_pkg_t* pkg, spn_build_io_t* io, spn_build_event_t e);
sp_da(spn_build_event_t) spn_event_buffer_drain(spn_event_buffer_t* events);
spn_build_event_t        spn_build_event_make(spn_build_ctx_t* ctx, spn_build_event_kind_t kind);

#endif
