#ifndef SPN_DAG_STAMP_H
#define SPN_DAG_STAMP_H

#include "sp.h"
#include "spn/core.h"
#include "dag/types.h"

#define SPN_DAG_STAMP_TRUST_ALL ((sp_sys_timespec_t) { .tv_sec = SP_LIMIT_S64_MAX })

bool      spn_dag_stamp_fenced(sp_sys_timespec_t fence, sp_sys_timespec_t mtime);
spn_err_t spn_dag_stamp_probe(sp_str_t dir, sp_sys_timespec_t* fence);
spn_err_t spn_dag_stamp_admit(spn_dag_stamp_t* stamp, sp_sys_timespec_t mtime, bool* admit);

#endif
