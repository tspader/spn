#ifndef SPN_UNIT_TYPES_H
#define SPN_UNIT_TYPES_H

#include "sp.h"
#include "spn.h"

#include "graph/types.h"
#include "node.h"
#include "pkg/types.h"
#include "profile/types.h"
#include "tui/types.h"
#include "external/tcc.h"

typedef struct spn_target_unit spn_target_unit_t;
typedef struct spn_session_t spn_session_t;

typedef enum {
  SPN_HOOK_CONFIGURE,
  SPN_HOOK_PACKAGE,
} spn_hook_t;

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
    sp_str_t jsonl;
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
  sp_io_writer_t jsonl;
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

typedef struct {
  spn_bg_id_t build;
  spn_bg_id_t bin;
} spn_target_nodes_t;

typedef struct {
  spn_bg_id_t manifest;
  spn_bg_id_t script;
  spn_bg_id_t package;
  spn_bg_id_t main;
  spn_bg_id_t exit;
  struct {
    spn_bg_id_t package;
    spn_bg_id_t main;
    spn_bg_id_t exit;
  } stamp;
  sp_da(spn_bg_id_t) user;
} spn_pkg_nodes_t;

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
    struct {
      spn_bg_id_t run;
      spn_bg_id_t object;
      spn_bg_id_t header;
    } embed;
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

#endif
