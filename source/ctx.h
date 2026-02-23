#ifndef SPN_CTX_H
#define SPN_CTX_H

#include "sp.h"
#include "spn.h"
#include "event.h"
#include "log.h"
#include "intern.h"
#include "pkg.h"
#include "resolve.h"

sp_str_t spn_app_project_dir(void);
sp_str_t spn_pkg_unit_get_include_dir(spn_pkg_unit_t* unit);

sp_intern_t* spn_ctx_get_intern(void);
spn_log_level_t spn_ctx_get_log_level(void);
sp_io_writer_t* spn_ctx_get_log_out(void);
sp_io_writer_t* spn_ctx_get_log_err(void);
sp_str_t spn_ctx_source_cache_root(void);
sp_str_t spn_ctx_build_cache_root(void);
sp_str_t spn_ctx_store_cache_root(void);
sp_str_t spn_ctx_project_root(void);
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
void spn_ctx_push_target_source_event(spn_target_t* target, sp_str_t source);
spn_pkg_t* spn_ctx_ensure_package(spn_pkg_req_t req);
spn_pkg_t* spn_ctx_root_package();
spn_resolver_t* spn_ctx_resolver();
void            spn_push_event(spn_build_event_kind_t kind);
void            spn_push_event_ex(spn_build_event_t event);

#endif
