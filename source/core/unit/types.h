#ifndef SPN_UNIT_TYPES_H
#define SPN_UNIT_TYPES_H

#include "core/types.h"
#include "compiler/types.h"
#include "paths/types.h"
#include "sp.h"
#include "spn/core.h"
#include "spn/types.h"
#include "intern/types.h"
#include "pkg/types.h"
#include "macro/macro.h"
#include "profile/types.h"
#include "target/types.h"
#include "external/wasm/types.h"

typedef struct {
  spn_triple_t host;
  spn_profile_info_t profile;
  spn_toolchain_role_t role;
} spn_build_config_t;

struct spn_build_unit_t {
  spn_build_id_t id;
  spn_profile_info_t profile;
  spn_toolchain_unit_t* toolchain;
  sp_da(spn_path_t) include;
  sp_da(sp_str_t) define;
  sp_da(spn_pkg_unit_t*) packages;
  struct {
    spn_path_t root;
  } paths;
};

#define spn_dep_kind_bit(kind) (1u << (kind))

SPN_PACK_PUSH
typedef struct {
  spn_pkg_id_t pkg;
  spn_build_id_t build;
} spn_pkg_unit_id_t;

typedef struct {
  spn_pkg_unit_id_t pkg;
  sp_intern_id_t target;
  spn_target_kind_t kind;
} spn_target_unit_id_t;

typedef struct {
  spn_target_unit_id_t target;
  sp_intern_id_t source;
} spn_compile_unit_id_t;
SPN_PACK_POP

_Static_assert(
  sizeof(spn_pkg_unit_id_t) == sizeof(spn_pkg_id_t) + sizeof(spn_build_id_t),
  "spn_pkg_unit_id_t is byte-hashed as a key; it must have no padding"
);
_Static_assert(
  sizeof(spn_target_unit_id_t) == sizeof(spn_pkg_unit_id_t) + sizeof(sp_intern_id_t) + sizeof(spn_target_kind_t),
  "spn_target_unit_id_t is byte-hashed as a key; it must have no padding"
);
_Static_assert(
  sizeof(spn_compile_unit_id_t) == sizeof(spn_target_unit_id_t) + sizeof(sp_intern_id_t),
  "spn_compile_unit_id_t is byte-hashed as a key; it must have no padding"
);

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

typedef struct {
  spn_pkg_unit_t* pkg;
  u32 index;
} spn_node_ref_t;

struct spn_user_node_t {
  spn_pkg_unit_t* pkg;
  sp_str_t tag;
  sp_str_t fn;
  sp_da(spn_path_t) inputs;
  sp_da(spn_path_t) outputs;
  sp_da(spn_node_ref_t) deps;
};

typedef struct {
  spn_compile_unit_id_t id;
  spn_target_unit_t* target;
  spn_lang_t lang;
  spn_invocation_t invocation;

  struct {
    spn_path_t file;
    spn_path_t object;
  } paths;
} spn_compile_unit_t;

typedef struct {
  spn_cc_link_t cc;
  sp_da(spn_path_t) archives;
  sp_da(spn_link_lib_t) libs;
} spn_link_plan_t;

struct spn_target_unit {
  spn_target_unit_id_t id;
  spn_pkg_unit_t* pkg;
  spn_target_info_t* info;
  spn_cc_output_kind_t kind;
  spn_linkage_t lib_kind;

  sp_da(spn_compile_unit_t*) objects;
  sp_da(spn_target_unit_t*) deps;

  spn_link_plan_t link;
};

struct spn_pkg_unit_t {
  spn_pkg_unit_id_t id;
  spn_build_unit_t* build;
  spn_session_t* session;
  spn_pkg_info_t* info;
  spn_pkg_source_t source;
  u32 kinds;

  // The unit whose scripts are this package's: its unit in the metaprogram
  // build (itself, there), or null when the package has none
  spn_pkg_unit_t* metaprogram;
  struct {
    spn_target_unit_t* configure;
    spn_target_unit_t* build;
  } scripts;

  sp_da(spn_pkg_dep_t) deps;
  sp_da(spn_target_unit_t*) libs;
  sp_da(spn_target_unit_t*) targets;
  sp_da(spn_user_node_t) user_nodes;

  struct {
    struct {
      spn_path_t dir;
      spn_path_t configure;
      spn_path_t package;
    } stamp;

    spn_tree_roots_t roots;
    spn_path_t work;
    spn_path_t object;
    spn_path_t store;
    spn_path_t include;
    spn_path_t lib;
    spn_path_t bin;
    spn_path_t vendor;
  } paths;

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
    spn_wasm_script_t* active;
  } wasm;

  sp_atomic_s32_t compile_announced;
};

struct spn_toolchain_unit_t {
  spn_toolchain_info_t* info;
  spn_triple_t host;
  spn_opt_artifact_t artifact;
  sp_str_t root;
  spn_cc_toolchain_t cc;
  sp_hash_t identity;
  sp_str_t version;
};

static inline spn_user_node_t* spn_node_deref(spn_node_ref_t ref) {
  SP_ASSERT(ref.index < sp_da_size(ref.pkg->user_nodes));
  return &ref.pkg->user_nodes[ref.index];
}

#endif
