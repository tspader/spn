#ifndef SPN_HOST_H
#define SPN_HOST_H

#ifdef SPN_SPN_H
  #error "spn/host.h is the host embedding API; it cannot share a translation unit with the guest include/spn.h"
#endif

#include "sp.h"
#include "core.h"
#include "types.h"
#include "events.h"

SP_BEGIN_EXTERN_C()

typedef struct spn_ctx_t spn_ctx_t;
typedef struct spn_session_t spn_session_t;
typedef struct spn_op_t spn_op_t;

spn_ctx_t* spn_ctx_new(spn_wake_fn_t wake, void* wake_data);
spn_err_t spn_ctx_open(spn_ctx_t* ctx, spn_open_request_t request);
spn_err_t spn_ctx_open_session(spn_ctx_t* ctx, const spn_session_config_t* config, spn_session_t** session);
void spn_ctx_close(spn_ctx_t* ctx, bool ok);
bool spn_ctx_progress(spn_ctx_t* ctx, spn_progress_t* progress);
spn_event_t* spn_ctx_drain(spn_ctx_t* ctx);
sp_str_t spn_ctx_project_dir(spn_ctx_t* ctx);
sp_str_t spn_ctx_cache_dir(spn_ctx_t* ctx);
spn_path_t spn_ctx_collapse_path(spn_ctx_t* ctx, sp_str_t path);
bool spn_get_index(spn_ctx_t* ctx, sp_str_t name, spn_index_desc_t* index);
spn_index_arr_t spn_get_indexes(sp_mem_t mem, spn_ctx_t* ctx);
spn_op_t* spn_add_dependency(spn_ctx_t* ctx, spn_add_request_t request);
spn_op_t* spn_publish(spn_ctx_t* ctx, spn_publish_request_t request);
spn_op_t* spn_sync_indexes(spn_ctx_t* ctx, spn_sync_request_t request);
spn_op_t* spn_scaffold_project(spn_ctx_t* ctx, spn_scaffold_request_t request);
spn_op_t* spn_build(spn_session_t* session);
spn_op_t* spn_run_tests(spn_session_t* session);
spn_op_t* spn_clean(spn_ctx_t* ctx);
spn_op_t* spn_clean_profile(spn_session_t* session);
spn_op_t* spn_op_new(spn_ctx_t* ctx, spn_session_t* session, spn_op_kind_t kind);
void spn_op_submit(spn_op_t* op);
void spn_op_cancel(spn_op_t* op);
bool spn_op_cancelled(spn_op_t* op);
bool spn_op_done(spn_op_t* op);
spn_op_result_t spn_op_result(spn_op_t* op);
void spn_op_free(spn_op_t* op);
spn_err_t spn_scaffold_check(spn_ctx_t* ctx, spn_scaffold_request_t request);
spn_err_t spn_triple_parse(sp_str_t str, spn_triple_t* triple);
spn_triple_t spn_triple_from_str(sp_str_t str);
spn_triple_t spn_triple_host();
spn_triple_t spn_triple_merge(spn_triple_t base, spn_triple_t partial);
sp_str_t spn_triple_to_str(sp_mem_t mem, spn_triple_t triple);
sp_str_t spn_triple_exe_file_name(sp_mem_t mem, spn_triple_t triple, sp_str_t name);
sp_str_t spn_triple_lib_file_name(sp_mem_t mem, spn_triple_t triple, sp_str_t name, sp_os_lib_kind_t kind);
spn_arch_t spn_arch_from_str(sp_str_t str);
spn_os_t spn_os_from_str(sp_str_t str);
sp_str_t spn_os_to_str(spn_os_t os);
spn_abi_t spn_abi_from_str(sp_str_t str);
sp_str_t spn_abi_to_str(spn_abi_t abi);
spn_mode_t spn_mode_from_str(sp_str_t str);
spn_opt_level_t spn_opt_level_from_str(sp_str_t str);
spn_sanitizer_t spn_sanitizer_from_str(sp_str_t str);
bool spn_sanitizer_set_has_conflict(spn_sanitizer_set_t set);
sp_str_t spn_sanitizer_set_to_str(sp_mem_t mem, spn_sanitizer_set_t set);
sp_str_t spn_semver_to_str(sp_mem_t mem, spn_semver_t version);
sp_str_t spn_index_kind_to_str(spn_index_kind_t kind);
sp_str_t spn_index_protocol_to_str(spn_index_protocol_t protocol);

SP_END_EXTERN_C()

#endif
