#ifndef SPN_CTX_H
#define SPN_CTX_H

#include "sp.h"
#include "spn.h"
#include "log.h"
#include "ordered_map.h"

sp_str_t spn_app_project_dir(void);
sp_str_t spn_pkg_unit_get_include_dir(spn_pkg_unit_t* unit);

sp_intern_t* spn_ctx_get_intern(void);
spn_log_level_t spn_ctx_get_log_level(void);
sp_io_writer_t* spn_ctx_get_log_out(void);
sp_io_writer_t* spn_ctx_get_log_err(void);
sp_str_t spn_ctx_build_source_dir(spn_build_ctx_t* build);
sp_str_t spn_ctx_build_work_dir(spn_build_ctx_t* build);
sp_str_t spn_ctx_build_store_dir(spn_build_ctx_t* build);
sp_str_t spn_ctx_build_include_dir(spn_build_ctx_t* build);
sp_str_t spn_ctx_build_lib_dir(spn_build_ctx_t* build);
spn_linkage_t spn_ctx_build_linkage(spn_build_ctx_t* build);
spn_build_mode_t spn_ctx_build_mode(spn_build_ctx_t* build);
sp_ps_output_t spn_ctx_build_subprocess(spn_build_ctx_t* build, sp_ps_config_t cfg);
sp_da(sp_str_t) spn_ctx_build_lib_entries(spn_build_ctx_t* build);
sp_da(spn_build_ctx_t*) spn_ctx_all_build_contexts(void);

#endif
