#ifndef SPN_OP_STAGE_H
#define SPN_OP_STAGE_H

#include "spn/types.h"

#include "error/types.h"
#include "core/types.h"

spn_err_union_t resolve(spn_op_t* op);
spn_err_union_t sync(spn_op_t* op, bool* reresolve);
spn_err_union_t configure(spn_op_t* op);
spn_err_union_t spn_stage_sync_indexes(spn_op_t* op, spn_sync_request_t request);

#endif
