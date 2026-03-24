#ifndef SPN_LOG_LOG_H
#define SPN_LOG_LOG_H

#include "sp.h"
#include "log/types.h"

spn_log_level_t spn_log_level_from_str(sp_str_t str);
sp_str_t        spn_log_level_to_str(spn_log_level_t level);
void            spn_log_info(const c8* fmt, ...);
void            spn_log_warn(const c8* fmt, ...);
void            spn_log_error(const c8* fmt, ...);
void            spn_log_debug(const c8* fmt, ...);

#endif
