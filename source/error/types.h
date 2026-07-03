#ifndef SPN_ERR_H
#define SPN_ERR_H

#include "sp.h"
#include "spn.h"

#include "forward/types.h"

#define spn_try(expr) do { \
  s32 _sp_result = (expr); \
  if (_sp_result) return _sp_result; \
} while (0)

#define spn_try_goto(expr, err, label) \
  do { \
    err = (expr); \
    if (err) goto label; \
  } while (0)

#define spn_try_as(expr, err) do { \
  if (expr) return err; \
} while (0)

#define spn_try_map(_expr, _err, _error_expr) do { \
  spn_err_t _err = (_expr); \
  if (_err) return _error_expr; \
} while (0)

#define spn_try_union(expr) do { \
  spn_err_union_t _spn_result = (expr); \
  if (_spn_result.kind) return _spn_result; \
} while (0)

#define spn_try_as_union(expr) do { \
  spn_err_t _spn_result = (expr); \
  if (_spn_result) return (spn_err_union_t) { .kind = _spn_result }; \
} while (0)

#define spn_try_map_union(_expr, _err, _error_expr) do { \
  spn_err_union_t _err = (_expr); \
  if (_err.kind) return _error_expr; \
} while (0)

#define spn_result(status) (spn_err_union_t) { .kind = (status) }

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
  spn_err_t kind;
  union {
    struct {
      sp_str_t path;
    } manifest_parse;
    struct {
      sp_str_t path;
      sp_str_t expected;
      sp_str_t actual;
    } manifest_field;
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
    spn_err_wasm_t wasm;
    spn_err_build_graph_t build_graph;
    spn_err_toolchain_t toolchain;
    spn_err_artifact_t artifact;
    sp_da(spn_codegen_issue_t) issues;
  };
} spn_err_union_t;

#endif
