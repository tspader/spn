#ifndef SPN_UNIT_BUILD_H
#define SPN_UNIT_BUILD_H

#include "err.h"
#include "log/types.h"
#include "unit/types.h"

void spn_build_ctx_log(spn_build_io_t* logs, sp_str_t message);
void spn_build_ctx_log_ex(spn_build_io_t* logs, spn_log_level_t level, u64 thread_id, sp_str_t source, sp_str_t message);
sp_str_t spn_build_ctx_resolve_dir(const spn_build_ctx_t* ctx, spn_pkg_dir_t kind, sp_str_t sub);
sp_str_t spn_build_ctx_get_dir(const spn_build_ctx_t* ctx, spn_pkg_dir_t kind);
sp_str_t spn_build_ctx_get_include_dir(spn_build_ctx_t* ctx);
sp_str_t spn_build_ctx_get_lib_dir(spn_build_ctx_t* ctx);
sp_str_t spn_build_ctx_get_lib_path(spn_build_ctx_t* ctx, spn_target_t* lib_target);
sp_str_t spn_build_ctx_get_rpath(spn_build_ctx_t* ctx);
sp_str_t spn_build_ctx_get_build_log_name(spn_build_ctx_t* ctx);
sp_str_t spn_build_ctx_get_test_log_name(spn_build_ctx_t* ctx);
sp_str_t spn_build_ctx_get_jsonl_log_name(spn_build_ctx_t* ctx);
sp_str_t spn_cache_dir_kind_to_path(spn_pkg_dir_t kind);
sp_str_t spn_ctx_build_source_dir(spn_build_ctx_t* build);
sp_str_t spn_ctx_build_work_dir(spn_build_ctx_t* build);
sp_str_t spn_ctx_build_store_dir(spn_build_ctx_t* build);
sp_str_t spn_ctx_build_include_dir(spn_build_ctx_t* build);
sp_str_t spn_ctx_build_lib_dir(spn_build_ctx_t* build);
spn_linkage_t spn_ctx_build_linkage(spn_build_ctx_t* build);
sp_ps_output_t spn_ctx_build_subprocess(spn_build_ctx_t* build, sp_ps_config_t cfg);
sp_da(sp_str_t) spn_ctx_build_lib_entries(spn_build_ctx_t* build);

#endif
