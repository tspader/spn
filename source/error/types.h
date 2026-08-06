#ifndef SPN_ERR_H
#define SPN_ERR_H

#include "sp.h"
#include "spn.h"

#include "compiler/types.h"
#include "forward/types.h"
#include "pkg/types.h"
#include "semver/types.h"

#define spn_try(expr) \
  do { \
    spn_err_t __err = (expr); \
    if (__err) { \
      return __err; \
    } \
  } while (0)

#define spn_try_union(expr) \
  do { \
    spn_err_union_t __err = (expr); \
    if (__err.kind) { \
      return __err; \
    } \
  } while (0)

#define spn_result(kind_) ((spn_err_union_t) { .kind = (kind_) })

#define spn_err_reported(kind_) ((spn_err_union_t) { .kind = (kind_), .reported = true })

typedef enum {
  SPN_BUILD_GRAPH_ERR_UNKNOWN,
  SPN_BUILD_GRAPH_ERR_MISSING_INPUT,
  SPN_BUILD_GRAPH_ERR_DUPLICATE_OUTPUT,
} spn_build_graph_err_kind_t;

typedef struct {
  spn_build_graph_err_kind_t kind;
  sp_str_t file;
  sp_str_t command_a;
  sp_str_t command_b;
} spn_err_build_graph_t;

typedef enum {
  SPN_TOOLCHAIN_ROLE_BUILD,
  SPN_TOOLCHAIN_ROLE_SCRIPT,
} spn_toolchain_role_t;

typedef struct {
  spn_toolchain_role_t role;
  sp_str_t name;
  spn_triple_t target;
  spn_triple_t host;
  spn_toolchain_catalog_t* catalog;
} spn_err_toolchain_t;

typedef struct {
  sp_str_t name;
  sp_str_t url;
  sp_str_t expected;
  sp_str_t actual;
} spn_err_artifact_t;

typedef struct {
  sp_str_t path;
  sp_str_t error;
  s32 rc;
} spn_err_wasm_t;

typedef struct spn_codegen_issue spn_codegen_issue_t;

typedef struct {
  sp_str_t name;
  sp_str_t path;
  sp_da(spn_codegen_issue_t) issues;
} spn_err_manifest_t;

typedef struct {
  spn_pkg_name_t id;
} spn_err_circular_t;

typedef struct {
  spn_requested_dep_t request;
} spn_err_unknown_t;

typedef struct {
  spn_requested_dep_t request;
  sp_str_t requester;
  spn_semver_t requester_version;
  bool conflict;
  spn_semver_t selected;
} spn_err_unsatisfiable_t;

typedef struct {
  spn_pkg_name_t id;
  spn_semver_t version;
} spn_err_unit_cycle_t;

typedef struct {
  spn_pkg_name_t id;
  spn_semver_t low;
  spn_semver_t high;
} spn_err_dynamic_dup_t;

typedef struct {
  spn_pkg_name_t id;
} spn_err_too_complex_t;

typedef struct {
  sp_str_t path;
  sp_str_t declared;
  sp_str_t requested;
} spn_err_mismatch_t;

typedef struct {
  spn_err_t kind;
  bool reported;
  union {
    struct {
      sp_str_t path;
    } manifest_parse;
    spn_err_manifest_t manifest;
    spn_err_circular_t circular;
    spn_err_unknown_t unknown;
    spn_err_unsatisfiable_t unsatisfiable;
    spn_err_unit_cycle_t unit_cycle;
    spn_err_dynamic_dup_t dynamic_dup;
    spn_err_too_complex_t too_complex;
    spn_err_mismatch_t mismatch;
    struct {
      sp_str_t name;
      sp_str_t path;
    } index_corrupt;
    struct {
      sp_str_t path;
    } no_manifest;
    struct {
      sp_str_t path;
    } not_git_repo;
    struct {
      sp_str_t command;
    } git;
    struct {
      sp_str_t name;
      sp_str_t version;
    } version_exists;
    struct {
      sp_str_t name;
    } profile;
    struct {
      sp_str_t toolchain;
      spn_triple_t target;
      spn_sanitizer_set_t unsupported;
    } sanitizer;
    struct {
      sp_str_t toolchain;
      spn_triple_t target;
      spn_cc_feature_t feature;
    } compiler;
    struct {
      sp_str_t path;
    } fs;
    struct {
      sp_str_t name;
      sp_str_t url;
    } index;
    struct {
      sp_str_t url;
      sp_str_t rev;
      sp_str_t path;
      sp_str_t output;
    } publish;
    struct {
      sp_str_t name;
      sp_str_t requested;
    } pkg;
    struct {
      sp_str_t name;
      sp_str_t source;
    } configure_source;
    struct {
      sp_str_t pkg;
      sp_str_t name;
      sp_str_t requested;
      sp_str_t requester;
    } target;
    struct {
      sp_str_t name;
      sp_str_t path;
      s32 code;
    } script;
    struct {
      sp_str_t path;
    } dag;
    spn_err_wasm_t wasm;
    spn_err_build_graph_t build_graph;
    spn_err_toolchain_t toolchain;
    spn_err_artifact_t artifact;
  };
} spn_err_union_t;

#endif
