#ifndef SPN_ERROR_H
#define SPN_ERROR_H

#include "error/types.h"

const c8* spn_err_to_str(spn_err_t err);
spn_err_t spn_err_emit(spn_ctx_t* ctx, spn_err_union_t err);

#endif
