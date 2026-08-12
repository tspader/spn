#include "spn/host.h"

#include "event/event.h"
#include "event/build.h"

#include "core/core.h"
#include "ctx/types.h"

#if defined(SP_POSIX)
  #include <pthread.h>
#endif

static u64 current_thread_id() {
#if defined(SP_WIN32)
  return (u64)GetCurrentThreadId();
#elif defined(SP_LINUX)
  return (u64)pthread_self();
#elif defined(SP_MACOS)
  u64 tid;
  pthread_threadid_np(SP_NULLPTR, &tid);
  return tid;
#else
  return (u64)pthread_self();
#endif
}

spn_event_buffer_t* spn_event_buffer_new(sp_mem_t mem) {
  spn_event_buffer_t* events = sp_alloc_type(mem, spn_event_buffer_t);
  sp_rb_init(mem, events->buffer);
  return events;
}

void spn_event_buffer_push_ex(spn_event_buffer_t* events, spn_pkg_info_t* pkg, spn_build_io_t* io, spn_build_event_t e) {
  spn_build_event_t event = e;
  event.pkg = pkg;
  event.io = io;
  spn_event_buffer_push(events, event);
}

void spn_event_buffer_push(spn_event_buffer_t* events, spn_build_event_t event) {
  event.thread_id = current_thread_id();
  event.epoch = sp_tm_now_epoch();

  sp_mutex_lock(&events->mutex);
  sp_rb_push(events->buffer, event);

  if (events->log.writer.write || event.io) {
    sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();

    sp_io_dyn_mem_writer_t line = sp_zero;
    sp_io_dyn_mem_writer_init(scratch.mem, &line);
    spn_event_log_jsonl(&line.base, &event);
    if (events->log.writer.write) {
      sp_io_write(&events->log.writer, line.storage.data, line.cursor, SP_NULLPTR);
    }

    if (event.io) {
      sp_io_write(&event.io->jsonl.writer, line.storage.data, line.cursor, SP_NULLPTR);

      sp_io_dyn_mem_writer_t text = sp_zero;
      sp_io_dyn_mem_writer_init(scratch.mem, &text);
      spn_event_log_build(&text.base, &event);
      if (text.cursor) {
        sp_io_write(&event.io->build.writer, text.storage.data, text.cursor, SP_NULLPTR);
      }
    }

    sp_mem_end_scratch(scratch);
  }

  sp_mutex_unlock(&events->mutex);

  if (events->wake) {
    spn_wake_ring(events->wake);
  }
}

spn_build_event_t* spn_ctx_drain(spn_ctx_t* ctx) {
  spn_event_buffer_t* events = ctx->events;

  sp_mutex_lock(&events->mutex);
  if (sp_rb_empty(events->buffer)) {
    if (events->wake) {
      spn_wake_rearm(events->wake);
    }
    sp_mutex_unlock(&events->mutex);
    return SP_NULLPTR;
  }

  events->current = sp_rb_at(events->buffer, 0);
  sp_rb_pop(events->buffer);
  sp_mutex_unlock(&events->mutex);
  return &events->current;
}

sp_da(spn_build_event_t) spn_event_buffer_drain(sp_mem_t mem, spn_event_buffer_t* events) {
  sp_mutex_lock(&events->mutex);

  sp_da(spn_build_event_t) result = sp_da_new(mem, spn_build_event_t);
  sp_rb_for(events->buffer, it) {
    spn_build_event_t* event = &sp_rb_at(events->buffer, it);
    sp_da_push(result, *event);
  }

  sp_rb_clear(events->buffer);
  sp_mutex_unlock(&events->mutex);

  return result;
}
