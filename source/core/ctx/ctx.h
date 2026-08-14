#ifndef SPN_CTX_CTX_H
#define SPN_CTX_CTX_H

#include "ctx/types.h"
#include "sp.h"
#include "spn/core.h"
#include "intern/types.h"

sp_intern_t* spn_ctx_get_intern();
spn_err_t spn_ctx_require_project(spn_ctx_t* ctx);
spn_index_info_t* spn_find_index(spn_ctx_t* ctx, sp_str_t name);

#endif
