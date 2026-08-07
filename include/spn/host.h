#ifndef SPN_HOST_H
#define SPN_HOST_H

#ifdef SPN_SPN_H
  #error "spn/host.h is the host embedding API; it cannot share a translation unit with the guest include/spn.h"
#endif

#include "sp.h"
#include "core.h"

typedef struct spn_ctx_t spn_ctx_t;
typedef struct spn_session_t spn_session_t;
typedef struct spn_session_config_t spn_session_config_t;
typedef struct spn_project_t spn_project_t;
typedef struct spn_target spn_target_t;
typedef struct spn_index_info spn_index_info_t;
typedef struct spn_pkg_info spn_pkg_info_t;
typedef struct sp_intern_t sp_intern_t;

typedef struct {
  u32 total;
  u32 completed;
  u32 hits;
  u32 misses;
} spn_progress_t;

typedef enum {
  SPN_ADD_DEP_PACKAGE,
  SPN_ADD_DEP_TEST,
  SPN_ADD_DEP_BUILD,
} spn_add_dep_t;

typedef struct {
  sp_str_t key;
  sp_str_t requested;
  spn_add_dep_t dep;
} spn_add_request_t;

typedef struct {
  bool force;
  sp_str_t only;
} spn_index_refresh_t;

typedef struct {
  sp_str_t index;
  sp_str_t url;
  sp_str_t revision;
  bool allow_dirty;
} spn_publish_request_t;

typedef struct {
  s32 code;
  sp_str_t out;
  sp_str_t err;
  u64 time;
} spn_test_run_t;

void      spn_ctx_init(spn_ctx_t* ctx);
spn_err_t spn_ctx_mount(spn_ctx_t* ctx);
spn_err_t spn_ctx_load_project(spn_ctx_t* ctx, sp_str_t dir, u32 refresh);
spn_err_t spn_ctx_open_session(spn_ctx_t* ctx, const spn_session_config_t* config);
void      spn_ctx_close(spn_ctx_t* ctx, bool ok);
void      spn_ctx_cancel(spn_ctx_t* ctx);
bool      spn_ctx_cancelled(spn_ctx_t* ctx);
bool      spn_ctx_progress(spn_ctx_t* ctx, spn_progress_t* progress);

spn_err_t spn_op_build(spn_session_t* session);
spn_err_t spn_op_add(spn_ctx_t* ctx, spn_add_request_t request);
spn_err_t spn_op_clean(spn_session_t* session, bool whole_build);
spn_err_t spn_op_publish(spn_ctx_t* ctx, spn_publish_request_t request);
spn_err_t spn_op_publish_dry(spn_ctx_t* ctx, spn_publish_request_t request, sp_mem_t mem, sp_str_t* json);
spn_err_t spn_op_sync_indexes(spn_ctx_t* ctx, spn_index_refresh_t refresh);
spn_err_t spn_op_run_target(spn_session_t* session, spn_target_t* target);
spn_err_t spn_op_run_test(spn_session_t* session, spn_target_t* target, spn_test_run_t* run);

u32               spn_session_num_targets(spn_session_t* session);
spn_target_t*     spn_session_target_at(spn_session_t* session, u32 index);
spn_target_t*     spn_session_script_root(spn_session_t* session);
sp_str_t          spn_target_name(spn_target_t* target);
spn_target_kind_t spn_target_kind(spn_target_t* target);
spn_pkg_info_t*   spn_target_pkg(spn_target_t* target);
sp_str_t          spn_target_staged_path(sp_mem_t mem, spn_target_t* target);

const c8* spn_err_to_str(spn_err_t err);

bool spn_project_has_script(spn_project_t* project, sp_intern_t* intern, sp_str_t name);

spn_index_info_t* spn_find_index(spn_ctx_t* ctx, sp_str_t name);
sp_str_t spn_index_source(spn_index_info_t* index);

spn_err_t    spn_triple_parse(sp_str_t str, spn_triple_t* triple);
spn_triple_t spn_triple_merge(spn_triple_t base, spn_triple_t partial);
sp_str_t     spn_triple_to_str(sp_mem_t mem, spn_triple_t triple);

spn_arch_t       spn_arch_from_str(sp_str_t str);
spn_os_t         spn_os_from_str(sp_str_t str);
spn_abi_t        spn_abi_from_str(sp_str_t str);
spn_build_mode_t spn_build_mode_from_str(sp_str_t str);
spn_opt_level_t  spn_opt_level_from_str(sp_str_t str);
spn_sanitizer_t  spn_sanitizer_from_str(sp_str_t str);
sp_str_t         spn_sanitizer_set_to_str(sp_mem_t mem, spn_sanitizer_set_t set);
bool             spn_sanitizer_set_conflicting(spn_sanitizer_set_t set);

#endif
