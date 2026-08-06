#ifndef SPN_CTX_INIT_H
#define SPN_CTX_INIT_H

#include "ctx/types.h"
#include "error/types.h"

void      spn_ctx_init(spn_ctx_t* ctx);
spn_err_t spn_ctx_mount(spn_ctx_t* ctx);
spn_err_t spn_ctx_load_project(spn_ctx_t* ctx, sp_str_t dir, u32 refresh);
spn_err_t spn_ctx_open_session(spn_ctx_t* ctx, spn_session_config_t config);
void      spn_ctx_close(spn_ctx_t* ctx, bool ok);

#endif
