#include "event/event.h"
#include "event/build.h"
#include "event/log.h"

#include <stddef.h>

#if defined(SP_POSIX)
  #include <pthread.h>
#endif

#define SPN_EVT(member) (u16)offsetof(spn_build_event_t, member)
#define SPN_EVT_NONE 0
#define SPN_EVENT_INFO(kind, name_, display_, verbosity_, level_, error_, terminal_, payload_) \
  [kind] = { \
    .name = name_, \
    .display = display_, \
    .verbosity = SPN_VERBOSITY_##verbosity_, \
    .level = SPN_LOG_LEVEL_##level_, \
    .error = error_, \
    .terminal = terminal_, \
    .payload = payload_, \
  },

const spn_event_info_t spn_event_info[SPN_EVENT_COUNT] = {
  SPN_EVENT_LIST(SPN_EVENT_INFO)
};

#undef SPN_EVENT_INFO
#undef SPN_EVT
#undef SPN_EVT_NONE

void* spn_event_payload(spn_build_event_t* event) {
  u16 offset = spn_event_info[event->kind].payload;
  return offset ? (u8*)event + offset : SP_NULLPTR;
}

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
  sp_mutex_init(&events->mutex, SP_MUTEX_PLAIN);
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
