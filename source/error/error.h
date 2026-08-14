#ifndef SPN_ERROR_H
#define SPN_ERROR_H

#include "spn/errors.h"

typedef struct spn_ctx_t spn_ctx_t;

spn_err_t spn_err_emit(spn_ctx_t* ctx, spn_err_union_t err);

#endif
