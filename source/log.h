#ifndef SPN_LOG_H
#define SPN_LOG_H

#include "sp.h"

typedef enum {
  SPN_LOG_LEVEL_ERROR,
  SPN_LOG_LEVEL_WARN,
  SPN_LOG_LEVEL_INFO,
  SPN_LOG_LEVEL_DEBUG,
} spn_log_level_t;

void spn_log_info(const c8* fmt, ...);
void spn_log_warn(const c8* fmt, ...);
void spn_log_error(const c8* fmt, ...);
void spn_log_debug(const c8* fmt, ...);
void spn_ctx_tui(const c8* fmt, ...);

#endif
