#ifndef SPN_LOG_H
#define SPN_LOG_H

#include "sp.h"

typedef enum {
  SPN_LOG_LEVEL_ERROR,
  SPN_LOG_LEVEL_WARN,
  SPN_LOG_LEVEL_INFO,
  SPN_LOG_LEVEL_DEBUG,
} spn_log_level_t;

spn_log_level_t spn_log_level_from_str(sp_str_t str);

void spn_log_info(const c8* fmt, ...);
void spn_log_warn(const c8* fmt, ...);
void spn_log_error(const c8* fmt, ...);
void spn_log_debug(const c8* fmt, ...);

sp_str_t spn_log_level_to_str(spn_log_level_t level);
spn_log_level_t spn_trace_event_to_level(s32 kind);

// Trace macros: emit trace events through the event buffer.
// These write to the per-package build log, not to stderr.
#define spn_trace_debug(events, pkg, io, fmt, ...) \
  spn_event_buffer_push_ex(events, pkg, io, (spn_build_event_t){ \
    .kind = SPN_EVENT_TRACE_DEBUG, \
    .trace = { .message = sp_format(fmt, ##__VA_ARGS__), .file = sp_str_lit(__FILE__), .line = __LINE__ } \
  })

#define spn_trace_info(events, pkg, io, fmt, ...) \
  spn_event_buffer_push_ex(events, pkg, io, (spn_build_event_t){ \
    .kind = SPN_EVENT_TRACE_INFO, \
    .trace = { .message = sp_format(fmt, ##__VA_ARGS__), .file = sp_str_lit(__FILE__), .line = __LINE__ } \
  })

#define spn_trace_warn(events, pkg, io, fmt, ...) \
  spn_event_buffer_push_ex(events, pkg, io, (spn_build_event_t){ \
    .kind = SPN_EVENT_TRACE_WARN, \
    .trace = { .message = sp_format(fmt, ##__VA_ARGS__), .file = sp_str_lit(__FILE__), .line = __LINE__ } \
  })

#define spn_trace_error(events, pkg, io, fmt, ...) \
  spn_event_buffer_push_ex(events, pkg, io, (spn_build_event_t){ \
    .kind = SPN_EVENT_TRACE_ERROR, \
    .trace = { .message = sp_format(fmt, ##__VA_ARGS__), .file = sp_str_lit(__FILE__), .line = __LINE__ } \
  })

#endif
