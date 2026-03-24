#ifndef SPN_CTX_CTX_H
#define SPN_CTX_CTX_H

#include "ctx/types.h"
#include "intern.h"
#include "log/types.h"

sp_str_t spn_pkg_unit_get_include_dir(spn_pkg_unit_t* unit);

sp_intern_t* spn_ctx_get_intern(void);
spn_log_level_t spn_ctx_get_log_level(void);
sp_io_writer_t* spn_ctx_get_log_out(void);
sp_io_writer_t* spn_ctx_get_log_err(void);
sp_str_t spn_ctx_source_cache_root(void);
sp_str_t spn_ctx_build_cache_root(void);
sp_str_t spn_ctx_store_cache_root(void);
sp_str_t spn_ctx_project_root(void);
sp_da(spn_build_ctx_t*) spn_ctx_all_build_contexts(void);
void spn_ctx_push_target_source_event(spn_target_t* target, sp_str_t source);

#endif
