#ifndef SPN_CTX_CTX_H
#define SPN_CTX_CTX_H

#include "ctx/types.h"
#include "error/types.h"
#include "intern/types.h"

sp_intern_t* spn_ctx_get_intern();
spn_err_union_t spn_ctx_require_project(spn_ctx_t* ctx);
spn_index_info_t* spn_find_index(sp_str_t name);
void spn_ctx_cancel(spn_ctx_t* ctx);
bool spn_ctx_cancelled(spn_ctx_t* ctx);
bool spn_ctx_progress(spn_ctx_t* ctx, spn_progress_t* progress);

#endif
