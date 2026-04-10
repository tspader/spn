#ifndef SPN_UNIT_TYPES_H
#define SPN_UNIT_TYPES_H

#include "intern/types.h"
#include "sp.h"
#include "spn.h"

#include "graph/types.h"
#include "pkg/types.h"
#include "target/types.h"
#include "external/tcc/types.h"

typedef struct spn_target_unit spn_target_unit_t;
typedef struct spn_session_t spn_session_t;
typedef struct spn_user_node_t spn_user_node_t;

struct spn_node_t {
  spn_pkg_unit_t* ctx;
  u32 index;
};

struct spn_node_ctx_t {
  void* user_data;
};

struct spn_user_node_t {
  spn_pkg_unit_t* pkg;
  sp_str_t tag;
  spn_node_fn_t fn;
  void* user_data;
  sp_da(sp_str_t) inputs;
  sp_da(sp_str_t) outputs;
  sp_da(spn_node_t*) deps;
  spn_bg_id_t id;
};


typedef struct {
  sp_str_t build;
  sp_str_t test;
  sp_str_t jsonl;
} spn_build_log_paths_t;

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
  spn_build_log_paths_t logs;
} spn_build_paths_t;

typedef struct spn_build_io_t {
  sp_io_writer_t build;
  sp_io_writer_t test;
  sp_io_writer_t jsonl;
} spn_build_io_t;

typedef struct {
  spn_session_t* session;
  spn_pkg_unit_t* package;
  spn_target_unit_t* target;

  struct {
    spn_bg_id_t source;
    spn_bg_id_t compile;
    spn_bg_id_t object;
  } nodes;

  struct {
    sp_str_t file;
    sp_str_t object;
    sp_str_t source;
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
  spn_session_t* session;
  spn_pkg_unit_t* pkg;
  spn_target_info_t* info;

  sp_da(spn_compile_unit_t*) objects;

  struct {
    sp_da(spn_target_unit_t*) target;
    sp_da(spn_target_unit_t*) package;
  } deps;

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

  spn_build_paths_t paths;
  spn_build_io_t logs;
};

struct spn_pkg_unit_t {
  sp_intern_str_t id;
  spn_session_t* session;
  spn_pkg_info_t* info;

  sp_str_om(spn_compile_unit_t) objects;
  sp_str_ht(spn_target_unit_t*) libs;
  sp_str_ht(spn_target_unit_t*) exes;
  sp_str_ht(spn_target_unit_t*) scripts;
  sp_str_ht(spn_target_unit_t*) tests;
  sp_da(spn_target_unit_t*) targets;

  struct {
    struct {
      spn_bg_id_t run;
      spn_bg_id_t stamp;
    } configure;
    spn_pkg_nodes_t build;
    sp_da(spn_user_node_t) user;
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

    spn_build_log_paths_t logs;

    sp_str_t manifest;
    sp_str_t script;
    sp_str_t source;
    sp_str_t work;
    sp_str_t generated;
    sp_str_t object;
    sp_str_t store;
    sp_str_t include;
    sp_str_t lib;
    sp_str_t bin;
    sp_str_t vendor;
  } paths;

  struct {
    sp_str_t build;
    sp_str_t test;
    sp_str_t jsonl;
    spn_build_io_t io;
  } logs;


  struct {
    u64 compile;
    u64 configure;
    u64 build;
    u64 package;
    u64 total;
  } time;

  spn_tcc_t* tcc;
  spn_configure_fn_t on_configure;
  spn_package_fn_t on_package;
};

typedef struct {
  spn_toolchain_kind_t kind;
  spn_toolchain_info_t info;
  spn_pkg_info_t* pkg;

  spn_session_t* session;
  sp_str_t url;
  spn_build_io_t logs;

  struct {
    spn_bg_id_t download;
    spn_bg_id_t stamp;
  } nodes;

  struct {
    sp_str_t work;
    sp_str_t store;
    sp_str_t stamp;
    spn_build_log_paths_t logs;
  } paths;

  spn_toolchain_launcher_t compiler;
  spn_toolchain_launcher_t linker;
  spn_toolchain_launcher_t archiver;
} spn_toolchain_unit_t;

static inline spn_user_node_t* spn_find_user_node(spn_node_t* node) {
  SP_ASSERT(node->index < sp_da_size(node->ctx->nodes.user));
  return &node->ctx->nodes.user[node->index];
}

#endif
