#include "event.h"
#include "app.h"
#include "unit.h"

void spn_build_event_init(spn_build_event_t* event, spn_build_event_kind_t kind, spn_build_ctx_t* ctx) {
  event->kind = kind;
  event->pkg = ctx->pkg;
  event->io = &ctx->logs;
}

spn_build_event_t spn_build_event_make(spn_build_ctx_t* ctx, spn_build_event_kind_t kind) {
  spn_build_event_t event = SP_ZERO_INITIALIZE();
  spn_build_event_init(&event, kind, ctx);
  return event;
}


spn_event_buffer_t* spn_event_buffer_new() {
  spn_event_buffer_t* events = SP_ALLOC(spn_event_buffer_t);
  return events;
}

void spn_event_buffer_push(spn_event_buffer_t* events, spn_build_ctx_t* ctx, spn_build_event_kind_t kind) {
  spn_event_buffer_push_ctx(events, ctx, spn_build_event_make(ctx, kind));
}

void spn_event_buffer_push_ctx(spn_event_buffer_t* events, spn_build_ctx_t* ctx, spn_build_event_t config) {
  spn_build_event_t event = config;
  spn_build_event_init(&event, event.kind, ctx);

  sp_mutex_lock(&events->mutex);
  sp_rb_push(events->buffer, event);
  sp_mutex_unlock(&events->mutex);
}

void spn_event_buffer_push_ex(spn_event_buffer_t* events, spn_pkg_t* pkg, spn_build_io_t* io, spn_build_event_t e) {
  spn_build_event_t event = e;
  event.pkg = pkg;
  event.io = io;

  sp_mutex_lock(&events->mutex);
  sp_rb_push(events->buffer, event);
  sp_mutex_unlock(&events->mutex);
}

sp_da(spn_build_event_t) spn_event_buffer_drain(spn_event_buffer_t* events) {
  sp_mutex_lock(&events->mutex);

  sp_da(spn_build_event_t) result = SP_NULLPTR;
  sp_rb_for(events->buffer, it) {
    spn_build_event_t* event = &sp_rb_at(events->buffer, it);
    sp_da_push(result, *event);
  }

  sp_rb_clear(events->buffer);
  sp_mutex_unlock(&events->mutex);

  return result;
}


void spn_push_event(spn_build_event_kind_t kind) {
  spn_push_event_ex((spn_build_event_t) {
    .kind = kind
  });
}

void spn_push_event_ex(spn_build_event_t event) {
  spn_event_buffer_push_ctx(spn.events, &spn_session_find_root(&app.session)->ctx, event);
}
