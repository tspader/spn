#include "spn/host.h"

#include "event/event.h"

#include "core/core.h"
#include "ctx/types.h"
#include "log/lazy/lazy.h"

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
  sp_io_dyn_mem_writer_init(mem, &events->backlog);
  return events;
}

spn_err_t spn_event_log_open(spn_event_buffer_t* events, sp_str_t path) {
  sp_mutex_lock(&events->mutex);
  if (!events->log.writer.write) {
    spn_lazy_log_init(&events->log, path);
    sp_io_write(&events->log.writer, events->backlog.storage.data, events->backlog.cursor, SP_NULLPTR);
  }
  spn_err_t err = events->log.failed ? SPN_ERR_FS_WRITE : SPN_OK;
  sp_mutex_unlock(&events->mutex);
  return err;
}

void spn_event_buffer_push(spn_event_buffer_t* events, spn_event_t event) {
  event.thread_id = current_thread_id();
  event.epoch = sp_tm_now_epoch();

  sp_mutex_lock(&events->mutex);
  sp_rb_push(events->buffer, event);

  sp_io_writer_t* sink = events->log.writer.write ? &events->log.writer : &events->backlog.base;
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();

  sp_io_dyn_mem_writer_t line = sp_zero;
  sp_io_dyn_mem_writer_init(scratch.mem, &line);
  spn_event_log_jsonl(&line.base, &event);
  sp_io_write(sink, line.storage.data, line.cursor, SP_NULLPTR);

  sp_mem_end_scratch(scratch);

  sp_mutex_unlock(&events->mutex);

  if (events->wake) {
    spn_wake_ring(events->wake);
  }
}

spn_event_t* spn_ctx_drain(spn_ctx_t* ctx) {
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

sp_da(spn_event_t) spn_event_buffer_drain(sp_mem_t mem, spn_event_buffer_t* events) {
  sp_mutex_lock(&events->mutex);

  sp_da(spn_event_t) result = sp_da_new(mem, spn_event_t);
  sp_rb_for(events->buffer, it) {
    spn_event_t* event = &sp_rb_at(events->buffer, it);
    sp_da_push(result, *event);
  }

  sp_rb_clear(events->buffer);
  sp_mutex_unlock(&events->mutex);

  return result;
}
