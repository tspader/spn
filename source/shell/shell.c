#include "shell/shell.h"

#include "ctx/types.h"
#include "event/build.h"
#include "event/event.h"
#include "event/log.h"
#include "tui/tui.h"

static u32 short_thread_id(u64 thread_id) {
  static sp_ht(u64, u32) thread_map = SP_NULLPTR;
  static u32 id = 0;

  if (!thread_map) sp_ht_init(shell.tui.mem, thread_map);
  if (!sp_ht_key_exists(thread_map, thread_id)) {
    sp_ht_insert(thread_map, thread_id, id++);
  }
  return *sp_ht_getp(thread_map, thread_id);
}

void spn_shell_flush() {
  if (!spn.events) return;

  sp_mem_arena_marker_t s = sp_mem_begin_scratch();
  sp_da(spn_build_event_t) events = spn_event_buffer_drain(s.mem, spn.events);

  sp_da_for(events, it) {
    spn_build_event_t* event = &events[it];
    event->thread_id = short_thread_id(event->thread_id);

    if (shell.logger.jsonl.fd) {
      spn_event_log_jsonl(&shell.logger.jsonl.base, event);
    }
    if (event->io) {
      spn_event_log_jsonl(&event->io->jsonl.writer, event);
      spn_event_log_build(&event->io->build.writer, event);
    }

    spn_tui_log_event(&shell.tui, event);
  }

  sp_mem_end_scratch(s);
}
