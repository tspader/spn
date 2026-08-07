#ifndef SPN_OP_STAGE_H
#define SPN_OP_STAGE_H

#include "error/types.h"
#include "core/types.h"

spn_err_union_t spn_op_resolve(spn_session_t* session);
spn_err_union_t spn_op_sync(spn_session_t* session, bool* reresolve);
spn_err_union_t spn_op_configure(spn_session_t* session);

#endif
