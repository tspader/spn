#ifndef SPN_OP_H
#define SPN_OP_H

#include "error/types.h"
#include "forward/types.h"
#include "op/types.h"
#include "session/types.h"

spn_err_union_t spn_op_exec(spn_ctx_t* ctx, spn_op_desc_t* desc);

spn_op_t* spn_op_start(spn_ctx_t* ctx, spn_op_desc_t desc);
bool      spn_op_poll(spn_op_t* op);
spn_err_union_t spn_op_result(spn_op_t* op);

spn_err_union_t spn_op_add(spn_ctx_t* ctx, spn_add_request_t* request);
spn_err_union_t spn_op_clean(spn_session_t* session, bool whole_build);
spn_err_union_t spn_op_publish(spn_ctx_t* ctx, spn_publish_request_t* request);
spn_err_union_t spn_op_sync_indexes(spn_ctx_t* ctx, spn_index_refresh_t refresh);
spn_err_union_t spn_op_run_target(spn_session_t* session, spn_target_unit_t* unit);

#endif
