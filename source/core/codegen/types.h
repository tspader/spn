#ifndef SPN_CODEGEN_TYPES_H
#define SPN_CODEGEN_TYPES_H

#include "sp.h"
#include "spn/core.h"
#include "intern/types.h"

#define SPN_CODEGEN_PATH_MAX 32

typedef struct spn_cg_manifest spn_cg_manifest_t;
typedef struct spn_cg_config spn_cg_config_t;

typedef enum {
  SPN_CODEGEN_PATH_KEY,
  SPN_CODEGEN_PATH_INDEX,
} spn_codegen_path_kind_t;

typedef struct {
  spn_codegen_path_kind_t kind;
  const c8* key;
  u32 index;
} spn_codegen_path_seg_t;

typedef struct spn_codegen_issue {
  spn_err_t code;
  sp_str_t path;
  sp_str_t detail;
  sp_str_t value;
  sp_str_t scope;
  sp_da(sp_str_t) choices;
  spn_codegen_path_seg_t segs [SPN_CODEGEN_PATH_MAX];
  u32 depth;
} spn_codegen_issue_t;

typedef sp_da(spn_codegen_issue_t) spn_codegen_issues_t;

typedef struct {
  sp_str_t name;
  u32 depth;
} spn_codegen_scope_t;

typedef struct {
  sp_mem_t mem;
  sp_intern_t* intern;
  sp_str_t dir;
  bool strict;
  spn_codegen_path_seg_t path [SPN_CODEGEN_PATH_MAX];
  u32 depth;
  spn_codegen_scope_t scope;
  sp_da(spn_codegen_issue_t) issues;
} spn_toml_loader_t;


#endif
