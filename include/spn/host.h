#ifndef SPN_HOST_H
#define SPN_HOST_H

#ifdef SPN_SPN_H
  #error "spn/host.h is the host embedding API; it cannot share a translation unit with the guest include/spn.h"
#endif

#include "sp.h"
#include "core.h"
#include "session.h"

SP_BEGIN_EXTERN_C()

typedef struct spn_ctx_t spn_ctx_t;
typedef struct spn_session_t spn_session_t;
typedef struct spn_target spn_target_t;

typedef struct {
  u32 total;
  u32 completed;
  u32 hits;
  u32 misses;
} spn_progress_t;

typedef struct {
  sp_str_t name;
  sp_str_t version;
  spn_dep_kind_t kind;
} spn_add_request_t;

typedef struct {
  bool force;
  sp_str_t only;
} spn_sync_request_t;

typedef struct {
  sp_str_t index;
  sp_str_t url;
  sp_str_t revision;
  bool allow_dirty;
} spn_publish_request_t;

typedef struct {
  sp_str_t dir;
  sp_str_t name;
  bool bare;
} spn_scaffold_request_t;

typedef struct {
  sp_str_t dir;
  u32 index_refresh_seconds;
  bool project_optional;
} spn_open_request_t;

typedef struct {
  sp_str_t name;
  spn_index_kind_t kind;
  spn_index_protocol_t protocol;
  sp_str_t source;
  sp_str_t location;
} spn_index_desc_t;

spn_ctx_t* spn_ctx_new();
spn_err_t spn_ctx_open(spn_ctx_t* ctx, spn_open_request_t request);
spn_err_t spn_ctx_open_session(spn_ctx_t* ctx, const spn_session_config_t* config, spn_session_t** session);
void spn_ctx_close(spn_ctx_t* ctx, bool ok);
void spn_ctx_cancel(spn_ctx_t* ctx);
bool spn_ctx_cancelled(spn_ctx_t* ctx);
bool spn_ctx_progress(spn_ctx_t* ctx, spn_progress_t* progress);
bool spn_ctx_find_target(spn_ctx_t* ctx, sp_str_t name, spn_target_kind_t* kind);
sp_str_t spn_ctx_project_dir(spn_ctx_t* ctx);
sp_str_t spn_ctx_cache_dir(spn_ctx_t* ctx);
bool spn_ctx_find_index(spn_ctx_t* ctx, sp_str_t name, spn_index_desc_t* index);
sp_da(spn_index_desc_t) spn_ctx_indexes(sp_mem_t mem, spn_ctx_t* ctx);
spn_err_t spn_op_add(spn_ctx_t* ctx, spn_add_request_t request);
spn_err_t spn_op_publish(spn_ctx_t* ctx, spn_publish_request_t request);
spn_err_t spn_op_publish_dry(spn_ctx_t* ctx, spn_publish_request_t request, sp_mem_t mem, sp_str_t* json);
spn_err_t spn_op_sync_indexes(spn_ctx_t* ctx, spn_sync_request_t request);
spn_err_t spn_op_scaffold(spn_ctx_t* ctx, spn_scaffold_request_t request, sp_mem_t mem, sp_da(sp_str_t)* files);
spn_err_t spn_op_scaffold_check(spn_ctx_t* ctx, spn_scaffold_request_t request);
spn_err_t spn_op_build(spn_session_t* session);
spn_err_t spn_op_test(spn_session_t* session);
spn_err_t spn_op_run_target(spn_session_t* session, spn_target_t* target);
spn_err_t spn_op_clean(spn_ctx_t* ctx);
spn_err_t spn_op_clean_profile(spn_session_t* session);
spn_target_t* spn_session_find_target(spn_session_t* session, sp_str_t name);
sp_str_t spn_target_name(spn_target_t* target);
sp_str_t spn_target_path(sp_mem_t mem, spn_target_t* target);
const c8* spn_err_to_str(spn_err_t err);
spn_err_t spn_triple_parse(sp_str_t str, spn_triple_t* triple);
spn_triple_t spn_triple_merge(spn_triple_t base, spn_triple_t partial);
sp_str_t spn_triple_to_str(sp_mem_t mem, spn_triple_t triple);
spn_arch_t spn_arch_from_str(sp_str_t str);
spn_os_t spn_os_from_str(sp_str_t str);
spn_abi_t spn_abi_from_str(sp_str_t str);
spn_build_mode_t spn_build_mode_from_str(sp_str_t str);
spn_opt_level_t spn_opt_level_from_str(sp_str_t str);
spn_sanitizer_t spn_sanitizer_from_str(sp_str_t str);
bool spn_sanitizer_set_has_conflict(spn_sanitizer_set_t set);
sp_str_t spn_index_kind_to_str(spn_index_kind_t kind);
sp_str_t spn_index_protocol_to_str(spn_index_protocol_t protocol);

SP_END_EXTERN_C()

#endif
