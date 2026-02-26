#ifndef SPN_UNIT_H
#define SPN_UNIT_H

#include "sp.h"
#include "spn.h"

#include "err.h"
#include "graph.h"
#include "node.h"
#include "pkg.h"
#include "profile.h"
#include "semver.h"
#include "tui.h"
#include "external/tcc.h"

typedef struct spn_target_unit spn_target_unit_t;
typedef struct spn_session_t spn_session_t;

typedef struct {
  u64 compile;
  u64 configure;
  u64 build;
  u64 package;
  u64 total;
} spn_build_time_t;

typedef struct {
  sp_str_t name;
  spn_pkg_t* package;
  spn_session_t* session;
  spn_linkage_t linkage;
  struct {
    sp_str_t source;
    sp_str_t store;
    sp_str_t work;
  } paths;
} spn_build_ctx_config_t;

typedef struct {
  u64 build_id;
  spn_pkg_metadata_t metadata;
  spn_build_ctx_config_t ctx;
} spn_pkg_unit_config_t;

typedef struct {
  sp_str_t source;
  sp_str_t work;
  sp_str_t generated;
  sp_str_t object;
  sp_str_t store;
  sp_str_t include;
  sp_str_t lib;
  sp_str_t bin;
  sp_str_t vendor;
  struct {
    sp_str_t build;
    sp_str_t test;
  } logs;
} spn_build_paths_t;

typedef struct {
  sp_str_t source;
  sp_str_t work;
  sp_str_t store;
} spn_bp_config_t;

typedef struct spn_build_io_t {
  sp_io_writer_t build;
  sp_io_writer_t test;
} spn_build_io_t;

struct spn_build_ctx {
  sp_str_t name;
  spn_session_t* session;
  spn_pkg_t* pkg;
  spn_profile_t* profile;
  spn_linkage_t linkage;
  spn_build_paths_t paths;
  spn_build_io_t logs;
  sp_mem_arena_t* arena;
  sp_str_t error;
  sp_da(sp_ps_config_t) commands;
  sp_ps_t ps;
};

typedef struct {
  sp_str_t name;
  spn_target_unit_t* target;
  spn_profile_t* profile;
  spn_session_t* session;
  spn_pkg_t* pkg;

  struct {
    spn_bg_id_t source;
    spn_bg_id_t compile;
    spn_bg_id_t object;
  } nodes;
  struct {
    sp_str_t source;
    sp_str_t object;
  } paths;
} spn_compile_unit_t;

struct spn_target_unit {
  spn_build_paths_t paths;
  spn_build_io_t logs;
  spn_build_ctx_t ctx;
  spn_session_t* session;
  spn_pkg_t* pkg;
  spn_target_t* info;
  spn_cc_t* cc;

  sp_da(spn_compile_unit_t*) objects;

  struct {
    spn_bg_id_t output;
    spn_bg_id_t link;
    sp_da(spn_bg_id_t) source;
  } nodes;
};

struct spn_pkg_unit_t {
  spn_build_ctx_t ctx;
  spn_pkg_metadata_t metadata;
  sp_om(spn_target_unit_t) targets;
  sp_om(spn_compile_unit_t) objects;

  struct {
    struct {
      spn_bg_id_t run;
      spn_bg_id_t stamp;
    } configure;
    spn_pkg_nodes_t build;
    sp_da(spn_user_node_t) all;
    sp_str_ht(spn_bg_id_t) files;
  } nodes;

  struct {
    struct {
      sp_str_t dir;
      sp_str_t build;
      sp_str_t configure;
      sp_str_t package;
      sp_str_t main;
      sp_str_t exit;
    } stamp;
  } paths;

  spn_build_time_t time;

  spn_tcc_t* tcc;
  spn_build_fn_t on_configure;
  spn_build_fn_t on_package;
};

spn_user_node_t* spn_find_user_node(spn_node_t* node);

spn_build_ctx_t spn_build_ctx_make(spn_build_ctx_config_t cfg);
void spn_build_ctx_init(spn_build_ctx_t* ctx, spn_build_ctx_config_t cfg);
void spn_build_ctx_log(spn_build_io_t* logs, sp_str_t message);
sp_ps_output_t spn_build_ctx_subprocess(spn_build_ctx_t* ctx, sp_ps_config_t cfg);
sp_str_t spn_build_ctx_resolve_dir(const spn_build_ctx_t* b, spn_pkg_dir_t kind, sp_str_t sub);
sp_str_t spn_build_ctx_get_dir(const spn_build_ctx_t* c, spn_pkg_dir_t kind);
sp_str_t spn_build_ctx_get_include_dir(spn_build_ctx_t* ctx);
sp_str_t spn_build_ctx_get_lib_dir(spn_build_ctx_t* ctx);
sp_str_t spn_build_ctx_get_lib_path(spn_build_ctx_t* ctx, spn_target_t* lib_target);
sp_str_t spn_build_ctx_get_rpath(spn_build_ctx_t* ctx);
sp_str_t spn_build_ctx_get_build_log_name(spn_build_ctx_t* ctx);
sp_str_t spn_build_ctx_get_test_log_name(spn_build_ctx_t* ctx);
void spn_pkg_unit_init(spn_pkg_unit_t* ctx, spn_pkg_unit_config_t config);
void spn_pkg_unit_write_stamp(spn_pkg_unit_t* ctx, sp_str_t path);
spn_err_t spn_pkg_unit_sync_remote(spn_pkg_unit_t* dep);
spn_err_t spn_pkg_unit_sync_local(spn_pkg_unit_t* dep);
spn_err_t spn_pkg_unit_call_hook(spn_pkg_unit_t* ctx, spn_build_fn_t fn);
spn_err_t spn_pkg_unit_run_configure_hook(spn_pkg_unit_t* ctx);
spn_err_t spn_pkg_unit_run_package_hook(spn_pkg_unit_t* ctx);
sp_str_t  spn_pkg_unit_get_node_stamp_file(spn_pkg_unit_t* ctx, spn_user_node_t* node);
void spn_pkg_unit_add_target(spn_pkg_unit_t* pkg, spn_target_t* target);
sp_str_t spn_cache_dir_kind_to_path(spn_pkg_dir_t kind);

#endif
