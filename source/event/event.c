#include "event/event.h"

#if defined(SP_POSIX)
  #include <pthread.h>
#endif

static u64 spn_current_thread_id(void) {
#if defined(_WIN32)
  return (u64)GetCurrentThreadId();
#elif defined(__linux__)
  return (u64)pthread_self();
#elif defined(__APPLE__)
  u64 tid;
  pthread_threadid_np(NULL, &tid);
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
  event.thread_id = spn_current_thread_id();

  sp_mutex_lock(&events->mutex);
  sp_rb_push(events->buffer, event);
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
