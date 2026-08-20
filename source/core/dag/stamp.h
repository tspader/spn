#ifndef SPN_DAG_STAMP_H
#define SPN_DAG_STAMP_H

#include "sp.h"
#include "spn/core.h"

#define SPN_DAG_STAMP_TRUST_ALL ((sp_sys_timespec_t) { .tv_sec = SP_LIMIT_S64_MAX })

bool is_timestamp_fenced(sp_sys_timespec_t fence, sp_sys_timespec_t mtime);
spn_err_t cache_timestamp_fence(sp_str_t dir, sp_sys_timespec_t* fence);

#endif
