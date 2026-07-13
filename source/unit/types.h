#ifndef SPN_UNIT_TYPES_H
#define SPN_UNIT_TYPES_H

#include "forward/types.h"
#include "sp.h"
#include "spn.h"

#include "graph/types.h"
#include "compiler/types.h"
#include "filter/types.h"
#include "intern/types.h"
#include "pkg/types.h"
#include "profile/types.h"
#include "external/wasm/types.h"
#include "log/lazy/types.h"

typedef u32 spn_build_unit_id_t;
typedef u32 spn_toolchain_unit_id_t;

struct spn_build_unit_t {
  spn_build_unit_id_t id;
  spn_profile_info_t profile;
  spn_toolchain_unit_t* toolchain;
  spn_symbol_visibility_t visibility;
  u32 dep_kinds;
  sp_da(sp_str_t) include;
  sp_da(spn_pkg_unit_t*) packages;
  struct {
    sp_str_t root;
  } paths;
};

#define spn_dep_kind_bit(kind) (1u << (kind))

typedef struct {
  spn_target_selection_t targets;
} spn_compile_request_t;

typedef struct SP_ALIGNED {
  spn_pkg_id_t pkg;
  spn_build_unit_id_t ctx;
} spn_pkg_unit_id_t;

typedef struct SP_ALIGNED {
  spn_pkg_unit_id_t pkg;
  sp_intern_id_t target;
} spn_target_unit_id_t;

typedef struct SP_ALIGNED {
  spn_target_unit_id_t target;
  sp_intern_id_t source;
} spn_compile_unit_id_t;

typedef struct {
  spn_build_unit_t* build;
  spn_target_selection_t selection;
  sp_da(spn_target_unit_id_t) roots;
} spn_build_plan_t;

typedef struct {
  spn_pkg_unit_t* unit;
  spn_dep_kind_t kind;
  bool private;
} spn_pkg_dep_t;


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
  sp_str_t fn;
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
  sp_str_t recipe;
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
  spn_lazy_log_t build;
  spn_lazy_log_t test;
  spn_lazy_log_t jsonl;
} spn_build_io_t;

typedef struct {
  sp_str_t program;
  sp_da(sp_str_t) args;
  sp_str_t cwd;
} spn_invocation_t;

typedef struct {
  sp_str_t source;
  sp_str_t output;
  spn_invocation_t invocation;
} spn_compile_command_t;

typedef struct {
  spn_compile_unit_id_t id;
  spn_session_t* session;
  spn_pkg_unit_t* package;
  spn_target_unit_t* target;
  spn_lang_t lang;
  spn_invocation_t invocation;

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
  sp_str_t from;
  sp_str_t to;
  spn_bg_id_t input;
} spn_stage_file_t;

typedef struct {
  sp_str_t dir;
  sp_da(spn_stage_file_t) files;
} spn_stage_unit_t;

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
    spn_bg_id_t profile;
  } stamp;
  sp_da(spn_bg_id_t) user;
} spn_pkg_nodes_t;

struct spn_target_unit {
  spn_target_unit_id_t id;
  spn_session_t* session;
  spn_pkg_unit_t* pkg;
  spn_build_unit_t* build;
  spn_target_info_t* info;
  spn_cc_output_kind_t kind;
  spn_linkage_t lib_kind;
  spn_invocation_t invocation;

  sp_da(spn_compile_unit_t*) objects;

  struct {
    sp_da(spn_target_unit_t*) target;
    sp_da(spn_pkg_unit_t*) package;
  } deps;

  struct {
    spn_bg_id_t output;
    spn_bg_id_t link;
    struct {
      spn_bg_id_t run;
      spn_bg_id_t object;
      spn_bg_id_t header;
    } embed;
  } nodes;

  spn_build_paths_t paths;
  spn_build_io_t logs;
};

/////////////
// PACKAGE //
/////////////
struct spn_pkg_unit_t {
  spn_pkg_unit_id_t id;
  spn_build_unit_t* build;
  spn_pkg_unit_t* program;
  spn_session_t* session;
  spn_pkg_info_t* info;
  spn_pkg_source_t source;
  u32 materialized_dep_kinds;

  struct {
    struct {
      spn_target_info_t* info;
      spn_target_unit_t* target;
    } configure;
    struct {
      spn_target_info_t* info;
      spn_target_unit_t* target;
    } build;
  } meta;

  sp_da(spn_pkg_dep_t) deps;
  sp_da(spn_target_unit_t*) libs;
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
      sp_str_t configure;
      sp_str_t package;
      sp_str_t main;
      sp_str_t exit;
      sp_str_t profile;
    } stamp;

    spn_build_log_paths_t logs;

    sp_str_t manifest;
    sp_str_t script;
    sp_str_t recipe;
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

  struct {
    spn_wasm_script_t configure;
    spn_wasm_script_t build;
  } wasm;

  sp_atomic_s32_t compile_announced;
};

struct spn_toolchain_unit_t {
  spn_toolchain_unit_id_t id;
  spn_toolchain_info_t* info;
  spn_triple_t host;
  spn_opt_artifact_t artifact;
  sp_str_t root;
  spn_toolchain_launcher_t compiler;
  spn_toolchain_launcher_t cxx;
  spn_toolchain_launcher_t linker;
  spn_toolchain_launcher_t archiver;
  sp_hash_t identity;
  spn_build_io_t logs;
};

static inline spn_user_node_t* spn_find_user_node(spn_node_t* node) {
  SP_ASSERT(node->index < sp_da_size(node->ctx->nodes.user));
  return &node->ctx->nodes.user[node->index];
}

#endif
