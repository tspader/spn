#ifndef SPN_CTX_CTX_H
#define SPN_CTX_CTX_H

#include "ctx/types.h"
#include "intern/types.h"
#include "log/types.h"

sp_str_t spn_pkg_unit_get_include_dir(spn_pkg_unit_t* unit);

sp_intern_t* spn_ctx_get_intern(void);
spn_log_level_t spn_ctx_get_log_level(void);
sp_io_writer_t* spn_ctx_get_log_out(void);
sp_io_writer_t* spn_ctx_get_log_err(void);
sp_str_t spn_ctx_project_root(void);
spn_index_info_t* spn_find_index(sp_str_t name);

#endif
