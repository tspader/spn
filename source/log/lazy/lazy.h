#ifndef SPN_LOG_LAZY_H
#define SPN_LOG_LAZY_H

#include "log/lazy/types.h"

void spn_lazy_log_init(spn_lazy_log_t* log, sp_str_t path);
void spn_lazy_log_close(spn_lazy_log_t* log);

#endif
