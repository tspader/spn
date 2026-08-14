#ifndef SPN_OP_OP_H
#define SPN_OP_OP_H

#include "op/types.h"

void spn_op_thread_start(spn_ctx_t* ctx);
void spn_op_thread_stop(spn_ctx_t* ctx);

void spn_op_cancel(spn_op_t* op);
bool spn_op_cancelled(spn_op_t* op);

spn_err_t spn_op_add(spn_op_t* op);
spn_err_t spn_op_publish(spn_op_t* op);
spn_err_t spn_op_sync_indexes(spn_op_t* op);
spn_err_t spn_op_scaffold(spn_op_t* op);
spn_err_t spn_op_build(spn_op_t* op);
spn_err_t spn_op_test(spn_op_t* op);
spn_err_t spn_op_clean(spn_op_t* op);
spn_err_t spn_op_clean_profile(spn_op_t* op);

#endif
