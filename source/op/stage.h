#ifndef SPN_OP_STAGE_H
#define SPN_OP_STAGE_H

#include "spn/types.h"

#include "error/types.h"
#include "core/types.h"

spn_err_union_t spn_stage_resolve(spn_session_t* session);
spn_err_union_t spn_stage_sync(spn_session_t* session, bool* reresolve);
spn_err_union_t spn_stage_configure(spn_session_t* session);
spn_err_union_t spn_stage_sync_indexes(spn_ctx_t* ctx, spn_sync_request_t request);

#endif
